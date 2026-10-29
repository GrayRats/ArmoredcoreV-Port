#!/usr/bin/env python3
"""Check real in-workspace packaging and rejection of a development-library leak."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument("package", type=Path)
args = ap.parse_args()
root = Path(__file__).resolve().parents[2]
verifier = root / "tools/acv-trg-launcher/verify_package.py"
package = args.package.resolve(strict=True)
assert package.is_relative_to(root), "Fixture must exercise the in-workspace package boundary"
positive = subprocess.run([sys.executable, str(verifier), str(package)], capture_output=True, text=True)
assert positive.returncode == 0, positive.stdout + positive.stderr
with tempfile.TemporaryDirectory(prefix="acv-ldd-fixture-") as temp:
    fake = Path(temp) / "ldd"
    outside = root / "tmp/not-the-package/libfixture.so"
    fake.write_text(f"#!{sys.executable}\nprint({('libfixture.so => ' + str(outside) + ' (0x1)')!r})\n")
    fake.chmod(0o755)
    negative = subprocess.run([sys.executable, str(verifier), str(package)], capture_output=True, text=True,
                              env=dict(os.environ, PATH=temp + os.pathsep + os.environ["PATH"]))
    assert negative.returncode != 0 and "Development dependency outside package" in negative.stderr, negative
print("PASS: actual package inside repository accepted; external development-library resolution rejected")
