#!/usr/bin/env python3
"""Incremental Linux build; invoke via the project SQLite runner."""
import argparse
import os
import subprocess
import shutil
import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--jobs", type=int)
parser.add_argument("--gpu-plugin-artifact", type=Path,
                    help="Stage a tested private GPU plugin after the host build; record its origin")
args = parser.parse_args()
gpu_artifact = args.gpu_plugin_artifact.resolve() if args.gpu_plugin_artifact else None
if gpu_artifact:
    if not gpu_artifact.is_relative_to(ROOT / "tmp") or not gpu_artifact.is_file():
        parser.error("--gpu-plugin-artifact must be an existing private artifact inside workspace tmp/")
    with gpu_artifact.open("rb") as stream:
        if stream.read(4) != b"\x7fELF":
            parser.error("--gpu-plugin-artifact must be a Linux ELF shared library")
available_kib = int(next(line.split()[1] for line in Path("/proc/meminfo").read_text().splitlines()
                         if line.startswith("MemAvailable:")))
# Reserve two logical CPUs for the desktop; budget 2 GiB per compiler job.
default_jobs = min(10, max(1, (os.cpu_count() or 2) - 2), max(1, available_kib // (2 * 1024 * 1024)))
jobs = args.jobs if args.jobs is not None else default_jobs
if jobs < 1:
    parser.error("--jobs must be positive")
source_dir = ROOT / "tmp/acv-rexglue-linux-indexed"
build_dir = source_dir / "out/build/linux-release"
canonical_cmake = ROOT / "tools/native-rexglue/CMakeLists.linux-probe.txt"
private_cmake = source_dir / "CMakeLists.txt"
if canonical_cmake.read_bytes() != private_cmake.read_bytes():
    shutil.copy2(canonical_cmake, private_cmake)
    print("Synchronized canonical host CMake configuration", flush=True)
print(f"Incremental Linux build: {jobs} jobs, {available_kib // 1024} MiB available; preserve PCH and objects", flush=True)
result = subprocess.call(["cmake", "--build", str(build_dir), "--parallel", str(jobs)], cwd=ROOT)
if result:
    raise SystemExit(result)

def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()

if gpu_artifact:
    staged_gpu = build_dir / "librexgpu-xenos.so"
    if gpu_artifact != staged_gpu.resolve():
        # Replace the file atomically; existing mappings retain their old inode.
        temporary_gpu = staged_gpu.with_suffix(".so.staging")
        shutil.copy2(gpu_artifact, temporary_gpu)
        temporary_gpu.replace(staged_gpu)
    print(f"Staged explicit private GPU plugin: {gpu_artifact}", flush=True)

artifact_names = ["acv_linux_probe", "librexruntime.so", "librexgpu-xenos.so"]
artifacts = {name: {"path": str(build_dir / name), "sha256": digest(build_dir / name)}
             for name in artifact_names}
manifest = {
    "built_at_utc": datetime.now(timezone.utc).isoformat(),
    "scope": "Linux test build; artifact hashes do not establish gameplay correctness",
    "artifacts": artifacts,
    "gpu_plugin_override": {"path": str(gpu_artifact), "sha256": digest(gpu_artifact)} if gpu_artifact else None,
    "config": {"path": str(ROOT / "tools/native-rexglue/acv.config"),
               "sha256": digest(ROOT / "tools/native-rexglue/acv.config")},
    "host_revision_header": (build_dir / "acv_build_info.h").read_text(),
}
manifest_path = build_dir / "acv-test-build.json"
manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
print(f"Test build manifest: {manifest_path}", flush=True)
