#!/usr/bin/env python3
"""Find the Xenos 2D tiling of a BC1 texture in guest memory by exact block matching.

Inputs: linear BC1 mip0 blocks from RenderDoc (replay_texture_dump) and a guest-memory dump.
Guest blocks are assumed to be the linear block with 16-bit words swapped (k8in16), stored at
base + T(bx, by). For each candidate tiling T, candidate bases come from the positions of a few
sample blocks; the score is the fraction of blocks matched exactly at base + T(k).
"""
from __future__ import annotations
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from x360_bc_decode import tiled_offset_2d  # noqa: E402


def swap16(b: bytes) -> bytes:
    out = bytearray(len(b))
    out[0::2] = b[1::2]
    out[1::2] = b[0::2]
    return bytes(out)


def tile_crunch_row(y: int, width: int, log2: int) -> int:
    macro = ((y // 32) * (width // 32)) << (log2 + 7)
    micro = ((y & 6) << 2) << log2
    return macro + ((micro & ~0xF) << 1) + (micro & 0xF) + ((y & 8) << (3 + log2)) + ((y & 1) << 4)


def tile_crunch_col(x: int, y: int, log2: int, base: int) -> int:
    macro = (x // 32) << (log2 + 7)
    micro = (x & 7) << log2
    offset = base + (macro + ((micro & ~0xF) << 1) + (micro & 0xF))
    return ((offset & ~0x1FF) << 3) + ((offset & 0x1C0) << 2) + (offset & 0x3F) + ((y & 16) << 7) + \
        (((((y & 8) >> 2) + (x >> 3)) & 3) << 6)


def hypotheses(bw: int, W: int):
    def sdk_blocks(bx, by):
        return tiled_offset_2d(bx, by, bw, 3)

    def sdk_texels(bx, by):
        return tiled_offset_2d(bx * 4, by * 4, W, 3)

    def crunch_blocks(bx, by):
        row = tile_crunch_row(by, bw, 3)
        return tile_crunch_col(bx, by, 3, row)

    def crunch_texels(bx, by):
        row = tile_crunch_row(by * 4, W, 3)
        return tile_crunch_col(bx * 4, by * 4, 3, row)

    return {"sdk_blocks": sdk_blocks, "sdk_texels": sdk_texels,
            "crunch_blocks": crunch_blocks, "crunch_texels": crunch_texels}


def main() -> int:
    linear = Path(sys.argv[1]).read_bytes()
    guest = Path(sys.argv[2]).read_bytes()
    W = int(sys.argv[3])
    bw = W // 4
    nb = bw * bw
    samples = [0, 1, bw, bw + 1, nb // 2, nb - 1, 77, 1234]
    results = []
    for name, T in hypotheses(bw, W).items():
        # candidate bases: occurrences of swapped sample blocks, minus their tiled offsets
        votes = Counter()
        for k in samples:
            block = swap16(linear[k * 8:k * 8 + 8])
            start = 0
            hits = 0
            while hits < 400:
                pos = guest.find(block, start)
                if pos < 0:
                    break
                votes[pos - T(k % bw, k // bw)] += 1
                start = pos + 1
                hits += 1
        best_base, _ = votes.most_common(1)[0] if votes else (None, 0)
        if best_base is None:
            results.append((name, None, 0.0))
            continue
        matched = 0
        for k in range(nb):
            o = best_base + T(k % bw, k // bw)
            if 0 <= o and o + 8 <= len(guest) and guest[o:o + 8] == swap16(linear[k * 8:k * 8 + 8]):
                matched += 1
        results.append((name, best_base, matched / nb))
    for name, base, frac in results:
        print(f"{name}\tbase={base}\tmatched={frac:.4f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
