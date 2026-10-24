# Standalone ACV Linux package — 2026-10-24

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
