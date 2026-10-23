#!/usr/bin/env python3
"""Mean RGB of a linear BC1 mip0 file (numeric only); used on replay-bound texture dumps."""
from __future__ import annotations
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from x360_bc_decode import decode_bc1_block  # noqa: E402

data = Path(sys.argv[1]).read_bytes()
sums = [0, 0, 0]
count = 0
for off in range(0, len(data), 8):
    for px in decode_bc1_block(data[off:off + 8]):
        for c in range(3):
            sums[c] += px[c]
        count += 1
print("mean_rgb=" + ",".join(f"{s / count:.2f}" for s in sums), "texels=" + str(count))
