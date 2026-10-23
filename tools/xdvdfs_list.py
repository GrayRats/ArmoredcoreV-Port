#!/usr/bin/env python3
"""Read-only XDVDFS directory listing for an Xbox 360 disc image.

The image is opened read-only and never written. Output is one line per entry:
start sector, size (or '-' for a directory) and path. With --verify-root DIR,
every listed file is read from the image and compared by size and SHA-256 with
the same relative path under DIR.

Layout (XDVDFS): the volume descriptor is sector 32 of the game partition and
holds the magic at 0x000 and again at 0x7EC, the root directory sector at 0x14
and its size at 0x18 (little-endian). Sector numbers are partition-relative.
Directory entries: u16 left, u16 right (subtree offsets in 4-byte units),
u32 start sector, u32 size, u8 attributes (0x10 = directory), u8 name length,
name; entries are 4-byte aligned, do not cross 2048-byte sectors, and unused
space is filled with 0xFF.
"""
import argparse
import hashlib
import struct
import sys
from pathlib import Path

MAGIC = b"MICROSOFT*XBOX*MEDIA"
SECTOR = 2048
# Descriptor offsets = partition start + 32 sectors (0x10000).
DESCRIPTOR_OFFSETS = {
    0xFDA0000: "XGD2 (partition 0xFD90000; verified on the ACV image)",
    0x2090000: "XGD3 (partition 0x2080000)",
    0x18310000: "XGD1 (partition 0x18300000)",
    0x10000: "plain XISO (partition 0)",
}


def find_descriptor(handle):
    for offset, kind in DESCRIPTOR_OFFSETS.items():
        handle.seek(offset)
        block = handle.read(SECTOR)
        if block[:len(MAGIC)] == MAGIC and block[0x7EC:0x7EC + len(MAGIC)] == MAGIC:
            return offset, kind, block
    raise SystemExit("no XDVDFS volume descriptor (both magics) at known offsets")


class Volume:
    def __init__(self, handle):
        self.file = handle
        self.descriptor, self.kind, block = find_descriptor(handle)
        self.base = self.descriptor - 32 * SECTOR
        self.root_sector, self.root_size = struct.unpack_from("<II", block, 0x14)

    def read(self, sector, size):
        self.file.seek(self.base + sector * SECTOR)
        data = self.file.read(size)
        if len(data) != size:
            raise ValueError(f"short read at sector {sector}: {len(data)} of {size} bytes")
        return data

    def entries(self, sector, size):
        """In-order (sorted) traversal of one directory's entry tree."""
        if size == 0:
            return
        table = self.read(sector, size)
        seen = set()

        def visit(offset):
            if offset in seen:
                raise ValueError(f"cycle in directory tree at sector {sector} offset {offset}")
            seen.add(offset)
            if offset + 14 > len(table):
                raise ValueError(f"entry header outside table at sector {sector} offset {offset}")
            left, right, start, length, attrs, name_len = struct.unpack_from("<HHIIBB", table, offset)
            if left == 0xFFFF and right == 0xFFFF:
                return  # 0xFF padding, not an entry
            end = offset + 14 + name_len
            if name_len == 0 or end > len(table) or offset // SECTOR != (end - 1) // SECTOR:
                raise ValueError(f"bad entry bounds at sector {sector} offset {offset}")
            name = table[offset + 14:end].decode("latin-1")
            if left:
                yield from visit(left * 4)
            yield name, start, length, bool(attrs & 0x10)
            if right:
                yield from visit(right * 4)

        yield from visit(0)

    def walk(self, sector=None, size=None, prefix=""):
        if sector is None:
            sector, size = self.root_sector, self.root_size
        for name, start, length, is_dir in self.entries(sector, size):
            path = prefix + name
            if is_dir:
                yield path + "/", start, length, True
                yield from self.walk(start, length, path + "/")
            else:
                yield path, start, length, False

    def sha256(self, sector, size):
        digest = hashlib.sha256()
        self.file.seek(self.base + sector * SECTOR)
        remaining = size
        while remaining:
            chunk = self.file.read(min(remaining, 8 << 20))
            if not chunk:
                raise ValueError(f"short read in file at sector {sector}")
            digest.update(chunk)
            remaining -= len(chunk)
        return digest.hexdigest()


def file_sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        while chunk := handle.read(8 << 20):
            digest.update(chunk)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("image", type=Path)
    parser.add_argument("--filter", default="", help="case-insensitive substring")
    parser.add_argument("--verify-root", type=Path, help="compare every file with this extracted tree")
    args = parser.parse_args()
    with args.image.open("rb") as handle:
        vol = Volume(handle)
        needle = args.filter.lower()
        files = dirs = 0
        mismatches = []
        for path, start, length, is_dir in vol.walk():
            if needle and needle not in path.lower():
                continue
            if is_dir:
                dirs += 1
            else:
                files += 1
            print(f"{start:10d} {'-' if is_dir else length:>12} {path}")
            if args.verify_root and not is_dir:
                local = args.verify_root / path
                if not local.is_file():
                    mismatches.append(f"missing locally: {path}")
                elif local.stat().st_size != length:
                    mismatches.append(f"size differs: {path} image={length} local={local.stat().st_size}")
                elif vol.sha256(start, length) != file_sha256(local):
                    mismatches.append(f"sha256 differs: {path}")
        print(f"# volume={vol.kind} descriptor={vol.descriptor:#x} root_sector={vol.root_sector} "
              f"root_size={vol.root_size} dirs={dirs} files={files}", file=sys.stderr)
        if args.verify_root:
            local_files = {str(p.relative_to(args.verify_root)) for p in args.verify_root.rglob("*") if p.is_file()}
            image_files = {p for p, _, _, d in vol.walk() if not d}
            extra = sorted(local_files - image_files)
            for item in mismatches:
                print("# " + item, file=sys.stderr)
            for item in extra:
                print(f"# not in image: {item}", file=sys.stderr)
            print(f"# verify: {files} files, {len(mismatches)} mismatches, {len(extra)} local-only",
                  file=sys.stderr)
            if mismatches or extra:
                raise SystemExit(1)


if __name__ == "__main__":
    main()
