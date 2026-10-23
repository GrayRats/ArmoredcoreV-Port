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
parser.add_argument("--runtime-artifact", type=Path,
                    help="Stage a tested private librexruntime.so after the host build; record its origin")
args = parser.parse_args()
source_dir = ROOT / "tmp/acv-rexglue-linux-indexed"
build_dir = source_dir / "out/build/linux-release"
manifest_path = build_dir / "acv-test-build.json"

def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()

gpu_artifact = args.gpu_plugin_artifact.resolve() if args.gpu_plugin_artifact else None
gpu_selection = "explicit" if gpu_artifact else "sdk_default"
expected_gpu_hash = None
if gpu_artifact is None and manifest_path.exists():
    try:
        previous = json.loads(manifest_path.read_text()).get("gpu_plugin_override")
        if previous is not None:
            gpu_artifact = Path(previous["path"]).resolve()
            expected_gpu_hash = previous["sha256"]
            if not isinstance(expected_gpu_hash, str) or len(expected_gpu_hash) != 64:
                raise ValueError("invalid recorded GPU hash")
            gpu_selection = "retained_manifest"
    except (OSError, ValueError, TypeError, KeyError) as error:
        parser.error(f"Cannot retain recorded GPU selection: {error}; pass --gpu-plugin-artifact explicitly")
if gpu_artifact:
    if not gpu_artifact.is_relative_to(ROOT / "tmp") or not gpu_artifact.is_file():
        parser.error("Selected GPU artifact is missing or outside workspace tmp/; pass --gpu-plugin-artifact explicitly")
    with gpu_artifact.open("rb") as stream:
        if stream.read(4) != b"\x7fELF":
            parser.error("Selected GPU artifact must be a Linux ELF shared library")
    actual_gpu_hash = digest(gpu_artifact)
    if expected_gpu_hash is not None and actual_gpu_hash != expected_gpu_hash:
        parser.error("Recorded GPU artifact changed; pass --gpu-plugin-artifact explicitly after verification")
    expected_gpu_hash = actual_gpu_hash
    print(f"GPU selection ({gpu_selection}): {gpu_artifact}", flush=True)

runtime_artifact = args.runtime_artifact.resolve() if args.runtime_artifact else None
runtime_selection = "explicit" if runtime_artifact else "sdk_default"
expected_runtime_hash = None
if runtime_artifact is None and manifest_path.exists():
    try:
        previous = json.loads(manifest_path.read_text()).get("runtime_override")
        if previous is not None:
            runtime_artifact = Path(previous["path"]).resolve()
            expected_runtime_hash = previous["sha256"]
            if not isinstance(expected_runtime_hash, str) or len(expected_runtime_hash) != 64:
                raise ValueError("invalid recorded runtime hash")
            runtime_selection = "retained_manifest"
    except (OSError, ValueError, TypeError, KeyError) as error:
        parser.error(f"Cannot retain recorded runtime selection: {error}; pass --runtime-artifact explicitly")
if runtime_artifact:
    if not runtime_artifact.is_relative_to(ROOT / "tmp") or not runtime_artifact.is_file():
        parser.error("Selected runtime artifact is missing or outside workspace tmp/; pass --runtime-artifact explicitly")
    with runtime_artifact.open("rb") as stream:
        if stream.read(4) != b"\x7fELF":
            parser.error("Selected runtime artifact must be a Linux ELF shared library")
    actual_runtime_hash = digest(runtime_artifact)
    if expected_runtime_hash is not None and actual_runtime_hash != expected_runtime_hash:
        parser.error("Recorded runtime artifact changed; pass --runtime-artifact explicitly after verification")
    expected_runtime_hash = actual_runtime_hash
    print(f"Runtime selection ({runtime_selection}): {runtime_artifact}", flush=True)
available_kib = int(next(line.split()[1] for line in Path("/proc/meminfo").read_text().splitlines()
                         if line.startswith("MemAvailable:")))
# Reserve two logical CPUs for the desktop; budget 2 GiB per compiler job.
default_jobs = min(10, max(1, (os.cpu_count() or 2) - 2), max(1, available_kib // (2 * 1024 * 1024)))
jobs = args.jobs if args.jobs is not None else default_jobs
if jobs < 1:
    parser.error("--jobs must be positive")
canonical_cmake = ROOT / "tools/native-rexglue/CMakeLists.linux-probe.txt"
private_cmake = source_dir / "CMakeLists.txt"
if canonical_cmake.read_bytes() != private_cmake.read_bytes():
    shutil.copy2(canonical_cmake, private_cmake)
    print("Synchronized canonical host CMake configuration", flush=True)
print(f"Incremental Linux build: {jobs} jobs, {available_kib // 1024} MiB available; preserve PCH and objects", flush=True)
result = subprocess.call(["cmake", "--build", str(build_dir), "--parallel", str(jobs)], cwd=ROOT)
if result:
    raise SystemExit(result)

if gpu_artifact:
    if digest(gpu_artifact) != expected_gpu_hash:
        raise SystemExit("Selected GPU artifact changed during host build; refusing to stage it")
    staged_gpu = build_dir / "librexgpu-xenos.so"
    if gpu_artifact != staged_gpu.resolve():
        # Replace the file atomically; existing mappings retain their old inode.
        temporary_gpu = staged_gpu.with_suffix(".so.staging")
        shutil.copy2(gpu_artifact, temporary_gpu)
        if digest(temporary_gpu) != expected_gpu_hash:
            raise SystemExit("Selected GPU artifact changed while copying; refusing to replace staged GPU")
        temporary_gpu.replace(staged_gpu)
    print(f"Staged selected private GPU plugin: {gpu_artifact}", flush=True)

if runtime_artifact:
    if digest(runtime_artifact) != expected_runtime_hash:
        raise SystemExit("Selected runtime artifact changed during host build; refusing to stage it")
    staged_runtime = build_dir / "librexruntime.so"
    if runtime_artifact != staged_runtime.resolve():
        temporary_runtime = staged_runtime.with_suffix(".so.staging")
        shutil.copy2(runtime_artifact, temporary_runtime)
        if digest(temporary_runtime) != expected_runtime_hash:
            raise SystemExit("Selected runtime artifact changed while copying; refusing to replace staged runtime")
        temporary_runtime.replace(staged_runtime)
    print(f"Staged selected private runtime: {runtime_artifact}", flush=True)

artifact_names = ["acv_linux_probe", "librexruntime.so", "librexgpu-xenos.so"]
artifacts = {name: {"path": str(build_dir / name), "sha256": digest(build_dir / name)}
             for name in artifact_names}
manifest = {
    "built_at_utc": datetime.now(timezone.utc).isoformat(),
    "scope": "Linux test build; artifact hashes do not establish gameplay correctness",
    "artifacts": artifacts,
    "gpu_plugin_selection": gpu_selection,
    "gpu_plugin_override": {"path": str(gpu_artifact), "sha256": expected_gpu_hash} if gpu_artifact else None,
    "runtime_selection": runtime_selection,
    "runtime_override": {"path": str(runtime_artifact), "sha256": expected_runtime_hash} if runtime_artifact else None,
    "config": {"path": str(ROOT / "tools/native-rexglue/acv.config"),
               "sha256": digest(ROOT / "tools/native-rexglue/acv.config")},
    "host_revision_header": (build_dir / "acv_build_info.h").read_text(),
}
manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
print(f"Test build manifest: {manifest_path}", flush=True)
