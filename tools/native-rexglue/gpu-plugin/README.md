# Private Linux Xenos plugin build

Builds the GPU plugin from the local SDK source inventory against the installed
ReXGlue 0.10.0 runtime and static dependencies. It does not install or replace
the regular game plugin. Keep the SDK source and installed dependency versions
matched. Required internal glslang headers are absent from the installed package;
use the SDK-pinned glslang source revision, not system glslang headers.

For the current SDK the glslang revision is
`f4f1d8a352ca1908943aea2ad8c54b39b4879080`. Locally fetched header checksums
are recorded in `logs/acv-gpu-sync-20261022/glslang-raw-headers.json`.

Run these commands through the project SQLite logger:

```bash
cmake -S tools/native-rexglue/gpu-plugin -B tmp/acv-gpu-plugin-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_PREFIX_PATH=~/.local/opt/rexglue-sdk/0.10.0 \
  -DACV_GLSLANG_HEADERS=tmp/acv-gpu-deps/glslang-f4f1d8a352ca1908943aea2ad8c54b39b4879080
cmake --build tmp/acv-gpu-plugin-build --target rexgpu-xenos -j 4
python3 tools/native-rexglue/test_shared_memory_barriers.py
```

The plugin loader opens `librexgpu-xenos.so` next to the executable. Stage it
beside an isolated copy of the ELF and runtime, then pass that executable to
`probe_linux.py --executable`. Record the staged plugin hash and verify its
actual path in the owned process maps. The normal build may restage the
installed plugin, so a host rebuild alone does not apply this SDK correction.
The access-scope regression is a host test; rendering effects require capture
and a comparable runtime scenario.

Optional upload snapshots: configure `-DACV_CAPTURE_UPLOADS=ON`, build and stage
the diagnostic variant separately. Environment variables
`ACV_GPU_UPLOAD_PROBE_DIR`, `ACV_GPU_UPLOAD_PROBE_START`,
`ACV_GPU_UPLOAD_PROBE_LENGTH` select an existing output directory and physical
range (decimal or0x notation). Maximum range8 MiB, at most32 snapshots. The
snapshot is the staging memcpy result, not a later live CPU dump. File I/O
changes timing. Record captures and resource identity before comparing events.
The regular fixed plugin is preserved in `tmp/acv-gpu-sync-test`; the diagnostic
variant is in `tmp/acv-gpu-upload-test`.
