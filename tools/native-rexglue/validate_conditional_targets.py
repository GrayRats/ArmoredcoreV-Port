#!/usr/bin/env python3
"""Validate ReXGlue continuation-fragment hints against original ACV PPC bytes."""
import hashlib
import json
import re
import subprocess
import sys
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from xex_read_guest_range import extract_image

XEX = ROOT / "games/acv/extracted/default.xex"
MANIFEST = ROOT / "tmp/acv-rexglue-linux-indexed/acv_linux_probe_manifest.toml"
INVENTORY = ROOT / "logs/acv-rexglue-unresolved-20261021/conditionals/inventory.json"
CONFIG = ROOT / "tools/native-rexglue/acv-conditional-targets.toml"
EVIDENCE = ROOT / "logs/acv-rexglue-unresolved-20261021/conditionals/config-validation.json"
EXPECTED_XEX_SHA256 = "4fac87348f827e84633013375012284cd8c769a3c155a675b2989757e666a12b"

raw = XEX.read_bytes()
assert hashlib.sha256(raw).hexdigest() == EXPECTED_XEX_SHA256
image, image_base, _, _ = extract_image(raw)
inventory = json.loads(INVENTORY.read_text())
functions = tomllib.loads(CONFIG.read_text())["functions"]
assert set(int(x, 16) for x in functions) == {int(x["target"], 16) for x in inventory["groups"]}

def word(addr):
    off = addr - image_base
    assert 0 <= off <= len(image) - 4, f"outside image: {addr:#x}"
    return int.from_bytes(image[off:off + 4], "big")

targets = []
for group in inventory["groups"]:
    addr = int(group["target"], 16)
    cfg = functions[f"0x{addr:x}"]
    assert addr + cfg["size"] <= image_base + len(image)
    # Every candidate is a complete short exit path. 18 return zero; the
    # remaining block returns the absolute pointer encoded by lis/addi.
    words = [word(addr + 4 * i) for i in range(cfg["size"] // 4)]
    assert words[-1] == 0x4E800020, f"candidate does not end in blr: {addr:#x}"
    assert len(words) in (2, 3), f"unexpected fragment length: {addr:#x}"
    if len(words) == 2:
        assert words[0] == 0x38600000, f"unexpected 2-instruction return: {addr:#x}"
    else:
        assert words[0] >> 26 == 15 and words[1] >> 26 == 14
    disasm = subprocess.check_output(
        [sys.executable, str(ROOT / "tools/ppc_dis.py"), str(ROOT / "tmp/scan/text.bin"),
         hex(addr), hex(addr + cfg["size"])], text=True)
    assert ".long" not in disasm
    targets.append({"address": hex(addr), "size": cfg["size"],
                    "words": [f"0x{x:08x}" for x in words],
                    "branches": group["sites"]})

manifest = tomllib.loads(MANIFEST.read_text())
includes = manifest["entrypoint"]["includes"]
assert str(ROOT / "tools/native-rexglue/acv-boundaries.toml") in includes
assert str(CONFIG) in includes
EVIDENCE.parent.mkdir(parents=True, exist_ok=True)
EVIDENCE.write_text(json.dumps({
    "confidence": "instruction-verified short branch-target fragments; function ownership and game behavior remain unverified",
    "xex_sha256": EXPECTED_XEX_SHA256,
    "inventory_sha256": hashlib.sha256(INVENTORY.read_bytes()).hexdigest(),
    "config_sha256": hashlib.sha256(CONFIG.read_bytes()).hexdigest(),
    "targets": targets,
}, indent=2) + "\n")
print(json.dumps({"targets": len(targets), "branches": sum(len(x["branches"]) for x in targets),
                  "evidence": str(EVIDENCE.relative_to(ROOT))}))
