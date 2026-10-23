#!/usr/bin/env python3
"""Build the SDK runtime and GPU plugin with REXGLUE_ENABLE_FIDELITYFX in a private tree and stage them.

Works around ninja running the generated shader-permutation commands through bash: an unquoted
-DNAME={0,1} is brace-expanded into two conflicting defines. The generated build.ninja is quoted
after configure (the SDK sources are not modified). Output: tmp/acv-runtime-ffx/ (never the accepted
artifacts). Run through tools/decomp-logging/cli.mjs like other builds."""
import re, shutil, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SDK, BUILD, OUT = ROOT / "tools/rexglue-sdk", ROOT / "tmp/acv-sdk-ffx-build", ROOT / "tmp/acv-runtime-ffx"
run = lambda *a: subprocess.run(a, check=True)

if not (BUILD / "build.ninja").exists():
    run("cmake", "-S", str(SDK), "-B", str(BUILD), "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_C_COMPILER=clang", "-DCMAKE_CXX_COMPILER=clang++", "-DCMAKE_CXX_FLAGS=-march=x86-64-v2",
        "-DREXGLUE_USE_VULKAN=ON", "-DREXGLUE_USE_D3D12=OFF", "-DREXGLUE_ENABLE_FIDELITYFX=ON",
        "-DREXGLUE_BUILD_TESTS=OFF", "-DREXGLUE_ENABLE_SANITIZERS=OFF",
        "-DREXGLUE_ENABLE_TRACY=OFF", "-DREXGLUE_ENABLE_PERF_COUNTERS=OFF")
ninja_file = BUILD / "build.ninja"
text = ninja_file.read_text()
fixed = re.sub(r"(?<= )(-D[A-Za-z0-9_]+=\{[0-9,]+\})", r"'\1'", text)
if fixed != text:
    ninja_file.write_text(fixed)
run("ninja", "-C", str(BUILD), f"-j{__import__('os').cpu_count()}", "librexruntime.so", "librexgpu-xenos.so")
OUT.mkdir(exist_ok=True)
for name in ("librexruntime.so", "librexgpu-xenos.so", "libamd_fidelityfx_vk.so"):
    shutil.copy2(SDK / "out/linux-amd64" / name, OUT / name)
print(f"staged FidelityFX runtime in {OUT}")
