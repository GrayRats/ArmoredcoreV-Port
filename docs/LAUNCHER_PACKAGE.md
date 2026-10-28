# Standalone ACV Linux package — 2026-10-25

Latest TRG candidate and checks: [checkpoint](#trg-frontend-and-installation--2026-10-25).
The Qt description below is the preserved previous package.

Local output: `../../ACV-Linux/`, entry point `start.sh`.
This is a statically recompiled game with ReXGlue, not a fully recovered
ACV source code. The package contains its own copies of the game data, Qt/ICU,
Qt platform plugins, the host ELF, the runtime, the GPU and FidelityFX. The originals were not changed.
Linux/glibc, Python 3, X11/XWayland, the Vulkan driver and the other system libraries
are required from the OS; their list is in the manifest. Other distributions have not been tested.

[Русская версия](LAUNCHER_PACKAGE.RU.MD)

## Build and paths

[standalone.py](../tools/acv-launcher/standalone.py) accepts `--out`,
`--runtime-dir` and `--include-game-data`. An existing output is not deleted.
Files are copied into a neighbouring staging directory, hashes, dependencies and the CLI are
verified, and then the staging directory is renamed to the output. A runtime other than the accepted
[RUNTIME_BASELINE](../tools/native-rexglue/RUNTIME_BASELINE.json) is explicitly marked
as a candidate; this is not an automatic acceptance of a new baseline.

[acv_paths.py](../tools/native-rexglue/acv_paths.py) recognises the marker
`acv-package.json`: the executable in `bin/game/`, the data in `data/game/`, profiles in
`${XDG_DATA_HOME:-~/.local/share}/DEArmoredcore/acv/`, logs in
`${XDG_STATE_HOME:-~/.local/state}/DEArmoredcore/acv/logs/`.
`ACV_PROFILES_ROOT` allows choosing a separate storage location. The launcher uses
the same paths. The SDK RUNPATH no longer decides which libraries are chosen: the launch
puts the directory of the selected runtime first in LD_LIBRARY_PATH. The ARTEM config is kept.

## Verified

- [verification.json](../logs/acv-standalone-20261024/verification.json): 183 hashes in the first build,
  a visible Qt/X11 window, the game launched from cwd `/`, runtime/GPU/FidelityFX and Qt
  loaded from a separate folder according to the actual process maps. There is no symlink
  to the source tree. The game probe ran for 20 s; the swapchain and pipelines were created.
- The probe returned 1 because of the known missing `system.properties`;
  this diagnostic was not hidden. There are no other runtime errors in this run.
  The runtime itself finished with a normal bounded probe. This is not gameplay acceptance.
- The unique XUID of the test profile is passed through the environment and recorded in the launch
  report. Whether the real save belongs to this XUID has not been verified yet.
- After the launch the number of shm orphans did not increase. Before profiling, 17 xenia_memory
  files no longer mapped by any process were removed (7,573,667,840 bytes).
- Python: 15 settings, 9 profiles, 3 shm tests. Qt: the launcher ctest passed.
  A new regression test checks that the flock is kept by the child process after the parent
  closes the descriptor. The probe passes the descriptor to the game via pass_fds.
- [X11 startup samples](../logs/acv-native-linux/20261024T000238328506Z/samples.json):
  58 samples over 60.97 s with 0 orphans before/after, 25,139,380,224 free bytes before/after.
  Host main thread: ioctl 36/58; GPU Commands top frame in libc 41/58;
  DrawThread libc 31/58, sub_82BB58A8 9/58, __imp__sub_82AF5708 8/58.
  This is the distribution of sampled stacks, not the distribution of CPU time. GDB and the
  presence of RenderDoc in the runtime distort the speed; the bottleneck and the FPS are not proven from it.

## Not accepted

Game save/load, restoring a real save, isolation of two game profiles and the XUID inside a save;
visual CAS/FSR; pacing/sound with the limiter; adaptive FOV/HUD, >60 FPS and visual accuracy of 10-bit output.
The patched runtime is in a separate package as a candidate; the accepted development-build artifacts were not replaced.
The current queue and criteria: [PLAN_GUI](../PLAN_GUI.MD).

## Display, colour and frame pacing — 2026-10-24

The [matrix of three runs](../logs/acv-display-20261024/matrix.json) records
35 s UltraWide/Bilinear, 35 s 16:10/CAS/FIFO and 35 s UltraWide/FSR/10-bit request.
In all three the child exited with 0, with no new shm orphans; the probe exit 1 keeps the known
optional system.properties diagnostic. No screenshots were taken.

The SDK function `GetConfiguredVideoModeWidth/Height` was fixed: the host window no longer
overrides the guest video mode. The former `HasNonDefaultValue` treated an explicitly set
1280/720 as not overridden and took the width/height of the host window. An actual-source
host test checked the defaults, independence from the host size, the resolution preset and
an explicit guest mode. The SDK changes are reproduced by applying the earlier patches in sequence
and [launcher-display-output.patch](../tools/native-rexglue/patches/launcher-display-output.patch).
The previous source bytes are kept in logs/acv-display-20261024/source-before.

Confirmed geometry:

| Host window | Image | Offset | Check |
|---|---|---|---|
| 1600×675 | 1200×675 | 200,0 | UltraWide, Bilinear |
| 1280×800 | 1280×720 | 0,40 | 16:10, CAS sharpen |
| 2560×1052 after resize | 1870×1052 | 345,0 | Bilinear/CAS/FSR, rounding to the pixel |

This is fitting the original 16:9 with bars. Extended FOV/HUD is still missing.
The host window manager changed the window size during the runs; the runtime reflected the real
size and recreated the swapchain while keeping the proportions.

VSync On is confirmed as FIFO (2), Off as Immediate (0). What is measured are the intervals of
XE_SWAP processing, not the simulation/sound speed and not the physical refresh rate of the monitor.
The limiter now keeps the deadline on the CommandProcessor instance and resets it when the limit changes.
Over windows of 300 samples: Bilinear 56.36–60.00 FPS;
CAS/FIFO 32.31–60.00; FSR 52.96–60.00. Start-up drops remain; a stable
60 in gameplay is not claimed. 120/144/165 are unavailable until guest timing validation.

CAS actually selected final effects 2/4; FSR — final effect 7/RCAS, two passes.
The RCAS sharpness is now controlled separately (0…2 stops). This confirms the execution of the shader path,
not an acceptance of visual quality or HUD.

Colour selection uses the actual surface. Here the formats are 44/50 and colourspace 0:
BGRA8 UNORM/SRGB. A 10-bit request gave an explicit fallback to 8-bit. The new path selects
only the offered A2RGB10/A2BGR10 UNORM + SRGB_NONLINEAR, without HDR.
A 10-bit request combined with 8-bit dithering is rejected. The 10-bit output precision has not
been verified yet; the physical HDMI/DP encoding remains under the control of the system.
An erroneous branch that picked an arbitrary format whenever there was a single surface format was also fixed;
an error in the format query can now no longer be taken as permission to pick an arbitrary format.

The launcher shows the application's size/depth/output mode, the source/output rect
and cadence/p95. The probe saves a machine-readable display report. Verified: 19 settings,
9 profiles, 3 shm, 2 display parser tests and the Qt ctest, with a check of the actual 8 bits
when 10 bits were requested. The package with the new iteration contains 184 files; the runtime is still a candidate.

## UltraWide: perspective confirmation

A read-only GDB trace via `probe_linux.py --projection-trace LAYOUT_JSON`
collected the arguments and output blocks without changing guest registers or memory.
The layout was obtained by compiling `offsetof(PPCContext, ...)` from the SDK and tied to the SHA
of the executable. [Source/ABI](../logs/acv-ultrawide-20261024/context_layout.cpp),
[layout](../logs/acv-ultrawide-20261024/context_layout.json),
[trace](../logs/acv-native-linux/20261024T115046780620Z/projection.json).

The chain `0x82303138 → 0x82302520 → 0x82300948 → 0x82BC8980` is observed
at start-up. In `0x82302520` r8/r9=1280/720; in `0x82BC8980`
f1=1.04719758, f2=1.77777779, f3=1, f4=1000. All 64 returned blocks
matched the perspective `y=1/tan(f1/2), x=y/f2, z=f4/(f4-f3)`,
the last rows `[0,0,z,1]`, `[0,0,-f3*z,0]`, with a maximum normalised
error of 3.285e-7 against a threshold of 2e-6. This justifies interpreting the arguments as
a vertical FOV ≈ 60°, aspect 16:9 and near/far 1/1000 for the observed calls.
[Machine check](../logs/acv-ultrawide-20261024/perspective-check.json),
[checking script](../tools/native-rexglue/analyze_projection_trace.py).

Limitation: the 64 samples have one set of inputs and come from the recompiled
code, not from the original console. The main gameplay camera, HUD and culling are not yet tied to this
chain; `0x82A0CA28` (the Versus/free-camera assertion path) was not encountered. The breakpoints were limited to
64 samples at entry; this is a start-up sample, not coverage of the whole 25 s interval. GDB child exit 0,
probe exit 1 because of the earlier system.properties; shm orphans 0 before/after. The native UltraWide is not enabled by this.
The separate ACV-Linux package remains the previously verified display build;
the new diagnostic option is so far only in the development tree.

## UltraWide: single-call experiment — 2026-10-24

A temporary read-only 60 s trace with stacks covers eight 8 s windows,
up to 8 samples per entry in each window: [trace](../logs/acv-native-linux/20261024T115603115026Z/projection.json).
After the diagnostic Return/Space presses other inputs appeared: FOV about 36–55°,
near 1, far 450–13,000. All 64 matrices agree with the perspective formula,
with a maximum normalised error of 3.853e-7:
[check](../logs/acv-ultrawide-20261024/temporal-perspective-check.json).
This extends the earlier start-up sample, but does not prove the full gameplay semantics.

In the retail `0x822B4428`, when the input field +0x48 is zero, the aspect is taken from object+0x4C0,
the FOV from input+0x40 with a unit conversion, and it calls `0x82BC8980` at the address
`0x822B450C` (LR=`0x822B4510`). The near/far values are taken from object+0x444/+0x448.
The non-zero branch calls another helper `0x82BC8CC0`; it is not changed.
[Raw PPC/.pdata](../logs/acv-ultrawide-20261024/822B4428.txt),
[caller](../logs/acv-ultrawide-20261024/822B59C8.txt).

Added [scene_aspect_experiment.cpp](../tools/native-rexglue/scene_aspect_experiment.cpp):
a strong override of `sub_82BC8980` through the existing weak-alias scheme, calling the
original `__imp__sub_82BC8980`. By default it is a full pass-through.
Only the explicit variable `ACV_EXPERIMENTAL_SCENE_ASPECT` (a finite number 1…4)
enables the experiment: LR must be 0x822B4510, the original aspect ≈ 16:9,
and the FOV must lie in (0, π). Only f2 is changed for the duration of the original helper;
after the call the original f2 is restored. Other callers are not changed.

Isolated build: `tmp/acv-ultrawide-test/`, host SHA `e3ab37044dc3037c9aa1ed7dae8fc13424a1e1b0e37dbae48a6a35b8b604355b`.
[Build manifest](../tmp/acv-rexglue-linux-indexed/out/build/linux-release/acv-test-build.json)
records the selected display runtime/GPU and the unchanged config SHA 98d1c2ed….
The previous development host is kept in `tmp/acv-ultrawide-before/`.
The separate user-facing `../../ACV-Linux/` was not replaced by this build.

With the request 64:27 in a 60 s run, six measured calls to `0x822B4428` got
aspect 2.37037037; the other 58 kept 16:9. All 64 output matrices passed
the check, max error 3.285e-7. Relative to 16:9, the horizontal projection factor decreases to
0.75, and the mathematical horizontal coverage grows by 4/3;
this is still not proof of a correct wide image on the screen.
[Trace](../logs/acv-native-linux/20261024T120037853774Z/projection.json),
[machine result](../logs/acv-ultrawide-20261024/aspect-trial-check.json).
GDB child exit 0; the probe exit 1 keeps the earlier optional system.properties error;
shm orphans 0 before/after. The two runs are not used as an FPS benchmark.

The actual hook object passed 30 host boundary cases: the variable disabled/valid/invalid in the environment,
the selected/foreign caller, the original aspect and the FOV range. A single call to the original,
the restoration of f2 and the preservation of the returned r3 were verified.
[Test source](../logs/acv-ultrawide-20261024/hook_boundary_test.cpp),
SQLite operation `d6fd7504-54ba-438a-8db6-424f891e74db`.
This is a check of the wrapper boundary with a stub of the original, not a parity check of the whole game.

**Not accepted:** affiliation with the main gameplay camera; agreement of culling, crosshair, HUD, menus,
auxiliary projections and the wide viewport/output. The experiment changes one projection with the
previous output geometry, so it is not published as a ready UltraWide. The launcher setting is not yet enabled.

## Full-screen test 2560×1080 — 2026-10-24

At the user's request, a 120 s X11 run of the isolated host `e3ab704…` was performed,
FIFO/VSync, cap 60, Bilinear 8-bit, `ACV_EXPERIMENTAL_SCENE_ASPECT=64/27`.
After correcting an erroneous `--letterbox=false` to the existing SDK cvar
`--present_letterbox=false`, the visible window 2560×1080 and the output rect
`x=0,y=0,width=2560,height=1080` were confirmed; the source remains 1280×720. In the scene the
bounded override at 0x822B450C fired. The first run with bars 1920×1080, x 320
was rejected and ended before the retry. The canonical config was not edited.

The runtime finished with 0 at the 120 s limit; the probe exited 1 because of the earlier optional
system.properties. Shm orphans 0 before/after. The XE_SWAP intervals gave windows of about
15.5–60 FPS; the late heavy scene ≈ 15.5–15.9. This is not a controlled benchmark,
and a stable 60 FPS is not confirmed. The user's visual assessment has not been received yet;
HUD/crosshair/culling and the absence of distortion are not accepted. ACV-Linux
was not replaced by this experimental build.
[Result](../logs/acv-ultrawide-2560x1080-20261024/result.json),
[run](../logs/acv-native-linux/20261024T120920332499Z/launch.json).

## TRG frontend and installation — 2026-10-25

Latest local candidate: `../../ACV-Linux-TRG-20261025/start.sh` (185 manifest files).
The previous `../../ACV-Linux/start.sh` Qt package is unchanged. Earlier Qt evidence in this
document describes that checkpoint, not the current frontend. Runtimef3600aca remains a candidate;
no RUNTIME_BASELINE promotion or full recovered-source claim is made.

[Frontend source](../tools/acv-trg-launcher/main.cpp), [bridge](../tools/acv-trg-launcher/backend.py),
[build/reproduction](../tools/acv-trg-launcher/README.md),
[pinned dependencies](../tools/acv-trg-launcher/dependencies.json): C++17, TRG b384a28c,
Dear ImGui1.92.9b, SDL2 release2.32.10, standalone target and OpenGL3.3.
The six Ember pages retain the existing Python schema, typed values, UUID/XUID,
profiles/backup/restore, presets, capability restrictions and atomic writes. Unsupported high FPS,
FSR2/3 and YCbCr remain unavailable. The UI caches snapshots and runs CLI jobs off the UI thread.
Save/PLAY waits for backend validation; acv.config is not rewritten. Last-run now reads the actual
acv_paths.LOGS/XDG directory and bounded live runtime telemetry instead of a hardcoded development path.
Actual/requested output, depth fallback, cadence, game elapsed/idle time and session exit are shown.
Decisions and errors append to XDG state/acv/logs/acv-trg-launcher.log.

Checks:

- Real SDL/X11 smoke renders all six pages with a resize and a Cyrillic glyph availability assertion;
  host and relocated package/font smokes pass. Physical controller/HiDPI remain untested.
- [Development GUI integration](../logs/acv-trg-integration-20261025-r2/verification.json): selected
  executable/runtime/GPU/FidelityFX mappings, --session without --seconds, profile writer rejection,
  normal GUI exit while game stays alive and keeps its inherited lock, release after game exit.
  The first test asserted lock release before the supervisor finished draining logs; the test now
  waits for actual completion. Startup/cadence is not gameplay-speed acceptance.
- ASan/UBSan process boundary checks cover literal argv, independent stdout/stderr, simultaneous
  large streams/stdin, EPIPE, missing executable, timeout and game survival after owner destruction.
  Backend tests cover typed save/rejection, unchanged prior file on failure, reset preserving data,
  backup/restore. Native tests include fatal-log accounting and live/completed session telemetry.
- [Actual ISO import](../logs/acv-trg-disc-import-20261025/verification.json): production TRG reader
  and install boundary, title4E4D0853, all26 payload hashes match
  [game manifest](../tools/native-rexglue/game_data_manifest.json), original ISO SHA unchanged,
  existing install rejected and cancelled staging removed. Synthetic XDVDFS tests cover exact
  extraction bytes and wrong title; Python tests cover mismatch, symlink rejection and no-replace.
  Publication calls renameat2(NOREPLACE), so an existing installation is preserved. A silent nonzero
  verifier initially appeared successful; an explicit failure message fixes that case and CTest passes.
- The package contains copied game/runtime/backend files, DejaVu regular/bold fonts and notices,
  no Qt/ICU or symlinks into the development tree. Manifest185 hashes and ldd resolution checked;
  follow-up integration uses cwd `/` and isolated XDG state. The first relocated launch exposed
  a hardcoded package/tmp/acv-system-root; the empty system: mount now lives in the selected profile
  cache, with parents created. This is a packaging/path fix, not guest filesystem behavior emulation.

The normal game writes its own separate stdout/stderr evidence through probe_linux.py; the outer
supervisor log is a regular file, so GUI closure cannot SIGPIPE the session. The GUI stops only its
own supervisor; normal PLAY is unbounded. Raw reports remain under logs/acv-trg-*.
Earlier candidates ACV-Linux-TRG-candidate and ACV-Linux-TRG are superseded local checkpoints.

Final artifact checkpoint: [checkpoint.json](../logs/acv-trg-final-20261025-r2/checkpoint.json),
[verification.json](../logs/acv-trg-final-20261025-r2/verification.json), operation
9a0da1c6-e609-43f1-aa98-7767debec825. Launcher SHA526269c5…, runtimef3600aca…;
manifest SHA c397227d734217dfe0f3d02580944cf66f5cac010f4cb013c0ffb96d027d1077.
This exact package ran from `/`, closed by WM_DELETE_WINDOW while its game retained the profile lock,
then the owned test supervisor stopped the game normally: child0, fatal0, one user_language,
profile/runtime XUID equal, no new shm orphans. No package/tmp was created; system: is in profile cache.
The first final test used focus-dependent synthetic Escape during another GUI smoke and timed out;
direct WM close removed that test dependency. Final host suite:39 native Python cases,2 bridge cases,
2 CTest binaries pass.185 package hashes resolve; source tools/config and original userdata are checked.
The requested windowed2560×1080 was constrained by the desktop work area to2560×1052,
with a16:9 output1870×1052 atx345. This is correct aspect fit, not the earlier full-screen UltraWide test.
Startup swap cadence≈60, p95≈16.706ms; this does not establish gameplay speed.

Open: interactive installation UX, physical controller/HiDPI, real save/load and the different-mission
selection, visual CAS/FSR, native UltraWide HUD/culling, high FPS timestep, actual10-bit display
acceptance and other Linux distributions. The saved-name trial is still an experiment, not a migration
accepted for personal data. No gameplay or runtime-baseline acceptance follows from these host tests.


### WaitFix candidate — 2026-10-25

New package: ../ACV-Linux-TRG-WaitFix-20261025/start.sh,185 files. Runtimeca04c1c6 fixes fractional WaitMultiple sleep; Audio Worker78.05%→0.56% of one core.190s child0/fatal0. Stable60FPS remains open. Manifest/dependencies verified, no Qt/ICU. Earlier packages and saves retained.

WaitFix package X11 frontend/probe smoke PASS: cwd /, selected local libraries, no Qt/ICU, game and writer lock survive GUI exit, lock releases after game exit; child0/fatal0. [Checkpoint](../logs/acv-waitfix-package-smoke-20261025-r2/checkpoint.json).

## TRG package with wide_view and the refreshed runtime — 2026-10-27

- New local package `../ACV-Linux-TRG-WideView-20261027/start.sh` (185 manifest files, 3.9 GB with the owned game data,
  no Qt/ICU). Built by `tools/acv-launcher/standalone.py --frontend trg --include-game-data` from the accepted build
  directory; older packages (`ACV-Linux`, `-TRG`, `-TRG-WaitFix`) are untouched.
- Runtime: `librexruntime.so` `ebf338a181f3...`, GPU plugin `141540635406...`, FidelityFX lib, host ELF `6cd54abf0b01...`
  (wide-view and census hooks, default off). The accepted runtime was rebuilt from the current SDK tree (`build_sdk_ffx.py`)
  because the one staged on 2026-10-24 predated the thread-join, TimerQueue and WaitMultiple fixes. Features reported:
  `ACV_XUID cas_additional_sharpness fidelityfx frame_limit_fps present_output_bits wide_view`. Baseline re-recorded
  (`history` keeps the previous hashes; old libraries in `tmp/acv-backups/pre-runtime-refresh/`).
- New in this package: the "Расширенный обзор (эксперимент)" selector on the Screen page (needs an explicit resolution and
  the experimental-settings switch); 16:10 and wider outputs via the same hooks.
- Checks (through the project logger): `verify_package.py` 185 files, libraries resolve, no Qt/ICU;
  `verify_frontend.py` (60 s, isolated profile; evidence `logs/acv-trg-wideview-20261027/`): launcher exit 0, game survived
  the launcher, profile lock survived and was released after the game, second writer rejected, session without time limit,
  pacing 22 FPS in the sampled window. Scope: frontend integration/startup only, no gameplay acceptance.
  Regression run on the same runtime (scripted keys, 100 s, `20261026T195337406828Z`): no stall messages,
  pacing 60 FPS in light scenes; the user confirmed that the game works.


## Selected integration and FOV/HUD refinement — 2026-10-27

Accepted FOV/HUD package: ACV-Linux-TRG-FOVHUD-20261027 (`../../ACV-Linux-TRG-FOVHUD-20261027/start.sh`, исторический пакет; каталог сейчас отсутствует),
185 manifest files, TRG without Qt/ICU. Prior WideView and Integrated packages
are preserved. Exact hashes and evidence:
[checkpoint](../logs/acv-fov-hud-refine-20261027/checkpoint.json).
ELF6c02660d/runtimeebf338a1/GPU8fb12565/FFX0933159a.

The user accepted parallel readback copying. start.sh now reproducibly exports
ACV_GENERAL_DEFERRED_READBACK=1 and ACV_PARALLEL_READBACK_COPY=2; the separate
GPU-poll backoff remains disabled. RUNTIME_BASELINE records these artifact
requirements and preserves previous records.

CAS and FSR1 are integrated with their sharpness controls and FidelityFX library.
**No CAS/FSR effect sessions/tests were run in this iteration, at the user's
request.** The settings suite was skipped and the omission is recorded in the
package manifest; ordinary package/profile/process checks remain recorded.
Visual quality is unverified; FSR2/3 remain unavailable.

FOV/HUD: raw retail822B52D8 proves HUD bounds are derived from the viewport.
The hook now derives padding from those bounds instead of requiring1280x720.
Only the measured HUD site and native16:9 rectangles qualify. Symmetric padding
keeps the centre and HUD proportions. Original helper inputs restore on both
normal return and C++ exceptions. Perspective logs now identify each actual LR;
previous hardcoded822B450C diagnostics were incorrect for82300978. Active HUD
calls are also included in the optional read-only census.

Actual-source host ASan/UBSan tests cover only target2560x1080: both camera sites,
unchanged vertical FOV with wider horizontal coverage, equal physical x/y scale,
HUD centre1280x540 and centred1920x1080 safe area, foreign passes, invalid bounds,
return values and exceptions. Other resolutions were not tested per user.
World-projected target markers and complete visual/HUD/culling parity are not
established by those mathematical checks.

Final package35s X11 session20261027T150014447328Z: child0/fatal0/runtime_errors0.
Surface/output2560x1080, x0/y0, bilinear; wide aspect64/27 and HUD hook active.
No gameplay FPS conclusion. Package manifest/dependencies PASS185files.
Earlier same-pipeline Integrated package frontend check confirms launch from cwd/,
correct library mappings and inherited accepted environment, game survival and
profile lock after GUI exit, release after shutdown; child0/fatal0.
[Frontend evidence](../logs/acv-selected-package-20261027/frontend/verification.json).

Colour integration remains RGB application output with actual surface reporting.
The separate2560x1080 RGB10-bit request20261027T145115528083Z offered only
BGRA8 UNORM/SRGB and explicitly fell back to8bits. Physical DP/HDMI encoding is
system controlled and was not inferred from VkFormat. YCbCr output has no
verified backend and remains unavailable. No additional colour/mode work after
the user's instruction to focus on FOV/HUD.

Prior user scope before the streaming request: package + FOV/HUD, integrated CAS/FSR without tests, existing
colour capability limits. FPS benchmarking, save/mission regression,
controller/HiDPI work and other-resolution tests are skipped by user instruction.
Protected config/save hashes unchanged; all owned game/build/test runs ended.


## Streaming settings candidate — 2026-10-27

New separate package (`../../ACV-Linux-TRG-Streaming-20261027-r2/start.sh`, исторический пакет; каталог сейчас отсутствует),185files,
TRG without Qt/ICU, runtime544bdaae/GPUfe4550d5, unchanged ELF6c02660d and
FFX0933159a. Full hashes in [checkpoint](../logs/acv-resource-streaming-20261027/checkpoint.json).
Includes cache soft/hard/retention and archive preloading controls, streaming
preset, custom resolution, observed modes and recovery validation.
Accepted FOVHUD and RUNTIME_BASELINE unchanged. Candidate environment is explicitly
selected by repeated --candidate-env KEY=VALUE, strict allowlist, and records
accepted_baseline=false; no accepted status is inferred from tests or overrides.
General deferred1, parallel-copy2, GPU-backoff0 preserved in start.sh.

Manifest185 hashes/dependencies, six-page UI smoke and45s frontend proof from cwd/
pass: local libraries, inherited environment, game/profile lock after GUI exit,
second writer rejection, orderly shutdown and lock release. CAS/FSR suite skipped
as requested. Two210s isolated probes pass; loading-time gain/model lifetime
correction remain open. [Details](RESOURCE_STREAMING_20261027.md).


## ACV-Linux-release-1.0 — 2026-10-27

User-requested release folder: [ACV-Linux-release-1.0/start.sh](../../ACV-Linux-release-1.0/start.sh).186 manifest files,
4163370903 bytes (~3.88GiB), current TRG launcher, copied game data and libraries,
version1.0 in acv-package.json/manifest.json; RELEASE_NOTES.md records scope/limits.
Based on the verified Streaming-r2 runtime544bdaae/GPUfe4550d5,
ELF6c02660d/FFX0933159a. General deferred1 / parallel-copy2 / GPU-backoff0 retained.
Original destination was empty; only its empty directory was removed before
assembly. Previous packages, accepted baseline and original user data preserved.

Manifest/dependency verification passes186files, no Qt/ICU or build-tree links.
Real relocated launcher/startup evidence and final hashes:
[release checkpoint](../logs/acv-release-1.0-20261027/checkpoint.json).
CAS/FSR tests skipped as requested. Release naming does not establish new
mission/FPS/image/loading-time acceptance; previous documented limits remain.

Release1.0 exact-folder45s startup test passes from cwd/: selected local libraries
and readback environment, GUI exit preserves game/writer lock, second writer
rejected, orderly shutdown releases lock. Session20261027T160134261574Z,
child0/fatal0. Requested windowed2560×1080 constrained to2560×1044 by work area;
startup integration only. Gameplay FPS and resource-lifetime acceptance unchanged.


### User-confirmed native-frustum patch — 2026-10-28

Existing [release1.0](../../ACV-Linux-release-1.0/start.sh) now contains
ELFefa4756c; runtime544bdaae/GPUfe4550d5/FFX0933159a retained.
patch_level2026-10-28-native-frustum,186 manifest hashes/dependencies pass.
User confirmed the UltraWide edge fix;160s final runtime child0/fatal0 and both
camera paths eligible=patched. Previous ELF/metadata have a checksum-verified
backup; baseline records exact accepted artifacts and limited scope.
[Checkpoint and scope](../logs/acv-engine-offscreen-20261028/checkpoint.json),
[engine explanation](ENGINE_OFFSCREEN_20261028.md). CAS/FSR effects untested.

## Interface polish — 2026-10-28

The current release entry is [start.sh](../../ACV-Linux-release-1.0/start.sh).
Patch level is `2026-10-28-native-frustum-ui`; this updates the launcher and its
Python bridge/schema messages, keeping the accepted ELF/runtime/GPU/FidelityFX.
[Publication and hashes](../logs/acv-launcher-polish-20261028/publication.json),
[previous files and hashes](../logs/acv-launcher-polish-20261028/release-before/sha256.json).

- `preview` uses the existing schema without saving. The UI submits it after
  400 ms without changes, ignores stale results and refreshes available options
  and the selected executable's existence. Experimental settings no longer autosave.
- Refresh keeps the draft. Saved settings are tracked separately; the UI shows
  unsaved edits and offers explicit confirmed revert. Graphics reset updates only
  draft graphics, retaining launcher edits and profile data.
- Profile selection fills name/gamertag/pilot. Backup results have their own state,
  so session polling cannot overwrite the list. Switching profiles clears the
  previous profile's selected backup. Empty name/path actions are disabled.
- Native file/folder pickers cover ISO, executable, backup and import. Manual
  path entry remains available. A button selects the bundled game, and another
  opens logs. CAS/FSR sharpness controls follow the selected effect.
- User-facing capability reasons no longer mention PLAN filenames or internal
  stages; unavailable FPS/colour/temporal-FSR choices remain unavailable.

Validation: Python `test_draft_preview_is_read_only` and `test_backup_restore`
passed (`e249ab2c-e1eb-40a9-b61b-1dd3e1f34aef`). The real App with its actual Python
boundary passes `ctest -R '^interface$'` (`1b7114f7-4d03-42bb-845a-3c0915be5963`):
preview does not write, refresh retains edits, explicit save clears dirty,
profile fields are filled, backups preserve session data and graphics reset
retains launcher edits and saved bytes. All six pages render with Cyrillic and
nonempty draw data (`1b1e8f28-89ec-4512-ba32-73eefd5f86c7`), without screenshots.
Package verification checks all 186 manifest records and library resolution
(`86b9bf78-1ef0-413f-8cd6-0d24b38b32c6`).

The desktop file chooser's interactive selection has not been manually accepted.
CAS/FSR effects and other game resolutions were not tested. These checks do not
establish gameplay, visual or save/load acceptance. Protected original config/save
and personal settings hashes are recorded and retained.

The exact updated release also passes the 35-second frontend→bridge→game check
(`1eb08978-61a1-4ccf-a0b8-3bb5be9a0bfd`), session `20261028T174646977657Z`:
child 0, fatal 0, accepted environment and package libraries, game/profile writer
lock survive GUI exit, lock released after game. Requested 2560×1080 window is
clamped by desktop work area to 2560×1044; this is startup integration only.
Test uses isolated data and bilinear output, without gameplay profiling or CAS/FSR.
All owned test game processes were completed. Protected hashes and backup copies
were checked again after the test.

## Launcher options — 2026-10-28

Current patch: `2026-10-28-launcher-options` in the existing release1.0.
[Publication hashes](../logs/acv-launcher-options-20261028/publication.json),
[previous files](../logs/acv-launcher-options-20261028/release-before/sha256.json).

- The process now initializes UTF-8 LC_CTYPE before SDL creates the window.
  SDL X11 calls XmbTextListToTextProperty: the previous C locale produced a
  mojibake WM_NAME and no _NET_WM_NAME. The actual old/new title oracle confirms
  both corrected properties equal `Armored Core V — Лаунчер`, even with LC_ALL=C
  inherited (`67575242-7847-462c-bc82-73ea125866d1`). No system locale changes.
- Executable help identifies bin/game/acv_linux_probe and the bundled-game button.
- Shared FPS choices are30/60/120, default60;144/165 removed. Existing runtime
  supports the cap directly. This controls presentation pacing, without changing
  guest timestep or guaranteeing120FPS in gameplay; the help explains VSync.
- Shared presets include16:10, UltraWide and32:9, up to7680×2160. Existing FOV/HUD
  hooks derive aspect from explicit WxH within1.2–4.0;16:9 keeps native view.
  Presets are output choices, not proof the monitor supports fullscreen modes.
  Other resolutions were not launched or visually accepted.
- 10-bit remains an experimental SDR request with explicit8-bit fallback.
  Selecting10 bits disables incompatible8-bit dithering; the colour page shows
  actual depth/format from the last session. It does not claim HDR or a10-bit
  physical link. Current hardware's true10-bit output remains unverified.
- FSR2/3 removed from the shared effect choices. Bilinear/CAS/FSR1 remain;
  no CAS/FSR effect tests were run.

Three focused Python checks pass (`793d1a4c-5ec6-4183-b327-2263d91d17d6`),
including30/120 save/mapping and10-bit request/capability handling. Actual App
regression passes (`ae8f6598-169a-49a5-bc21-9f948884e335`); all six pages render
with Cyrillic/nonempty draw data (`97c17fcb-7994-414e-9fe3-be943ae9dc17`).
Accepted game binaries and personal config/save/settings remain unchanged.

Exact release startup sessions with30 and120 limits each pass35 seconds, child0/fatal0,
including frontend/profile lock lifecycle: `20031d11-bd15-410b-aa90-b89637397fc9`
and `5ddd2794-0863-4552-9ee5-7a4d5459b6b4`. Logs confirm pacing limit30/120.
Observed startup presentation was about29/60FPS respectively;120 is a configured
ceiling, not evidence of120FPS gameplay. Tests requested2560×1080, desktop work
area clamped the window to2560×1044. Bilinear only; no gameplay profiling or CAS/FSR.
All186 manifest entries/dependencies verify (`0aeac84f-dd8e-41c2-85c2-bc8520c8078e`).

## Game window title — 2026-10-28

The game window now displays `ARMORE CORE V [Rexglue]` exactly as requested.
Canonical host OnCreateDialogs/OnPostSetup hooks update the visible title while
retaining internal app/config/storage name `acv_linux_probe`. Incremental host
build passes; codegen writes0files. Actual X11 title assertion and35s frontend
startup pass (`dca190cb-4ae8-4031-9b0f-1eb952b7901d`, child0/fatal0), with existing
profile-lock lifecycle retained. This is title/startup verification, not gameplay.
The same tested ELF is installed in release1.0; runtime/GPU/FFX and protected
config/save/settings hashes are preserved. Previous ELF/metadata have verified
backups. [Checkpoint](../logs/acv-game-window-title-20261028/checkpoint.json).
