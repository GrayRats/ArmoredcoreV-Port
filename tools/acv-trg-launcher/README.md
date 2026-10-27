# ACV TRG frontend

C++17 / SDL2 2.32.10 / OpenGL3.3 / Dear ImGui1.92.9b; upstream TRG is pinned in
[dependencies.json](dependencies.json). Source remains separate from upstream and the Qt checkpoint.
CMake checks exact checkout revisions. Python policy stays in
[acv_settings.py](../native-rexglue/acv_settings.py); widgets cache one asynchronous JSON snapshot.

Build from the project root through the required logger:

```bash
node tools/decomp-logging/cli.mjs run --stage trg-configure --timeout-ms 600000 -- cmake -S tools/acv-trg-launcher -B tmp/acv-trg-build -DCMAKE_BUILD_TYPE=Release
node tools/decomp-logging/cli.mjs run --stage trg-build --timeout-ms 600000 -- cmake --build tmp/acv-trg-build --parallel 4
node tools/decomp-logging/cli.mjs run --stage trg-tests -- ctest --test-dir tmp/acv-trg-build --output-on-failure
node tools/decomp-logging/cli.mjs run --stage trg-backend -- python3 tools/acv-trg-launcher/test_backend.py
```

`backend.py`: snapshot/save/reset plus installation verification/publication. No schema copy,
no acv.config rewriting. UUID/XUID and backups use the existing profile CLI. Save is queued and
backend failures remain visible; PLAY starts only after validation and atomic settings publication.
The probe session has no time limit, owns the profile lock and passes it to the runtime.
Closing this window leaves the supervised game alive; Stop sends SIGTERM to our own supervisor.

`install.h`: TRG Task reads the owned ISO, verifies title4E4D0853, extracts to an exclusive sibling
staging directory and invokes the Python full manifest verifier. Publication uses renameat2
NOREPLACE. Cancellation removes staging and never replaces an existing installation. The
26-file manifest contains sizes/hashes only. ISO/game data are never published to GitHub.

Standalone packaging reuses [standalone.py](../acv-launcher/standalone.py) with `--frontend trg`,
`--runtime-dir`, `--out` and optional `--include-game-data`. It copies the bridge, Python modules,
fonts/licenses and runtime assets; no Qt/ICU. `verify_package.py` checks hashes/resolution;
`verify_frontend.py` tests the real GUI→probe→game and lock lifecycle with an isolated profile.
Run these through the project logger. Never treat their startup results as gameplay acceptance.

The local candidate is `../../../ACV-Linux-TRG-20261025/start.sh`; XDG data/state and optional
ACV_PROFILES_ROOT preserve profiles across folder moves. ACV_ROOT may select a package root;
normal launches locate it relative to the executable and work from an arbitrary cwd.
TRG upstream checkout has no declared license file; its original README and revision are retained
in this local candidate. No public redistribution is performed by these scripts.

Open acceptance: physical controller/HiDPI, full UI installation interaction, real save/load,
mission-selection regression, visual CAS/FSR, high FPS timestep and UltraWide HUD/culling.
