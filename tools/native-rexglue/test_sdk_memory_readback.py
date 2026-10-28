#!/usr/bin/env python3
"""Compile and exercise the explicit access boundary against the built SDK."""
import sys
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "tmp/acv-sdk-ffx-build"
SDK = ROOT / "tools/rexglue-sdk"
BENCHMARK = "--read-benchmark" in sys.argv
VULKAN = "--vulkan" in sys.argv
PARALLEL_COPY = "--parallel-copy" in sys.argv
if PARALLEL_COPY and not VULKAN: raise ValueError("--parallel-copy requires --vulkan")
GRAPHICS = "--graphics-cpu" in sys.argv
XMA = "--xma" in sys.argv
FILE_IO = "--file-io" in sys.argv
FAULTS = "--faults" in sys.argv or FILE_IO or XMA or GRAPHICS or VULKAN or BENCHMARK
OUT = ROOT / ("tmp/sdk_physical_read_benchmark" if BENCHMARK else "tmp/sdk_vulkan_readback_test" if VULKAN else "tmp/sdk_graphics_cpu_access_test" if GRAPHICS else "tmp/sdk_xma_physical_access_test" if XMA else "tmp/sdk_physical_file_io_test" if FILE_IO else "tmp/sdk_physical_provider_fault_test" if FAULTS else "tmp/sdk_memory_readback_test")
commands = subprocess.check_output(["ninja", "-C", str(BUILD), "-t", "commands"], text=True)
command = next(line for line in commands.splitlines()
               if "xmemory.cpp" in line and " -c " in line)
args = shlex.split(command)
# Retain the SDK's pinned includes, ABI defines and language mode; omit PCH and
# release NDEBUG so all semantic assertions in this integration test execute.
flags = []
i = 1
while i < len(args):
    arg = args[i]
    if arg in ("-isystem", "-I", "-D"):
        flags.extend(args[i:i + 2])
        i += 2
        continue
    if arg.startswith(("-I", "-D", "-std=", "-march=", "-fno-strict-aliasing", "-ffp-model=")) and arg != "-DNDEBUG":
        flags.append(arg)
    i += 1
if FILE_IO:
    original = (SDK / "src/system/xfile.cpp").read_text()
    start = original.index("X_STATUS XFile::ReadInternal(")
    end = original.index("X_STATUS XFile::ReadScatter(", start)
    body = original[start:end].replace("XFile::ReadInternal", "TestFile::ReadInternal").replace("XIOCompletion", "MockCompletion")
    (ROOT / "tmp/acv_xfile_read_internal.inc").write_text(body)
    flags.append("-I" + str(ROOT / "tmp"))
if XMA:
    original = (SDK / "src/audio/xma_context.cpp").read_text()
    start = original.index("namespace {\nstruct PhysicalAudioAccess")
    end = original.index("void XmaContext::Enable()", start)
    body = original[start:end].replace("XmaContext::Work", "MockXmaContext::Work")
    (ROOT / "tmp/acv_xma_work.inc").write_text(body)
    flags.append("-I" + str(ROOT / "tmp"))
source = ROOT / "tools/native-rexglue" / ("sdk_physical_read_benchmark.cpp" if BENCHMARK else "sdk_vulkan_readback_test.cpp" if VULKAN else "sdk_graphics_cpu_access_test.cpp" if GRAPHICS else "sdk_xma_physical_access_test.cpp" if XMA else "sdk_physical_file_io_test.cpp" if FILE_IO else "sdk_physical_provider_fault_test.cpp" if FAULTS else "sdk_memory_readback_test.cpp")
libraries = (Path(sys.argv[sys.argv.index("--runtime-dir") + 1]).resolve(strict=True) if "--runtime-dir" in sys.argv else SDK / "out/linux-amd64")
compile_args = [args[0], *flags, "-UNDEBUG", "-O1", "-g", "-pthread",
                *(["-fsanitize=address,undefined", "-fno-omit-frame-pointer"] if not FAULTS else []),
                str(source), *(["-lvulkan"] if VULKAN else []), "-L" + str(libraries), "-lrexruntime",
                "-Wl,-rpath," + str(libraries), "-o", str(OUT)]
subprocess.run(compile_args, cwd=BUILD, check=True)
subprocess.run(["timeout", "20s", str(OUT), *(["--expect-no-allocations"] if BENCHMARK and "--expect-no-allocations" in sys.argv else []), *(["--pending-providers"] if BENCHMARK and "--pending-providers" in sys.argv else []), *(["--parallel-copy"] if PARALLEL_COPY else [])], cwd=ROOT, check=True)
print("Compiled SDK bridge integration verified")
