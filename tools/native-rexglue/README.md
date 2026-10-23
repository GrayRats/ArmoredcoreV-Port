# ACV independent Linux ReXGlue probe

No XenonRecomp-generated C++ is used in this build. Original game input remains
read-only. See `docs/NATIVE_REXGLUE_LINUX.md` for checked evidence and limits.

- `acv-boundaries.toml`: 31 instruction-backed analysis hints; no semantic names.
- `validate_boundaries.py`: verify hints against the pinned original XEX and
  saved listings/registry. Run through project SQLite logger. `--manifest` adds
  the include to a freshly initialized private manifest under `tmp/`.
- `codegen-override.cmake`: choose an explicitly supplied isolated codegen CLI
  without editing generated boilerplate or replacing the installed SDK.
- `CMakeLists.linux-probe.txt`: preserved handwritten private host build file;
  stages Linux runtime and requested Xenos plugin with the official SDK targets.

Active private tree: `tmp/acv-rexglue-linux-indexed`. CMake release configuration
will use installed SDK 0.10.0, `/usr/bin/clang++`, Ninja and an explicit
`ACV_CODEGEN_EXECUTABLE` pointing to the checked experimental CLI. Regenerate and
reconfigure after boundary changes. Do not use `--force` to bypass validation.

Full game data is already extracted under `tmp/iso_root`; the private `assets/`
folder contains only `default.xex`. User/cache/config paths for launch must stay
inside workspace `tmp/`. No Windows builds without a new user request.

`probe_linux.py` performs a bounded launch of our private ELF, forwarding both streams into the outer SQLite runner and saving launch metadata. Actual game behavior remains unverified until that probe runs and its logs are inspected.

Build the current test executable (preserves generated code, PCH and objects):

    node tools/decomp-logging/cli.mjs run --stage acv_test_build --timeout-ms 600000 -- python3 tools/native-rexglue/build_linux.py

The builder synchronizes the canonical host CMake file; CMake copies the canonical
app header into the private tree. The host revision is printed as [acv-build].
Artifact SHA-256 values are saved beside the ELF in acv-test-build.json.

Launch a five-minute manual test with canonical acv.config and the existing
acv-mnk-profile (close the window earlier to stop):

    ./tools/native-rexglue/test_linux.sh

The launcher logs through SQLite and accepts the probe's additional arguments.
Confirmed boot/input/import/capture changes remain included; experimental
readback/gamma/allocation switches retain their default values. The separate
pixel-debugger changes are compiled into tmp/acv-targets/replay_debug_pixel,
not into the game's shaders.

The launcher accepts UTF-8 config files with or without a BOM and preserves the
original file bytes for the recorded config hash. Missing config remains an error.

### Corrected private GPU plugin

The latest test package includes the verified compute-write access-scope
correction. Rebuild while retaining that private plugin with:

```bash
node tools/decomp-logging/cli.mjs run --stage linux_build --timeout-ms 180000 -- python3 tools/native-rexglue/build_linux.py --gpu-plugin-artifact tmp/acv-gpu-sync-test/librexgpu-xenos.so
```

The artifact path/hash is recorded in `acv-test-build.json`. Default SDK staging
may restore the installed plugin during a later relink unless this override is
passed. Installed SDK files remain unchanged. `ACV_TRACE_PHYSICAL_ALLOCS=1`
enables a bounded allocation ABI trace; it changes no allocation semantics.
The dark-scene defect remains open.
