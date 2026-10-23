#!/usr/bin/env python3
"""Read a guest VA range from a retail XEX using XenonRecomp's XEX layout.

Supports normal retail AES encryption and XEX basic compression. Reads the
input XEX, writes only the requested byte range to an explicitly selected path.
Normal/LZX compression is deliberately rejected.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

RETAIL_KEY = bytes.fromhex("20b185a59d28fdc340583fbb0896bf91")


def aes_cbc_decrypt(data: bytes, key: bytes) -> bytes:
    if len(data) % 16:
        raise ValueError(f"AES input length {len(data)} is not block aligned")
    result = subprocess.run(
        ["openssl", "enc", "-d", "-aes-128-cbc", "-K", key.hex(),
         "-iv", "00" * 16, "-nopad"],
        input=data, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
    if result.returncode:
        raise RuntimeError(result.stderr.decode(errors="replace"))
    return result.stdout


def extract_image(xex: bytes):
    if len(xex) < 24 or xex[:4] != b"XEX2":
        raise ValueError("input is not an XEX2 image")
    _, _, header_size, _, security_offset, header_count = struct.unpack_from(">6I", xex)
    if header_size > len(xex) or security_offset + 0x160 > len(xex):
        raise ValueError("XEX header/security range is outside input")
    options = {}
    for i in range(header_count):
        key, value = struct.unpack_from(">II", xex, 24 + i * 8)
        options[key] = value
    fmt_offset = options.get(0x3FF)
    if fmt_offset is None or fmt_offset + 8 > len(xex):
        raise ValueError("XEX file-format optional header is missing/out of range")
    info_size, encryption, compression = struct.unpack_from(">IHH", xex, fmt_offset)
    image_size = struct.unpack_from(">I", xex, security_offset + 4)[0]
    if encryption == 1:
        title_key = xex[security_offset + 0x150:security_offset + 0x160]
        key = aes_cbc_decrypt(title_key, RETAIL_KEY)
        payload = aes_cbc_decrypt(xex[header_size:], key)
    elif encryption == 0:
        payload = xex[header_size:]
    else:
        raise ValueError(f"unsupported XEX encryption type {encryption}")

    if compression == 0:
        image = payload[:image_size]
    elif compression == 1:
        if info_size < 16 or info_size % 8:
            raise ValueError(f"invalid basic-compression info size {info_size}")
        block_count = info_size // 8 - 1
        blocks = [struct.unpack_from(">II", xex, fmt_offset + 8 + i * 8)
                  for i in range(block_count)]
        image_out = bytearray()
        source_offset = 0
        for data_size, zero_size in blocks:
            if source_offset + data_size > len(payload):
                raise ValueError("basic-compression data block exceeds decrypted payload")
            image_out.extend(payload[source_offset:source_offset + data_size])
            image_out.extend(bytes(zero_size))
            source_offset += data_size
        image = bytes(image_out)
        if len(image) != image_size:
            raise ValueError(f"decompressed image size {len(image):#x} != security size {image_size:#x}")
    else:
        raise ValueError(f"unsupported XEX compression type {compression}; no LZX fallback")

    if len(image) < 0x40 or image[:2] != b"MZ":
        raise ValueError("decompressed image does not begin with a PE DOS header")
    pe_offset = struct.unpack_from("<I", image, 0x3C)[0]
    if pe_offset + 24 > len(image) or image[pe_offset:pe_offset + 4] != b"PE\0\0":
        raise ValueError("decompressed image has an invalid PE header")
    number_sections = struct.unpack_from("<H", image, pe_offset + 6)[0]
    optional_size = struct.unpack_from("<H", image, pe_offset + 20)[0]
    section_table = pe_offset + 24 + optional_size
    image_base = options.get(0x10201)
    if image_base is None:
        image_base = struct.unpack_from(">I", xex, security_offset + 0x110)[0]
    sections = []
    for i in range(number_sections):
        off = section_table + i * 40
        if off + 40 > len(image):
            raise ValueError("PE section table exceeds decompressed image")
        name, virtual_size, rva, _, _, _, _, _, _, characteristics = struct.unpack_from(
            "<8sIIIIIIHHI", image, off)
        sections.append({"name": name.rstrip(b"\0").decode("ascii", errors="replace"),
                         "rva": rva, "virtual_size": virtual_size,
                         "characteristics": characteristics})
    return image, image_base, sections, {
        "header_size": header_size, "security_offset": security_offset,
        "image_size": image_size, "encryption_type": encryption,
        "compression_type": compression, "compression_block_count": block_count if compression == 1 else None,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("xex", type=Path)
    parser.add_argument("guest_address", type=lambda s: int(s, 0))
    parser.add_argument("length", type=lambda s: int(s, 0))
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    if args.length <= 0 or args.length > 16 * 1024 * 1024:
        parser.error("length must be between 1 and 16 MiB")
    xex = args.xex.read_bytes()
    image, image_base, sections, metadata = extract_image(xex)
    hits = []
    for section in sections:
        start = image_base + section["rva"]
        end = start + section["virtual_size"]
        if start <= args.guest_address and args.guest_address + args.length <= end:
            hits.append(section)
    if len(hits) != 1:
        raise SystemExit(f"requested range belongs to {len(hits)} mapped PE sections; refusing ambiguous/unmapped read")
    rva = args.guest_address - image_base
    data = image[rva:rva + args.length]
    if len(data) != args.length:
        raise SystemExit("requested bytes lie outside the mapped image")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(data)
    print(json.dumps({
        "xex": str(args.xex), "xex_sha256": hashlib.sha256(xex).hexdigest(),
        "guest_address": hex(args.guest_address), "length": len(data),
        "image_base": hex(image_base), "section": hits[0],
        "range_sha256": hashlib.sha256(data).hexdigest(),
        "metadata": metadata, "output": str(args.output), "hex": data.hex(),
    }, indent=2))


if __name__ == "__main__":
    main()
