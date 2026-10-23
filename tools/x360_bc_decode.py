#!/usr/bin/env python3
"""Decode Xbox 360 tiled BC1/BC3 mip0 payloads and report numeric statistics only (no images).

Tiling: GetTiledOffset2D from tools/rexglue-sdk/src/graphics/pipeline/texture/util.cpp, with block
coordinates and bytes_per_block_log2 (3 for BC1, 4 for BC3). Byte order: 16-bit words swapped
(k8in16), selectable. Output is numeric only: roughness (mean absolute difference between
horizontal and vertical neighbours) and channel means over the decoded RGBA.
"""

from __future__ import annotations

import struct
import sys
from pathlib import Path


def tiled_offset_2d(x: int, y: int, pitch: int, bpb_log2: int) -> int:
    """Port of texture_util::GetTiledOffset2D (UModel UnTexture.cpp derivation)."""
    pitch = (pitch + 31) & ~31
    macro = ((x >> 5) + (y >> 5) * (pitch >> 5)) << (bpb_log2 + 7)
    micro = ((x & 7) + ((y & 0xE) << 2)) << bpb_log2
    offset = macro + ((micro & ~0xF) << 1) + (micro & 0xF) + ((y & 1) << 4)
    return (((offset & ~0x1FF) << 3) + ((y & 16) << 7) + ((offset & 0x1C0) << 2)
            + ((((y & 8) >> 2) + (x >> 3)) & 3) << 6) + (offset & 0x3F)


def swap16(data: bytes) -> bytes:
    out = bytearray(len(data))
    out[0::2] = data[1::2]
    out[1::2] = data[0::2]
    return bytes(out)


def rgb565(c: int) -> tuple[int, int, int]:
    r, g, b = (c >> 11) & 31, (c >> 5) & 63, c & 31
    return (r * 255 // 31, g * 255 // 63, b * 255 // 31)


def decode_bc1_block(block: bytes) -> list[tuple[int, int, int, int]]:
    c0, c1 = struct.unpack_from("<HH", block, 0)
    p0, p1 = rgb565(c0), rgb565(c1)
    if c0 > c1:
        p2 = tuple((2 * a + b) // 3 for a, b in zip(p0, p1))
        p3 = tuple((a + 2 * b) // 3 for a, b in zip(p0, p1))
        pal = [p0 + (255,), p1 + (255,), p2 + (255,), p3 + (255,)]
    else:
        p2 = tuple((a + b) // 2 for a, b in zip(p0, p1))
        pal = [p0 + (255,), p1 + (255,), p2 + (255,), (0, 0, 0, 0)]
    bits = struct.unpack_from("<I", block, 4)[0]
    return [pal[(bits >> (2 * i)) & 3] for i in range(16)]


def decode_bc3_alpha(block: bytes) -> list[int]:
    a0, a1 = block[0], block[1]
    alphas = [a0, a1]
    if a0 > a1:
        alphas += [((6 - i) * a0 + i * a1) // 7 for i in range(1, 7)]
    else:
        alphas += [((5 - i) * a0 + i * a1) // 5 for i in range(1, 5)] + [0, 255]
    bits = int.from_bytes(block[2:8], "little")
    return [alphas[(bits >> (3 * i)) & 7] for i in range(16)]


def decode_mip0(payload: bytes, width: int, height: int, fmt: str, swap: bool, tiled: str) -> list[list[tuple]]:
    bpb = 8 if fmt == "bc1" else 16
    bpb_log2 = 3 if fmt == "bc1" else 4
    bw, bh = width // 4, height // 4
    pitch_blocks = bw
    image = [[None] * width for _ in range(height)]
    for by in range(bh):
        for bx in range(bw):
            if tiled == "linear":
                off = (by * bw + bx) * bpb
            elif tiled == "blocks":
                off = tiled_offset_2d(bx, by, pitch_blocks, bpb_log2)
            else:  # "texels": texel coordinates of the block origin, byte offset divided by block size
                off = tiled_offset_2d(bx * 4, by * 4, width, bpb_log2) // bpb * bpb
            block = payload[off:off + bpb].ljust(bpb, b"\0")  # out-of-range variants decode as zeros
            if swap:
                block = swap16(block)
            if fmt == "bc1":
                pix = decode_bc1_block(block)
            else:
                alpha = decode_bc3_alpha(block[:8])
                color = decode_bc1_block(block[8:16])
                pix = [c[:3] + (a,) for c, a in zip(color, alpha)]
            for py in range(4):
                for px in range(4):
                    image[by * 4 + py][bx * 4 + px] = pix[py * 4 + px]
    return image


def stats(image: list[list[tuple]]) -> dict:
    h, w = len(image), len(image[0])
    total = 0.0
    count = 0
    for y in range(h):
        for x in range(w):
            p = image[y][x]
            if x + 1 < w:
                total += sum(abs(a - b) for a, b in zip(p[:3], image[y][x + 1][:3]))
                count += 3
            if y + 1 < h:
                total += sum(abs(a - b) for a, b in zip(p[:3], image[y + 1][x][:3]))
                count += 3
    means = [sum(image[y][x][c] for y in range(h) for x in range(w)) / (w * h) for c in range(3)]
    return {"roughness": round(total / count, 3), "mean_rgb": [round(m, 2) for m in means]}


def main() -> int:
    if len(sys.argv) != 5:
        print("Usage: x360_bc_decode.py PAYLOAD WIDTH HEIGHT bc1|bc3", file=sys.stderr)
        return 2
    payload = Path(sys.argv[1]).read_bytes()
    width, height, fmt = int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
    for tiled in ("linear", "blocks", "texels"):
        for swap in (False, True):
            s = stats(decode_mip0(payload, width, height, fmt, swap, tiled))
            print(f"layout={tiled}\tswap16={swap}\t{s['roughness']}\t{s['mean_rgb']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
