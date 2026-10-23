#!/usr/bin/env python3
"""Assemble the Linux test package (PLAN_GUI.MD section 9) under tmp/acv-package/.

Includes launcher, runtime + GPU plugin from the recorded test build, Python tooling and config.
Game data (tmp/iso_root), personal saves, caches and logs are never copied. Hashes of every file go
to manifest.json together with the unit-test results run at packaging time."""
import argparse, datetime, hashlib, json, shutil, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
NATIVE = ROOT / "tools/native-rexglue"
BUILD = ROOT / "tmp/acv-rexglue-linux-indexed/out/build/linux-release"
TOOL_FILES = ["RUNTIME_BASELINE.json", "probe_linux.py", "acv_settings.py", "acv_profiles.py", "acv_profile_cli.py",
              "acv.config", "input-profile.json"]

ap = argparse.ArgumentParser()
ap.add_argument("--launcher", type=Path, default=ROOT / "tmp/acv-launcher-build/acv-launcher")
ap.add_argument("--out", type=Path, default=ROOT / "tmp/acv-package")
args = ap.parse_args()

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

# Runtime artifacts: honour the explicit overrides recorded by the builder (accepted GPU fix).
record = json.loads((BUILD / "acv-test-build.json").read_text())
sources = {}
for name in ("acv_linux_probe", "librexruntime.so", "librexgpu-xenos.so"):
    override = record.get({"librexruntime.so": "runtime_override", "librexgpu-xenos.so": "gpu_plugin_override"}.get(name, ""), {})
    path = Path(override.get("path", record["artifacts"][name]["path"]))
    if sha(path) != override.get("sha256", record["artifacts"][name]["sha256"]):
        sys.exit(f"{path}: hash differs from build record")
    sources[name] = path

# The recorded baseline (RUNTIME_BASELINE.json) pins the accepted runtime: refuse to package drift.
baseline = json.loads((NATIVE / "RUNTIME_BASELINE.json").read_text())
for name, src in sources.items():
    if sha(src) != baseline["runtime_artifacts"][name]["sha256"]:
        sys.exit(f"{name}: differs from recorded baseline {baseline['recorded_utc']}; re-record it deliberately")

# Unit tests must pass before anything is packaged.
tests = {}
for test in ("test_acv_settings.py", "test_acv_profiles.py"):
    result = subprocess.run([sys.executable, "-I", str(NATIVE / test)], capture_output=True, text=True)
    tests[test] = {"returncode": result.returncode, "summary": result.stderr.strip().splitlines()[-1] if result.stderr.strip() else ""}
    if result.returncode:
        sys.exit(f"{test} failed:\n{result.stderr}")

if args.out.exists():
    shutil.rmtree(args.out)
files = {}
def put(src, rel):
    dst = args.out / rel
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)
    files[rel] = {"sha256": sha(dst), "size": dst.stat().st_size}

put(args.launcher, "bin/acv-launcher")
for f in TOOL_FILES:
    put(NATIVE / f, f"tools/native-rexglue/{f}")
for name, src in sources.items():
    put(src, f"tmp/acv-rexglue-linux-indexed/out/build/linux-release/{name}")
(args.out / "README.txt").write_text("""ACV Linux test package
1. Extract the game files to tmp/iso_root (default.xex is checked by SHA-256). They are NOT included.
2. Start bin/acv-launcher (Qt 6.12 runtime required), create a profile, press Play.
Saves live in tmp/acv-user-data/profiles/<uuid>/userdata; caches in .../cache.
Unavailable in this build: >60 FPS, Y'CbCr output, 10-bit output (see the launcher's options).
""")
files["README.txt"] = {"sha256": sha(args.out / "README.txt"), "size": (args.out / "README.txt").stat().st_size}
manifest = {"created_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(), "files": files,
            "tests_at_packaging": tests,
            "baseline": {"recorded_utc": baseline["recorded_utc"], "project_head": baseline["project"]["head"],
                         "rexglue_sdk_head": baseline["rexglue_sdk"]["head"],
                         "sdk_working_diff_sha256": baseline["rexglue_sdk"]["working_diff_sha256"]},
            "excluded": ["tmp/iso_root (game data)", "personal saves", "caches", "logs"],
            "not_verified": ["rendering effect of settings (no visual verification by project rule)",
                             "in-game save/load round trip"]}
(args.out / "manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
print(f"packaged {len(files)} files into {args.out}")
