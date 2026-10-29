#!/bin/bash
set -euo pipefail
# Runs inside the disposable CI container, never in a user's existing checkout.
git clone https://github.com/TekRantGaming/trg-launcher.git tools/trg-launcher
git -C tools/trg-launcher checkout --detach b384a28c712c9320eea8f9d50639dec49981b6c1
git clone --no-checkout https://github.com/rexglue/rexglue-sdk.git tools/rexglue-sdk
git -C tools/rexglue-sdk checkout --detach c94f5ebdcb3c9d1a460ca48e04f9758448f8d518
git -C tools/rexglue-sdk submodule update --init --depth 1 thirdparty/inja
cmake -S tools/acv-trg-launcher -B tmp/acv-trg-build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_C_FLAGS=-march=x86-64-v2 -DCMAKE_CXX_FLAGS=-march=x86-64-v2
cmake --build tmp/acv-trg-build --target acv-launcher --parallel 2
xvfb-run -a -s '-screen 0 1280x900x24' python3 tools/build-tools/capture_launcher.py
