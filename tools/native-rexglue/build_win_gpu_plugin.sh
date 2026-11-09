#!/usr/bin/env bash
# Rebuild the Windows GPU plugin (rexgpu-xenos.dll) natively in MSYS2 UCRT64 and optionally install it into a package.
#
# Why UCRT64: the shipped DLLs import libstdc++-6, libgcc_s_seh-1, libwinpthread-1 and the UCRT. The CLANG64 environment
# uses libc++, which does not compile the SDK (chrono.h) and would not match the ABI of librexruntime.dll.
#
# One-time setup, in an MSYS2 shell:
#   pacman -S --needed mingw-w64-ucrt-x86_64-clang mingw-w64-ucrt-x86_64-lld mingw-w64-ucrt-x86_64-cmake \
#       mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-python
#
# Usage (from any shell): bash tools/native-rexglue/build_win_gpu_plugin.sh [PACKAGE_DIR]
#   Builds tmp/win-ucrt-build and writes tools/rexglue-sdk/out/win-amd64/rexgpu-xenos.dll (the SDK output folder also
#   receives librexruntime.dll and the static libraries; the previous files are backed up first).
#   With PACKAGE_DIR (for example D:/USER/Desktop/DEV/ACV-Windows-release-2.2) the plugin is checked against the
#   package's runtime and copied to PACKAGE_DIR/bin/game, keeping the old one in tmp/acv-backups.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PACKAGE="${1:-}"
MSYS="${MSYS2_ROOT:-/c/msys64}"
BUILD="$REPO/tmp/win-ucrt-build"
OUT="$REPO/tools/rexglue-sdk/out/win-amd64"
STAMP="$(date +%Y%m%d-%H%M%S)"

run_ucrt() {  # run a command in the UCRT64 environment
  "$MSYS/usr/bin/bash.exe" -lc "export MSYSTEM=UCRT64; source /etc/profile; export PROCESSOR_ARCHITECTURE=AMD64; $1"
}

mkdir -p "$REPO/tmp/acv-backups/pre-build-$STAMP"
for f in rexgpu-xenos.dll librexruntime.dll; do
  [ -f "$OUT/$f" ] && cp -p "$OUT/$f" "$REPO/tmp/acv-backups/pre-build-$STAMP/"
done

# Same options as the earlier cross build (tmp/rexglue-win-dx-build2): Vulkan and D3D12, no FidelityFX/tests.
run_ucrt "cd '$REPO' && cmake -S tools/rexglue-sdk -B tmp/win-ucrt-build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_STANDARD=23 \
  -DCMAKE_C_FLAGS='-march=x86-64-v2' -DCMAKE_CXX_FLAGS='-march=x86-64-v2 -fms-extensions' \
  -DREXGLUE_USE_VULKAN=ON -DREXGLUE_USE_D3D12=ON -DREXGLUE_ENABLE_FIDELITYFX=OFF -DREXGLUE_BUILD_TESTS=OFF \
  -DREXGLUE_ENABLE_TRACY=ON -DREXGLUE_ENABLE_PERF_COUNTERS=ON"
run_ucrt "cd '$REPO' && ninja -C tmp/win-ucrt-build rexgpu-xenos"
echo "built: $OUT/rexgpu-xenos.dll"

[ -n "$PACKAGE" ] || exit 0
GAME="$PACKAGE/bin/game"
READOBJ="$MSYS/ucrt64/bin/llvm-readobj.exe"
imports() { "$READOBJ" --coff-imports "$1" | awk -v L="$2" 'index($0,"Name: " L){f=1;next} /^Import \{/{f=0} f&&/Symbol: /{sub(/.*Symbol: /,""); sub(/ \(.*/,""); print}' | sort -u; }
exports() { "$READOBJ" --coff-exports "$1" | awk '/Name: /{sub(/.*Name: /,""); print}' | sort -u; }
# Every symbol the new plugin imports must exist in the package's DLLs (link-level ABI check; it cannot see layouts).
for lib in librexruntime.dll libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll; do
  missing="$(comm -23 <(imports "$OUT/rexgpu-xenos.dll" "$lib") <(exports "$GAME/$lib") | wc -l)"
  echo "$lib: $missing imported symbols missing in the package"
  [ "$missing" = 0 ] || { echo "refusing to install"; exit 1; }
done
mkdir -p "$REPO/tmp/acv-backups/pre-install-$STAMP"
cp -p "$GAME/rexgpu-xenos.dll" "$REPO/tmp/acv-backups/pre-install-$STAMP/"
cp -p "$OUT/rexgpu-xenos.dll" "$GAME/rexgpu-xenos.dll"
echo "installed into $GAME (old plugin: tmp/acv-backups/pre-install-$STAMP)"
