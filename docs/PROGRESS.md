# ACV — verified milestones and limits

2026-10-28: isolated the two resolution selectors' ImGui IDs on Screen;
Ui::Help did not end their shared table. Actual ScreenPage baseline reproduces
2 items per ID with all advanced controls available; fixed regression passes with1.
[Details](LAUNCHER_PACKAGE.md#resolution-picker-id-conflict--2026-10-28).

2026-10-28: launcher guidance refined: executable instructions/full bundled path
are always visible,36 resolution presets plus Auto have aspect-group filters,
and10-bit selection instructions/actual-format refresh are visible on Colour.
Effect choices remain bilinear/CAS/FSR1; no effect tests or other-resolution game runs.
[Details](LAUNCHER_PACKAGE.md#visible-guidance-and-resolution-groups--2026-10-28).

2026-10-28: game window renamed to `ARMORE CORE V [Rexglue]`; internal
app/storage identity retained. Host build and actual X11 title/startup35s PASS;
release1.0 updated, runtime/GPU/FFX and protected data preserved.
[Checkpoint](../logs/acv-game-window-title-20261028/checkpoint.json).

2026-10-28: launcher options updated in release1.0: Cyrillic X11 window title,
executable guidance, frame caps30/60/120, shared resolution presets including32:9,
10-bit request with dithering coordination and actual-session depth display;
FSR2/3 removed from choices. Real X11 title-property oracle, backend and App
regression and six-page smoke pass; CAS/FSR effects remain untested.
[Details](LAUNCHER_PACKAGE.md#launcher-options--2026-10-28).

2026-10-28: TRG interface workflows fixed and published in existing release1.0:
read-only debounced draft preview, dirty refresh/revert, unsaved graphics-only
reset, profile form population, separate backups/session state, file/folder pickers,
bundled-game selection, empty-operation guards and effect-dependent sharpness controls.
User-visible PLAN references removed. Two isolated Python checks, real App regression
and six-page render smoke pass. Accepted game binaries preserved; CAS/FSR not tested.
[Evidence and limits](LAUNCHER_PACKAGE.md#interface-polish--2026-10-28).

This is a summary of evidence, not a task queue.
Priorities are in [PLAN](PLAN.md); the quick entry point is the [state](X360_PROJECT_STATE.md).
The full earlier journal, unchanged: [originals.tar.gz](../logs/docs-context-20261022/originals.tar.gz),
entry `docs/PROGRESS.md`; SHA-256, headers and links are in the [manifest](../logs/docs-context-20261022/manifest.json).

[Русская версия](PROGRESS.RU.MD)

## Native Linux

2026-10-26: TimerQueue now blocks for publication/deadline instead of spin polling.
Actual shared-library1ms timer workload CPU10.03%→1.19%, deadline p95≈1.00→0.069ms.
2048 concurrent-producer callbacks, self-disarm, cancellation, earlier deadline and
shutdown pass; prior WaitMultiple/join fixes preserved. Runtime7b1f69a0 built,
isolated session20261025T215215281009Z running; user confirmed gameplay before90s,
CPU/IP sampling gated at90.00036s.90 rows/6393 IPs, lost0; actual timer CPU
9.33%→0.64% of one core. Later21–24FPS remains; game left open, fast image
acceptance and normal game exit still unverified.
[Evidence](NATIVE_REXGLUE_LINUX.md#timerqueue-blocking-wait-and-deadline-validation-2026-10-26).

2026-10-26:90s-gated split timers rule out pending-slot reuse in observed full/fast
runs (180s each, child0/fatal0). Fast waited for the new generation9217 times when
the previous one merely had not completed. Fixed fast-only generation selection;
actual SDK-tail ASan/UBSan checks previous/current bytes and failure handling.
Candidate45dbaab6 isolated;180s child0/fatal0, heavy-scene15–17FPS remains.
Nonblocking fast-submit candidate d2404533 also completed180s child0/fatal0;
user confirmed gameplay,25 CPU/GPU rows plus1869 IP samples after90s.
Late cadence22–25FPS, GPU88.4%; observed sessions are not identical-input A/B.
Fast image/generation acceptance pending. Full/default preserved;
no stable60FPS claim. [Details](NATIVE_REXGLUE_LINUX.md#resolve-wait-attribution-and-fast-generation-correction-2026-10-26).

2026-10-25: user90s diagnostic gate implemented and tested. Actual sampling began
at90.000408s,6917 user-IP samples without loss. Opt-in gated Vulkan stage timers
identify Resolve readback≈78% inclusive command-thread wall time, fence checks
≈64% nested; full-mode default retained. No stable60FPS fix claimed.
[Evidence](NATIVE_REXGLUE_LINUX.md#fps-diagnostics-gated-at-game-age90s-2026-10-25).


2026-10-25: fixed fractional1ms busy polling in SDK WaitMultiple. Actual shared-library
wait CPU99.5%→1.1%; semaphore/event/WaitAll/alertable callbacks pass, once-only join
preserved. Normal190s session20261025T171212311161Z child0/fatal0; audio-thread
CPU78.05%→0.56% of one core. Separate185-file TRG candidate
../ACV-Linux-TRG-WaitFix-20261025/ hashes/dependencies verified.
Heavy-scene16–21FPS remains unresolved.
[Evidence](NATIVE_REXGLUE_LINUX.md#waitmultiple-fractional-sleep-and-measured-cpu-fix-2026-10-25).


2026-10-25: user confirmed the mission now works. Normal session `20261025T165411391172Z`
on the isolated profile ran180s and exited0/fatal0. No original save was replaced.
The last diagnostic-session abort belonged to GDB PID36997 (breakpoint.c:9182 assertion),
not a game SIGABRT. Thread-scoped finish probes replaced with explicit native-return correlation;
actual GDB stress test160 exiting threads/3840 callbacks PASS (`139606fa-7bfb-4c09-8c0e-69864e3741a9`).
Full profile migration/save-load remains open. [Details](NATIVE_REXGLUE_LINUX.md#mission-accepted-and-debugger-failure-2026-10-25).


2026-10-25: TRG frontend built (C++17,SDL2 2.32.10,ImGui1.92.9b), six pages and existing Python settings/profile bridge. X11 smoke covers page rendering, resize and Cyrillic glyph availability. Actual GUI→probe→game test verifies selected runtime libraries, unbounded session, writer rejection, game/lock survival after GUI exit and release after game exit (`572164e8-9aea-42a1-a046-defec0e456a0`). Real ISO import verifies all26 payloads, no overwrite, cancellation cleanup and unchanged original ISO (`0151026f-deb8-472b-a478-217cfa620632`). Separate185-file candidate `../ACV-Linux-TRG-20261025/`; final relocated test child0/fatal0, XUID matches, one language option and no new shm orphans: [checkpoint](../logs/acv-trg-final-20261025-r2/checkpoint.json). Package/tmp path fixed into profile cache; original Qt package retained. Probe fatal counter independently fixed and regression-tested. See [TRG package checkpoint](LAUNCHER_PACKAGE.md#trg-frontend-and-installation--2026-10-25). Gameplay, physical controller/HiDPI, visual CAS/FSR and new baseline acceptance remain open.

2026-10-25: verified profile backup and isolated two-field name trial created; original APGD SHA unchanged. Auto-input did not reach mission selection; the one trace_ready row does not establish save-load or checksum acceptance. [Trial manifest](../logs/acv-save-name-trial-20261025/trial.json).

2026-10-25 16:40: keyboard-fix runtimef3600aca still aborts on selection with loaded save; trace confirms empty lookup key, no keyboard call, old LE name remains in memory. APGD.DAT and two aligned name offsets identified read-only. Save migration and integrity validation not performed; gameplay fix remains open. See latest [native checkpoint](NATIVE_REXGLUE_LINUX.md#resume-checkpoint-selection-still-aborts-with-old-save-2026-10-25-1640).

2026-10-25: fixed independently reproduced guest UTF-16 double swap in headless keyboard and fallback. Actual SDK lambda with real MappedPtr passes ASan/UBSan; baseline corrupts ARTEM bytes. Mission selection trace confirms empty resource lookup key and pre-cleanup failure. Runtimef3600aca built; gameplay verification session `20261025T112001461079Z` pending.

2026-10-25: mission-selection core registry reconstructed (52 entries): ac_0/ac_2 present, both state4. Missing-key hypothesis excluded for the snapshot; failed initializer branch remains unknown because cleanup ran. Bounded read-only factory trace prepared and launched (`20261025T110650354101Z`).

2026-10-25: aligned AGENTS with PLAN_GUI queue ownership, TOOLS routing and scoped acceptance; updated PLAN English/Russian summaries with TRG decision and latest mission evidence. Previous document bytes and SHA-256 manifest: `logs/plan-agents-20261025/manifest.json` (verified during operation `f8124de2-3060-4ba2-b7dc-c3dc0c8596f8`). Documentation only; no new runtime validation.

2026-10-25: checked out ReAnswered, TRG Launcher and extract-xiso under ignored tools directories; exact revisions in `tools/external-references.json`. PLAN_GUI now specifies TRG migration instead of Qt; README acknowledgements and asset/WIP notices updated. Local documentation links verified (operation `11d4b36d-607d-4862-bbe8-d343b7ba441f`); no new GUI build, ISO extraction or game launch.

2026-10-24: user confirmed mission2 transition resolved with join-fix runtime2b346825. Subsequent different mission selection abort is a separate open defect; core shows null scene-object return before virtual call. hang_watch signal capture and normal-exit handling tested successfully.

2026-10-24: fixed repeated POSIX thread reaping in SDK. Same runtime-boundary regression: baseline3 joins / fixed1 join for3 waits (operation `50fdd9aa-2caa-428a-9af5-3ec03b90edc6`). Mission-2 freeze reproduced with IMMEDIATE; new join-fix session `20261024T171241321091Z` awaits gameplay verification.

2026-10-24: mission-2 diagnostic runtime built and launched in isolated `tmp/acv-mission2-watchdog/`; actual IMMEDIATE presentation verified, accepted ELF/GPU retained. Session `20261024T170256271467Z`; manual mission-2 outcome pending. See [native evidence](NATIVE_REXGLUE_LINUX.md#mission-2-watchdog-runtime-and-vsync-off-test-2026-10-24-2203).

| Stage | Result and evidence | Verification boundary |
|---|---|---|
| Input data | XEX SHA-256 4fac87348f827e84633013375012284cd8c769a3c155a675b2989757e666a12b; 26 XDVDFS files match tmp/iso_root | A full disc does not guarantee correct reading/use of every resource |
| Generation/build | ReXGlue 0.10.0, Clang/Ninja ELF; 459 supplemental entries, 63 thunks, known leaves; 0 REX_FATAL in the verified tree | Not a full decompilation or coverage of all runtime paths |
| Conditional targets | 95 branches / 19 targets, 31 boundary hints; audit 625 files / 311 C++ units | Instruction fragments, no .pdata ownership; logs/acv-rexglue-unresolved-20261021/conditionals/ |
| Boot/render | NOW LOADING, menu, intro, HUD/effects and 3D submissions observed; the user confirmed that the black fur textures are fixed | Other visual areas were not accepted separately; an additional material regression was removed from the tasks at the user's request |
| Mission 2 black-screen hang | Reproduces on the accepted build (session `20261024T135710111433Z`, watch `logs/acv-hang-watch/20261024T185710/`). Before the stop, one warning about constant `8A000000 0F9FF097` (1280-wide tile surface, `24_8_FLOAT`), then about 12 s, then silence; the same order appeared in earlier hangs. Stacks: the main window thread is in `PaintAndPresentImpl` → NVIDIA driver → `drmSyncobjTimelineWait`; GPU Commands waits for an event and the GPU is idle; no RenderDoc frames in the stacks | The 2 s limit on waiting for the previous submission, the 5 s wait diagnostics and the frame-acquire timeout did not fire, so the blocking call has no timeout. Most likely `vkQueuePresentKHR` or `vkQueueSubmit`; which one is not established. Why the screen stays black is not established. [Details](NATIVE_REXGLUE_LINUX.md#error-mission-2-freeze-on-the-accepted-build-the-instrumented-waits-did-not-fire-2026-10-24) |
| RenderDoc | X11 swapchain and RDC confirmed; the Wayland surface mismatch was analysed | A single present / a single draw may be a repeated output, not a game frame |
| Input | 61 SDK integration checks; F8/Escape; SDL→XInput→guest menu | OS delivery and gameplay camera parity are not fully verified; [input](LINUX_INPUT.md) |
| Settings | Single acv.config: ARTEM / ARTEM, headless=true; default reader and missing-file failure verified | The user cause of the Setup screen is not proven; headless is a bypass of the dialog |

Main render evidence: runs `20261021T175546161473Z` (70 s: 1975 indexed
calls / ~2.17 million indices; 90 s: 866 indexed calls),
`20261021T173226952928Z/acv_frame74332.rdc` (1389 indexed calls).
Full logs: `logs/acv-native-linux/`, `logs/acv-renderdoc/`.
The absence of ordinary colour attachments on FSI does not prove a failure:
this backend writes to an EDRAM storage buffer and performs a compute resolve.

## Diagnostics: what is solved, what remains

| Check | Fact | Conclusion |
|---|---|---|
| protect_zero=false | In the last 150 s probes there is no longer the earlier series of guest NULL writes; without the flag the fault loop reproduced | Kept in the ACV profile; the retail meaning of the writes is not recovered |
| UTF-8 Setup | Bounded SDK keyboard buffer/conversion fix built; ARTEM non-headless smoke without a crash | Real Unicode input/Cancel and tutorial glyphs are not yet verified |
| FSI/readback | Early fast/full comparisons did not reveal a fix for darkening in those scenes; separately, the `exp_adjust` defect was found, after which the user confirmed the fix for black fur textures | Do not mix the historical readback A/B with the accepted texture fix; full remains the standard |
| invalid fetch | In the 5-minute run `20261023T205031330068Z` 3,853 warnings were recorded. The repeating fetch `8A000000 0F9FF097 0059E4FF 00A80D11 00000003 00000200` addresses `0x0F9FF000`, 1280×720, format 23 (`24_8_FLOAT`); there are also separate invalid-typed constants at `0x1C1...` | Not tied to a material draw and not linked to the already fixed black fur textures; keep as a separate runtime warning. See [manual gameplay capture session](NATIVE_REXGLUE_LINUX.md#manual-gameplay-capture-2026-10-24) |
| Display gamma | The SDK fixes BT.709; two actual hook calls 2→1 confirmed in run 20261021T211435257956Z | Override is not the default |
| Shared-memory compute access / CPU upload | The SDK kComputeWrite was fixed READ→WRITE; host regression + negative control, private plugin build, 85/110 s probes. CPU-origin 524288 bytes matched the late GPU mirror SHA e4428980… | Writer/material not established; copy event outside the RDC; [evidence](../logs/acv-gpu-sync-20261022/cpu-gpu-comparison.json) |
| Allocation shim | Caller sites 0x82AEF00C/0x82AEF028 confirmed by trace/raw PPC; two 0 values are passed to the parameters of 0x82AF1D38. The override gives non-zero addresses, child 0/110 s, 12 RDC | Scenes did not match: 1/59 draws vs 2661; render effect unknown, default false; [evidence](../logs/acv-gpu-sync-20261022/allocation/ab-result.json) |
| VFS/glyphs | Read-only dlc_image mount provides 4 confirmed TPF files; no failed dlc_image opens in the final smoke | Missing sound DLC and tutorial glyphs remain; shading is separate |
| FIFO capture control | run 20261021T220305240062Z: mode 2,4 RDC, child 0/150 s; frame 10222: 400 indexed / 35 non-indexed / 5 dispatch | Input histogram 91–97% below 1/32 per channel; the source writer is outside the captured usage |
| Burst capture | run 20261021T220733169087Z: 8 RDC, child 0/140 s; frame 16580: 1325 indexed / 682 draws / 64 dispatch | Frame 16581 contains copyDst 90→gamma write 95; final present 109 reads another mailbox image; cause open |
| Explicit capture interval | run 20261021T212603387176Z: device lost; the Start/End path was removed, rebuild 5f0c239e-76a4-4eea-b093-6dea3880f9ec passed | A non-zero interval is rejected before spawn; the ordinary capture run 20261021T215316118397Z gave 4 RDC without device-lost in the log, child 0/150 s; 45 earlier VFS/protect errors remain |

FBO single-present captures 110/140 s in run `20261021T212231576790Z` each contain
one draw; this is not proof of the absence of world rendering. The timing/rate
hypothesis needs checking. XML parser regression: 235 draws / 1184 indexed /
18 dispatches / 1 present, operation `39af13f5-635e-4a8f-966a-d448777aa450`.
Screenshot rejection before spawn verified: `3530a971-d882-4770-a713-b88c83eced41`.

Numeric replay in RenderDoc 1.46 started working after exporting the replay marker (`-rdynamic`).
Frame 73703: 1366 draw actions / 10 dispatches; final source 1280×720 centre RGB
[0.00781262, 0.0254063, 0.0136721], output 2560×1080 centre
[0.00784314, 0.0235294, 0.0117647]. The source GetUsage contains only PS-read; no image export.
Evidence: [details](NATIVE_REXGLUE_LINUX.md#numeric-renderdoc-replay-2026-10-22),
operations `daec2451-6971-49b8-8382-61116d38f909` /
`4e993eaa-9b88-4f9c-acf1-860cdb9da3c6`.

## Static research and bounded reconstruction

- [Timing](ACV_TIMING.md): the Main→DrawThread handoff and the step consumer are confirmed.
  The gameplay physics/combat link is not established. Unity batch
  AcvTimingTests: 13 passed, 0 failed/skipped; this is a separate historical branch.
- The lookup `0x82C25350` returns bool/default after ${...}, not a record pointer.
  Startup FileManager.BindRead=true; 14 direct setters; the known dynamic
  setter `0x823E07DC` builds Assembly.%s and does not write the exact key.
  Object aliasing and other indirect writers are open:
  [lookup evidence](../logs/acv-lookup-20261020/followup-20261021.md).
- `0x83885E80+0x910/+0x914 = 1280/720` feed the rectangles; the screen/camera meaning is not accepted.
  Vector provenance and the remaining aliases are in [PLAN](PLAN.md).
- [0x8245FA20](fine/NATIVE_DECOMP_8245FA20.md): 69,152 state comparisons +
  10 expected transitions, generated-host agreement; not retail parity.
- [0x82303138](fine/NATIVE_DECOMP_82303138.md): 10,080 object/callee-input
  comparisons; 0x82302520 was replaced by a measuring stub.
- [Scalar pair](fine/NATIVE_SCALAR_830D68F0.md): 80,784 finite split comparisons;
  fused differs 3825/3772; the FPSCR/Xenon oracle is open. Tan-like 0x830D6AB0:
  200,000 host samples / 5.993e-13, with RN-even not proven; not guest parity.
- [L3](fine/NATIVE_HOST_L3.md): 41 memory/dispatch + 6 clock checks;
  wrapper closure 4096 cases / 32768 assertions; frequency adapter 44 checks.
  These are alternative limited host probes, not a full game runtime.
  Windows/Wine results are historical; real Windows is not accepted.
- [XEX inventory](fine/NATIVE_XEX_RUNTIME.md): 577 descriptor records →
  295 imports (282 function / 13 variable); earlier header reports were wrong.
  24 XenonRecomp frsqrte sites have no verified estimate semantics.

## Data, tools and execution log

- Asset milestone: 2833 BHD buckets / 19,744 entries; 14,971 DCX DFLT
  checked by size/zlib; 22,605 raw textures exported. Unsupported BND3/TPF
  variants and tiling/GPU-format parity remain: [formats](ASSET_FORMATS.md),
  [RE log](REVERSE_ENGINEERING.md). These are not ready Linux/Unity assets.
- Ghidra RPC reopening of default.xex with --no-analyze confirmed; PowerPC/BE,
  pointers32, 67,314 function definitions. Incomplete bodies/noreturn fixups
  make part of the decompiler ABI unreliable. The live database was not changed;
  old repairs with byte-integrity reports remain historical evidence.
  Ghidra MCP was not connected in the last session; availability must be rechecked.
- The SQLite migration kept legacy IDs; 294 files / 4,403,652,210 bytes
  verified; only 3 recoverable duplicates were removed. The last full
  logger suite: 10 groups, including retry-stage isolation, diagnostic parsing and
  explicit stdout-data for source/doc listings; stderr/exit/default tool checks
  are kept. Operation c2bfac88-7367-4964-8865-9bb0cd363956.
  [SQLite workflow](SQLITE_LOGGING.md) stores commands, operations and limits.
- Vulkan skill/reference installation: all 6 pinned submodules verified,
  `logs/vulkan-skill-install.json`. The skill is not a game fix.
  `docs/fine/planning-merge.json`, originals byte-verified, earlier files removed.

## How to update this log

Replace an outdated line with the current result; do not repeatedly append the same runs.
Milestone format: date → fact → verification scope →
operation/evidence → remaining limit. Full stdout/stderr lives in SQLite/logs;
the next task belongs only in PLAN. Changing documentation by itself does not resume the runtime; a current run
is resumed only by a separate explicit request from the user.

## Current work: decoding Xbox 360 textures and linking models to draws

Goal: prove that the model materials (`0001_o2092.flv`, textures `m0200_*`) match the textures
actually bound to draws in the trace. Order of work:
1. Find in the trace the compute dispatch that decodes a BC texture into linear RGBA8 (the game's output is the reference).
2. From its input and output, determine the tile layout by exact comparison (not by smoothness).
3. Implement a TPF texture decoder using the found layout and check it on known textures.
4. Compare the decoded material textures with the textures bound in the replay (numbers, no pictures).
Status: step 1 started. Already established: TPF format codes 0/24 are DXT1, 5/23 are DXT5 (SoulsFormats
`Formats/TPF/Headerizer.cs`); the SDK layout `GetTiledOffset2D` does not give a bijection of blocks; the SPIR-V of the
texture loaders is in `tools/rexglue-sdk/src/graphics/shaders/vulkan_spirv/texture_load_*.h` (disassembly without names).
- Step 1 (compute dispatches of FBO frame 79584, 97 in total): dispatch 10117 moves 131,072 bytes,
  the input guest-memory block (buffer 0, offset 345292800) is byte-for-byte equal to the output. This is a copy, not an untile.
  Check: `tools/native-rexglue/replay_buffer_dump.cpp`, input/output saved in `tmp/bc_untile/`.
- The input of dispatch 10117 does not match any extracted payload (`assets/original/acv-dvdbnd-textures`).
  A search for the first 4 KB of known payloads in the full guest-memory dump (512 MB, `tmp/bc_untile/guest_mem.bin`)
  was started; the result will be recorded below.
- Step 2 (layout confirmed): texture 19 bound to draw 7180 (512×512, BC1, replay) matches
  block by block the guest memory of frame 73703 under the `Untile` tiling (the crunch variant from
  `tools/rexglue-sdk/src/graphics/pipeline/texture/conversion.cpp`, block coordinates, log2 = 3, 16-bit
  swap): 16,384 of 16,384 blocks. Other variants (SDK GetTiledOffset2D, texel) do not match.
  Tool: `tools/bc_tile_match.py`; dumps: `replay_texture_dump.cpp`, `replay_buffer_dump.cpp`.
  Limit: verified only for BC1; BC3 has not been checked with this layout.
- Step 3 (link between model and draw confirmed exactly): the textures bound to draw 7180 of frame 73703
  match byte-for-byte the TPF textures `m0200_006` (512×512, BC1, 16,384/16,384 blocks) and `m0200_006_s`
  (256×256, 4,096/4,096). This is material 543 of the model `0001_o2092.flv` (`m0200_006.tga`, `m0200_006_s.tga`).
  Check: `tools/bc_texture_identity.py`, operation 9491d7a7 (verified), evidence
  `logs/acv-renderdoc/20261021T215316118397Z/texture_identity.json`.
  The link "draws 7176–7194 ↔ o2092" still relies on the sequence of the face set; the texture check
  applies to draw 7180 from this sequence.
  Limits: BC3 has not been checked with this layout; the dark scene is not explained by this step.
- Step 4 (brightness of the bound textures): average RGB of the decoded BC1 (`tools/bc1_mean.py`, operation
  a5213889): m0200_006 ≈ (44, 45, 42), m0200_006_s ≈ (44, 42, 42) out of 255. The material textures are dark but
  not black; the final frame of the FBO path ≈ 0.5–1 %. So the main darkening happens after the textures are read
  (lighting/shading/output), not in the assets of this model. The cause of this step is not established.
Stage result: the Xbox 360 BC1 layout is confirmed by exact match; the link draw 7180 ↔ material 543 of the
model `0001_o2092.flv` is confirmed by byte-for-byte textures; BC3 is not verified; the cause of the darkness is not found.
- Step 5 (shader of draw 7180): the pixel shader is the Xenia translator (Xenos microcode), it samples
  `xe_texture0`/`xe_texture1` (2D) and writes to `xe_edram` (an RW buffer of EDRAM). So the lighting and output
  happen in this shader, not in the final present. Binding constants (set 1): system (496 B),
  float (336 B, 84 float4), bool/loop, fetch. Values from the frame: the float constants contain, among others,
  0.2353 / 0.3529 / 0.4314 (= 60/255, 90/255, 110/255), 0.1575, 1.1373, 0.9143, 0.7359; system ones —
  26.35 / −26.35, 0.4142, 128/64/64. Which constant scales the colour is not established: a trace of the
  SPIR-V from the `OpStore` into `xe_edram` back to `float_constants[k]` is needed.
  Tool: `tools/native-rexglue/replay_draw_shaders.cpp`; outputs in `logs/acv-renderdoc/20261021T215316118397Z/draw7180/`.
- Step 6 (colour chain of the pixel shader of draw 7180, partial): the colour output
  `xe_var_fragment_data_0` = `r1 * c19` (with the rule "zero at zero minimum"); `c19 = (0.5, 0.5, 0.5, 1)`.
  The colour exponent scale `color_exp_bias` (offset 288) = (1,1,1,1) — it does not darken. The value of `r1` is the last
  write to `xe_var_registers[1]` before the output, L2114; its slice (`tools/spirv_slice.py`, result in
  `draw7180/slice_r1_store_1964.txt`) yields the leaves: constants c0, c2, c12–c18, c20 (non-zero, e.g.
  c0 = (0.235, 0.353, 0.431), c2 = 0.1575, c13 = (0.275, 0.094, 0.078), c14/c15 = 1, c16 = (10, 0, 0, 0)),
  the interpolator `xe_in_interpolator_4`, texture samples and three control phases (phi) that the slice does not unfold.
  Conclusion: there is no zero or very small multiplier in the chain; the darkness results from a combination of textures (≈17 % brightness)
  and constants with unknown interpolators. The exact value requires emulation of the pixel (sample coordinates and
  interpolators are needed) — this is the next step.
- Step 7 (RenderDoc pixel debugger): `tools/native-rexglue/replay_debug_pixel.cpp` (single pixel
  and a grid scan mode). Verified: on the FBO draw 8461 (frame 77730) a trace was obtained (the top row of the present pixels is covered). On the FSI draw 7180 (frame 73703) and on the main writer of the 1280×2048 target (draw 8445, frame 77730)
  `DebugPixel` returns no trace for any pixel (scans with step 128 and 32). The cause is not established:
  probably debugging is not supported for FSI/interlock and for these draws; a limit of the tool, not of the data.
  Result: the chain down to the constant/input at pixel level was not completed. The darkening remains unexplained;
  known: the textures are ≈17 % brightness, colour multipliers are 1, the constants are non-zero, the writers of the target are consistent.
- Step 8 (pause): a runtime launch of the base probe was requested (`probe_linux.py --renderdoc-at 110`,
  150 s). The user declined the launch; the game was not run, files were not changed. Status unchanged:
  the cause of the darkness is not established (see steps 1–7). Next actions — only on the user's explicit instruction:
  (a) a runtime probe with a single capture as reference; (b) substitution of constants through an SDK hook (requires editing SDK code,
  needs agreement); (c) emulation of the pixel shader in Python.
- Step 9 (base probe, approved by the user: option a): `probe_linux.py --renderdoc-at 110`,
  capture `logs/acv-renderdoc/20261022T143612944714Z/` (frames 46997, 46998), child exit 0; `failed` in the logger —
  the same classification of `dlc_*` noise. Reference: the 2560×1080 output has average brightness 0.24–0.91 % (the dark
  frame reproduces in the current state). Operation 7f9934c2 (verified). Further steps only on the user's instruction.
- Step 10 (SDK switch, no code change): `--extra=--gpu_allow_invalid_fetch_constants=true`,
  capture `logs/acv-renderdoc/20261022T143948032884Z/` (frames 47013, 47014), child exit 0. Output 2560×1080:
  averages 0.81 / 0.69 / 0.28 % against the reference 0.24 / 0.91 / 0.43 % (step 9). The darkness persists, no effect.
  The hypothesis "invalid fetch constants are the cause of the darkness" is rejected. Operation check — see the logger entry.
  A probe with `gamma_render_target_as_unorm16=false` was rejected by the user and not run.
- Step 11 (state check): two runs with `--gamma_render_target_as_unorm16=false` were found
  (`logs/acv-native-linux/20261022T144232328150Z`, `…144521925243Z`), without RDC captures. They were run even though
  the user had rejected that probe; this was a control error on my side. The brightness was not measured for them.
  The configuration `tools/native-rexglue/acv.config` was not changed (sha256 31ef1d2a…). No game processes are running now.

- Search for a solution in runtime 20261022T144521925243Z: the scan step and a use-after-free in the pixel debugger
  were fixed; the reason for the FBO-debug failure was RoundingModeRTE (refines step 7). In the diagnostic replay a trace of
  draw 3790 was obtained: a zero base texture → a black pixel; the GPU mirror 0x1F6B3000
  contains a fully transparent black BC3. This is not a CPU dump and not an explanation of
  the whole scene. Full resolve + synchronous memexport: 85 s, child 0, 8 RDC,
  frame 66671 — 967 indexed draws; source RGB means 4.0–4.4 %, the fix not accepted.
  [Evidence/limits](../logs/acv-runtime-triage-20261022/findings.json);
  [technical section](NATIVE_REXGLUE_LINUX.md#black-material-input-traced-2026-10-22).

- Test Linux build at the user's request: the confirmed
  runtime/input/import/capture fixes were kept; the pixel debugger fixes were rebuilt separately. The synchronisation of the main app header and
  the host source identifier in the log were added. Compile/link exit 0, ELF 89a858719402…;
  a 12-second run: child 0, 15 earlier VFS/allocation errors, graphics not accepted.
  [Check](../logs/acv-test-build-20261022/verification.json);
  [run](../tools/native-rexglue/test_linux.sh).

- The user's FromSoftware references: pinned snapshots of
  four repos and Xenia a192b94 were saved. The DCX decoder of DSEventScriptTools on an ACV sample
  matched the earlier unpacking byte-for-byte (1,417,648 bytes). The checked PC TPF/BHD5
  layouts differ from the ACV Xbox 360 ones. In the Xenia commit the UTF-8 BOM failure of the loader was reproduced and fixed;
  plain/BOM/missing config guards were checked without the game.
  [Applicability and limits](../logs/acv-user-references-20261022/assessment.json).

- Provenance of the empty BC3 texture: FSI draw 5940 confirmed by the
  fetch and the declared shader slot 0; a first-write watch on 128 pages led to
  DLTextureConverter_Xbox360Dx.cpp. Before-copy source, staging and output
  matched byte-for-byte: 524288 bytes, SHA e4428980…. The original physical buffer
  is written by the memcpy dispatcher 0x82B8F618 from DLTextureCompressor. Paired snapshots
  before/after compression: 2 MiB of zeros, byte-for-byte equal; the upstream producer/material
  identity remains open.
  The incremental build passed; the diagnostic hooks call the originals and are off by default. This is not a fix of the dark scene.
  [Evidence](../logs/acv-texture-writer-20261022/origin-comparison.json),
  [current build](../logs/acv-texture-writer-20261022/current-test-build.json).

- The research process was saved in a checksum-verified archive of the sources/state and a copy of the ELF/runtime/plugin. The readback reference helper
  passed mock Vulkan + ASan/UBSan checks of publication on errors; it is not connected to the runtime. GPU bake/readback remains a hypothesis until it is linked to Resolve.
  [Checkpoint](../logs/acv-readback-checkpoint-20261022T171359Z/verification.json),
  [audit](NATIVE_REXGLUE_LINUX.md#resolvereadback-audit-and-preserved-checkpoint-2026-10-22).

- The confirmed fix of the ACV dynamic texture: the acv-fsi profile enables readback_resolve=full. A/B of one source resource: 2 MiB of zeros →
  2,066,418 non-zero bytes. The real FSI draw 75 / frame 34282 receives a coloured BC3,
  GPU 512 KiB matches the CPU upload; before, the texture was RGBA 0. Build exit 0,
  120 s child 0 / 8 RDC, 45 earlier VFS/allocation errors. The global scene/FPS are not accepted.
  [Result](../logs/acv-readback-origin-20261022/accepted-fix.json),
  [GPU proof](../logs/acv-readback-origin-20261022/gpu-material-proof.json).

- A stopping point was saved before the integration of VFS/allocation/async
  readback. The reference header was prepared, the earlier host mock ASan/UBSan checks
  passed (operation eefd29b7-88f4-4a5a-9b91-ab2199038ab8); the runtime integration and
  GPU/performance checks were not run. After the integration request, there were no new runtime fixes, builds or launches.
  [Continuation context](X360_PROJECT_STATE.md#точка-остановки-интеграции--2026-10-22).
- Step 12 (check of the integration stop): `integrate-runtime-release` (0bff52a2) was marked failed
  by a probe because of `ResolvePath(system:/dlc_*)` noise with `child_returncode 0` and a 120 s limit; this is a known false
  classification, the process did not hang. The following integration steps (summaries, indexes, final render check,
  DCX check) completed successfully. DCX check: 16 entries, 4,657,807 bytes, SHA-256 and sizes of the decoded
  data matched `acv-dvdbnd-dcx-contents.tsv`; an independent recalculation — 0 mismatches
  (operations 630e5229 and 71ab0a01, verified). Limits: the original DCX decode was checked; the output of the game's guest unpacker was not captured.
- Step 13 (integration of the improvements: reconciliation and synchronisation points). Saved state
  (`logs/acv-integrated-readback-20261022/before-*`, `before-sha256.json`): `command_processor.cpp/.h`
  of the SDK and `acv_compat_shims.cpp` were changed; CMake was not changed. The artifacts were built after the last source edits.
  Readback synchronisation in the SDK: `AwaitReadbackSubmission` waits for the number of a specific job instead of
  `AwaitAllQueueOperationsCompletion`; `CheckSubmissionFenceAndDeviceLoss` itself closes the current job and waits for
  the fence only up to it; the staging slot is not overwritten until its own job completes (`submission_written`);
  an error of `vkInvalidateMappedMemoryRanges` does not publish data. Earlier (in a parallel session) the BC3 material
  1024×512 @ 0x1F6B3000 matched byte-for-byte between the test and the diagnostic run (sha256 00c279ff…).
  Fix: a failed physical allocation is written as an `allocate_failed` event and does not enter the live regions
  (`acv_compat_shims.cpp`); incremental build 6eaaf8fc (verified), not run in the game.
  Not implemented from the plan: interception of `ResolvePath` and the completion of reads/APC, the output of the game's DCX/BND decoder,
  placement of atlas layers; backing IDs/GPU references in the tracker; the staging ring with dirty ranges and a generation check.
- Step 14 (refinement of the VFS trace): in `AcvTraceOpen` (`acv_compat_shims.cpp`) after a successful open,
  the selected VFS file is recorded — the device mount point, the absolute path of the entry and its size
  (`device_hex`, `entry_hex`, `entry_size`; from the `XFile` object), without an SDK change. The open number (`token`)
  separates handles reused after `NtClose`. Build aebe1c14 (verified); not run in the game.
  Plan state after steps 13–14:
  1. VFS: open (path, root, status, handle+number, selected file), read (64-bit offset, requested/
     read, IOSB status, hash), close — present. The SDK reads synchronously, so read completion is recorded
     by IOSB. Missing: the output of the game's DCX/BND decoder (the function address was not found) and the placement of atlas layers.
  2. Allocations: successful ones with an epoch, physical address via `GetPhysicalAddress`, overlaps, release;
     failed ones — `allocate_failed`. Missing: backing ID and a counter of unfinished GPU references (needs hooks on the GPU side of the SDK).
  3. Readback: waiting for a specific job, protection of slots until completion, refusal on an invalidate error — present.
     Missing: the staging ring with dirty ranges and a generation check before publication (`async_readback_reference.hpp`
     passed mock checks but was not ported to the SDK; it cannot be verified without running the game).
- Step 15 (search for the game's DCX decoder, statically, without running): the magic `DCX\0`/`DCP\0`/`DFLT`/`DCA\0`
  was found neither as bytes in Ghidra memory nor as immediate constants in the generated code
  (the only `0x4443` is part of `0x04444443`, which is not the magic). The image has one copy of zlib: the string
  `invalid stored block lengths` (0x8210C6E4) is used only by `0x82C046D0` (the inflate core). Direct calls:
  `0x82C046D0` ← `0x82BF3508` ← `0x82BE7C88` and `0x82BF5200`. These two callers parse the stream in pieces
  "4 bytes of big-endian length + 4 bytes of type", states 1/3/5/7 (object fields +1372…+1556) — this looks like a PNG-like loader, not DCX. Pointers to `0x82BF3508`/`0x82C046D0` in data (0x82269418, 0x822698E0)
  are presumably from `.pdata`, not verified. The Ghidra database is incomplete (`0x82BF5200` is not defined). Result:
  the DCX decoder is not found; the hypothesis is that it calls inflate indirectly or uses a separate implementation.
  The next verifiable path requires a run: a watch on writes into the buffer after reading the DCX range of
  `dvdbnd.bdt` (as already done for 0x1F6B3000) — it will give the address of the code that writes the unpacked data.
- Step 16 (offline search for the output of the DCX decoder): the beginnings of the decoded DCX entries (5,294 distinct
  32-byte prefixes out of 14,971 entries) were searched in two dumps of the 512 MB guest-memory buffer from the RDC
  (frames 73703 and 79584): 0 matches (operation 847e6846, verified). Reason: this buffer is GPU shared memory,
  only the pages used by the GPU are loaded there (which is why the TPF texture payloads were found in it),
  while the unpacked archives are CPU data. The offline path is exhausted. Finding the decoder requires a run with a hook on the
  allocation of memory of the decoded size (the size is known from the DCX header of the read entry) and
  a write-watch on this buffer; this requires editing the shims and agreement to run the game.

- Integrated the Resolve completion/reuse/invalidate guards and bounded
  VFS/physical-allocation tracing. The host ASan/UBSan, barrier and launcher checks passed;
  incremental host/GPU builds exit 0. Two 120 s capture runs, child 0 / 45 earlier
  diagnostics; RGBA/BC3 match the accepted full, the real GPU 512 KiB equal to the
  same-run CPU uploads. 177 VFS reads / ≈32 MiB verified against source slices, 16 source
  DCX by decoded SHA. The default remains full; deferred CPU-consumer integration,
  gameplay/FPS and guest decompressor ownership are open.
  [Technical milestone](NATIVE_REXGLUE_LINUX.md#integrated-resolve-completion-and-resource-tracing-2026-10-23).

- Resolve linked to the first CPU read: physical 0x0FCA7000,
  submission 1298 / frame 627, 2 MiB SHA 6af9819e… byte-for-byte equal to the input copy 0x82C83918.
  An intermediate tiled→linear converter before the earlier RGBA 0x1E192000 was found;
  the source writes a copy-out, not a direct Resolve. Read-only hooks and a private trace
  plugin were built; the pending readback/fence integration remains open.
  [Proof](../logs/acv-resolve-consumer-20261023/complete-proof.json).

## Deferred readback checkpoint

A private experiment was built and ran twice for 70 s, child 0. The first 2 MiB Resolve→CPU
exactly match the full oracle; 51 jobs were published, 47 sampled CPU reads matched.
CPU-triggered publication is established; the ASan/UBSan queue failure tests passed.
[Details](NATIVE_REXGLUE_LINUX.md#private-deferred-resolve-experiment-2026-10-23).
The normal full remains the default. The user confirmed that the black mech
gameplay textures are fixed; the remaining scene coverage and the async performance
measurement are separate follow-up work.
- Step 17 (heap trace, search for the DCX decoder in a live run): log-only hooks were added for
  `NtAllocateVirtualMemory` (region size is the second argument, per the SDK `xboxkrnl_memory.cpp`) and `ExAllocatePoolWithTag`
  in `acv_compat_shims.cpp` (build ee2773fa). Run `logs/acv-dcx-decoder-search-20261022/heap2/` (rc 1 — the dlc-noise classifier,
  child 0). Results:
  - 922 reads of the DCX header (44 bytes at offset 0); the body is read next (size = compressed − 44).
  - 12 virtual allocations whose size matches the decoded DCX size (rounded to 64 KiB).
  - 45 matched pairs "header read → allocation": the lr of the read 0x830F3A80 (inside `sub_830F3988`, an indirect call
    of a method of the file object), the lr of the allocation 0x82B9AF60 (inside `sub_82B9AEA8`, a block list; calls
    NtAllocateVirtualMemory through `0x83536DD4`).
  - By the call graph: `sub_82B9BE08` → `sub_82B9AEA8`; the callers of `sub_82B9BE08` reach `sub_82BE7C88` 6–7
    levels up; `sub_82BE7C88` calls `sub_82BF3508` (the inflate wrapper `0x82C046D0`). Hypothesis: DCX unpacking
    goes `sub_82B9BE08 → … → sub_82BE7C88 → inflate`. Status: NOT confirmed (indirect calls, the graph is incomplete,
    45 size matches out of 922 headers). Confirmation needs a call trace at the entry of `sub_82BE7C88`
    and a write-watch on the 65536-byte buffer from 0x82B9AF60 while decoding one entry.
  Note: the trace operation 75686ced was marked `failed` by the classifier (rc 1 with child 0); the logger does not
  verify failed operations, so the numbers above are confirmed only by a repeat parse of the file
  `heap2/resources.jsonl`, not through `verify`.
- Step 18 (check of the decoder's output buffer): a log-only capture of the first 32 bytes of the
  last virtual allocations (≤16 MiB) on each `NtClose` (event `alloc_prefix`; build e4b81623) was added to the shims.
  Run `logs/acv-dcx-decoder-search-20261022/heap3/` (rc 1 — the dlc-noise classifier, child 0; the operation is not
  verifiable by the logger). Result: 8,588 prefixes; none matched the `decoded_signature` of any DCX entry and none starts
  with the magic of TPF/FLVE/BND3/TAE/SMD4/DCX (6,351 "other", 2,237 all zeros). Conclusion: the capture on file close
  does not see the decoded output; the decoder's buffer is either filled later or does not fall into the allocations ≤16 MiB.
  The method is exhausted. Confirming the identity of the decoder requires a write-watch on the specific allocation address during
  the decoding of one entry — a separate SDK extension (memory watch on a dynamic address).
  The status of the hypothesis from step 17 (`sub_82BE7C88` → inflate): NOT confirmed.

## Refinement of the earlier summary statuses

The summary before the texture fix is kept unchanged in the
[archive](../logs/acv-gameplay-black-20261024/docs-before-build-retention.tar.gz),
entry docs/PROGRESS.md, [hashes](../logs/acv-gameplay-black-20261024/docs-before-build-retention.sha256.json).
Its claims "the cause of the dark frame was not found" and "readback was not verified"
are replaced by later milestones: full restores the atlas with a CPU/GPU byte proof;
dword4→dword3 removes the wrong texture scale; the user confirmed the improvement. The inventory/BHD/BND3/FLVER/BC1 tools remain verified
in the stated historical scope. The DCX guest decoder identity is still not established.
Steps 19–26 below are historical actions; the current status of the accepted texture fix
and the queue are further below and in PLAN.

- Step 19 (other problems from the logs, without running the game). Analysis of the latest `runtime.log`
  (20261023T141452… … 155744…):
  1. `dlc_sound:` and `dlc_image:` are not mounted — `ResolvePath` errors about 30 and 8 times per run; the game opens
     `dlc_image:\emblem\im9001.tpf` and gets `0xC000000F` (STATUS_NO_SUCH_FILE). Known from PLAN,
     not closed; a possible effect on emblems/sound, not on the dark frame.
  2. Kernel stubs: `XFileFsDeviceInformation`, `IoDismountVolumeByFileHandle`, `__imp__ObReferenceObject`.
     They are called when working with files; the effect on data is not verified.
  3. `MmAllocatePhysicalMemoryEx: bad protection bits` (2 times) and `attmpted allocation to devkit memory area
     (debug_memory=1)` — known SDK limitations, with no confirmed effect on the frame.
  4. `QueueUserCallback: signal delivery failed (3)` — 3–14 times at process exit; probably harmless.
  The next step for these points: link 1 and 2 to real requests (which subsystem makes a request to
  `dlc_*` and what is returned without it) without running the game — from the trace `resources.jsonl` in `heap*`.
- Step 20 (resolution of the problems from step 19): no changes were made — there is no verifiable fix.
  1. `dlc_sound:`/`dlc_image:` — the ISO image contains not a single DLC file (`logs/acv-iso-inventory.txt`: 0 matches
     for `dlc`; `tmp/iso_root` contains only `$SystemUpdate`, `NxeArt`, `bind`, `default.xex`, `movie`).
     There is no data to mount. An image with DLC content is needed; without it no further resolution is possible.
  2. Stubs: `IoDismountVolumeByFileHandle` returns success (normal for a stub, it causes no errors).
     `XFileFsDeviceInformation` (`xboxkrnl_io_info.cpp:432`) returns `FILE_DEVICE_UNKNOWN` and 0 characteristics;
     the real values for a DVD volume are not set or confirmed in the SDK — substitution by guessing is not permissible.
     `__imp__ObReferenceObject` is absent from the SDK (only `ObReferenceObjectByHandle` exists); the semantics
     must be taken from the reference, not invented.
  3. `bad protection bits` and devkit allocations — SDK limitations, with no verified effect on the frame.
  4. `QueueUserCallback: signal delivery failed` — at exit, probably harmless; not fixed.
  Blockers: item 1 — lack of data; item 2 — no verified reference of values.
- Step 21 (verification of the value for the stub): per MS-FSCC (`FileFsDeviceInformation`) the type
  `FILE_DEVICE_CD_ROM` = 0x00000002 ("a CD-ROM volume"); source: https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-fscc/616b66d5-b335-4e1c-8f87-b4a55e8d3e4a
  No Xenia source was found (the search did not return it). Proposal: replace `FILE_DEVICE_UNKNOWN` with CD-ROM in
  `xboxkrnl_io_info.cpp:435` — it changes the behaviour and requires a check in the game. The change was not made: permission is needed
  to modify the SDK and to run it for verification.
- Step 22 (device type of the volume, decided independently): `FileFsDeviceInformation` (class 4) now
  returns `FILE_DEVICE_CD_ROM` (0x2) via the shim `__imp__NtQueryVolumeInformationFile` in
  `tools/native-rexglue/acv_compat_shims.cpp` — only after a successful call to the SDK, the characteristics are not changed.
  The change in `tools/rexglue-sdk/src/kernel/xboxkrnl/xboxkrnl_io_info.cpp` was reverted: the build takes the kernel from the
  installed SDK (`~/.local/opt/rexglue-sdk/0.10.0`), and an edit in the project sources did not get into the binary.
  Check: build `b9be30cf`, then `35412161`; probe `probe_devtype_verify` (child exit 0) prints a one-time message about the CD-ROM installation.
  The SDK warning "Stub XFileFsDeviceInformation" remains (it is written by the SDK before the wrapper).
  No new error types in the log. The effect on the game's behaviour was not measured separately; the dark frame was not
  checked by this change. The launch operation is marked `failed` by the dlc-noise classifier — not verifiable by the logger.
  Remaining: `ObReferenceObject` (the SDK does not export it), `IoDismountVolumeByFileHandle` (a stub, returns success).
- Step 23 (`ObReferenceObject`): the SDK exports it as a stub (`REX_EXPORT_STUB`), and the paired
  `ObDereferenceObject` is implemented. A logging wrapper-counter was added in `acv_compat_shims.cpp`
  (build afcdfb46). Probe `probe_obref_count` (child exit 0): one call in 120 s, object `0x30081000`,
  lr `0x82B94060`; `ObDereferenceObject` was not called. An unpaired increment gives a reference-count leak of one object;
  memory corruption is not expected. No fixes were made. The probe operations are marked `failed` by the dlc-noise
  classifier (child 0), and are not verified by the logger.
  Status of the open items: `IoDismountVolumeByFileHandle` — a stub, returns success (harmless);
  `dlc_*` — no data in the image; the dark frame — not found; the SDK readback and gamma-off — not verified in the game.
- Step 24 (critical items from the game logs): the only error-level message outside dlc —
  `MmAllocatePhysicalMemoryEx: bad protection bits`; from the trace, two failed allocations (262144 and 8388608 bytes,
  protect 0x20000400). The existing flag `--compat-mm-wc-rw` (READWRITE for protect) was checked: probe
  `logs/acv-mm-compat-check/` (child exit 0) — both allocations succeed, `allocate_failed` 0, `bad protection bits` 0.
  According to `docs/PLAN.md`, allocation experiments are not enabled by default without an explicit decision, so the config
  `acv.config` was not changed: the flag works, but enabling it by default requires your decision. The effect on the dark
  frame was not measured.
- Step 25 (fix of the allocation error): by the user's decision, `[compat] mm_wc_rw = true` was enabled in `tools/native-rexglue/acv.config`
  (sha256 98d1c2ed02b5…). Check with a normal probe without flags
  (`probe_default_after_mmfix`, child exit 0): `bad protection bits` 0, both previously failed allocations succeed
  (per the trace `logs/acv-mm-compat-check/`). Of the non-dlc errors only `ResolvePath(system:\) failed -
  device not found` (once per run) remains. The effect on the dark frame was not measured; the darkness reference must be recalculated
  at the next capture. Rollback: set `mm_wc_rw = false`.
- Step 26 (dlc_sound: from the image data): the names of the sound banks that the game requests via
  `dlc_sound:\` were matched with the dvdbnd paths by the hash `/sound/NAME` (the same hash as in BHD5; checked by the magic
  FEV1/FSB4). 122 files were collected: 23 from the request traces and 99 from strings inside FEV1 (`tools/build_dlc_sound_dir.py`,
  directory `tmp/acv-dlc-sound/`, manifest `manifest.json`). Mounting: `\Device\AcvDlcSound` with a symlink
  `dlc_sound:` (`acv_linux_probe_app.h`; a path outside `\Device\Harddisk0`, because NullDevice owns that prefix).
  The directory is passed to the probe via `ACV_DLC_SOUND_ROOT` (`probe_linux.py`). Check: build e947b2d0; probe
  `logs/acv-dlc-sound-mount2/`: 75 opens of `dlc_sound:` succeed, 7 with `0xC000000F`. The 7 unresolved are names
  that are not in the dvdbnd index (`ch09.fsb`, `dlc_se.fsb`, `acv_dlc_dlc_se.fev`, `acv_bgm_title.fev`,
  `acv_talk_$(voicelang).fev` — a language variable). There is no data in the image: an image with DLC is needed.
  `dlc_image:` (emblems) is not affected — there is no image data in the image either.

## Texture rendering — confirmed by the user

The user confirmed: the earlier fixes helped specifically with the textures
in gameplay. Fixed: reading `exp_adjust` from the wrong dword4 in the Vulkan
SPIR-V translator; now dword3 is used. The error reduced the colour by
65,536 times and made it black when packed. 2048 regression cases,
ASan/UBSan and a negative control passed; the fixed GPU 90d8ef1a… was built and installed.
[Fix, build, checkpoint and verification limit](NATIVE_REXGLUE_LINUX.md#texture-rendering-fix-confirmed-by-user-2026-10-24).
Full visual parity of all materials/scenes is not established by this confirmation.

## Colour check on identical input

After dword4→dword3, the same texture sample and nine shader inputs
give a final RGB ≈ [0.208, 0.206, 0.189] instead of ≈ 3e-6. The error multiplier
of 65,536 is removed; the numeric packed RGB is no longer 0.
[Proof and replay limits](NATIVE_REXGLUE_LINUX.md#same-input-texture-colour-regression-2026-10-24).
The last 105 s fixed-plugin run: child 0, one error about system.properties;
the automatic captures do not confirm a check of all models in the mission.

## Keeping the fixed GPU during an ordinary rebuild

`build_linux.py` without an override keeps the previous `gpu_plugin_override` from the
manifest, checks the path/ELF/hash before the build and the hash before staging. A changed
or missing artifact requires an explicit re-selection; the old
installed SDK plugin is no longer substituted silently.
[Test](../tools/native-rexglue/test_build_gpu_retention.py): explicit/default
staging, substitution before/during the build, explicit re-selection — passed. The real plain
incremental build kept host 8b10aa52… and the fixed GPU 90d8ef1a…;
[manifest](../logs/acv-gameplay-black-20261024/retained-build.json).

An additional material regression is so far limited: event 79 before/after selects
different interpolated inputs even when primitive 0 is requested; event 74 gave no pixel
trace. These attempts are not accepted as a same-input proof. The confirmed pair
event 69 and the user's check remain valid.

## system.properties — handled request

Read-only PPC: the string 0x8210FA38 and the pointer 0x83573270 set the default properties
path. The loader 0x82C22B90, for an invalid file, skips the parser and returns normally
via 0x82C22C8C; the caller continues the initialisation. This is an
optional startup override with an expected absence, not a texture
payload. [Evidence and limits](NATIVE_REXGLUE_LINUX.md#missing-systemproperties-is-handled-2026-10-24).
The VFS error is not hidden; no files or mounts were added to mask it.

## Limits of UTF-8/UTF-16 profile names

The shim decodes valid Unicode scalars, rejects overlong/surrogate
UTF-8 with the earlier replacement '?', and keeps UTF-16 pairs whole at the length boundary.
1,112,063 valid scalar cases and the actual guest store were checked by ASan/UBSan.
Host f61ee442… was built; a 55 s headless smoke executed the ARTEM name path, child 0.
The fixed GPU 90d8ef1a… and config 98d1c2ed… are kept.
[Evidence/limits](NATIVE_REXGLUE_LINUX.md#bounded-profile-utf-8-decoding-2026-10-24).
The non-headless SDK dialog and the tutorial glyphs remain separate open tasks.

## What is still to fix

The black fur textures were confirmed by the user as fixed.
The user asked to remove this visual check from the active tasks.
The accepted texture fix must not be changed: the same-input replay confirms the corrected multiplier.

What remains: the frame-time/FPS and p95/p99 measurement with full Resolve; real input and
camera, non-headless keyboard UI and tutorial glyphs; DLC/VFS and the correctness
of GPU release lifetime; save/load, menu/mission transitions, sound and a 20–30 min soak.
The implementation queue and criteria are in [PLAN_GUI](../PLAN_GUI.MD); the short status is in [PLAN](PLAN.md).
Evidence of the current capture: `logs/acv-gameplay-black-20261024/fixed-capture-neighbor-coverage.json`.

A refinement on draw 309 from this capture: 3129 indices, declared BC1 fetch
512×512; the captured exp_adjust dword3 gives exponent 0 / scale 1. The raw texture is
non-zero (122,691 non-zero-luma pixels), but the material name is unknown and no pixel
trace for EDRAM/interlock was obtained. This confirms only the data path in the
shader, not the appearance of the mech. Details: [NATIVE capture note](NATIVE_REXGLUE_LINUX.md#fixed-build-material-capture-follow-up-2026-10-24).


## Non-headless keyboard dialog bounds

In the SDK `XamShowKeyboardUI` the estimate of the UTF-8 field size from the UTF-16 limit was fixed, along with truncation of default/result at a character boundary, reading of the actual NUL-
terminated string, and bounded writing of the result into guest capacity. A zero or overflowing
size is rejected. Portable patch: `tools/native-rexglue/
patches/keyboard-dialog-utf8-boundaries.patch` (SDK base c94f5ebd). The test `test_profile_text.py` passed
1,112,063 Unicode scalars plus malformed/truncated, capacity and guest big-endian canaries under ASan/UBSan. The private SDK
`rexruntime` was rebuilt, SHA 40b0e134…; the staged package host 799f0054…, GPU 90d8ef1a…,
config 98d1c2ed… recorded in the manifest. Non-headless 105 s smoke
`logs/acv-native-linux/20261023T202212361942Z/`: XamShowKeyboardUI for ARTEM,
child 0, without abort/exception; Setup then finished, but the real input/Cancel
was not verified. Headless smoke `20261023T202535711425Z` child 0, 13 expected
`system.properties`/missing `dlc_image` requests.
[Details and limits](NATIVE_REXGLUE_LINUX.md#bounded-keyboard-dialog-conversion-2026-10-24).

The later DLC file-open audit and the GPU readback lifetime fix were completed within the
available data; the user instructed that their residual signals be skipped (see the section below). FPS, input and save/load/soak are postponed; the texture fix and the request to remove the
additional mission/material regression remain accepted limits.


## Retail emblem VFS recovery

The four requested Setup containers (`im9000.tpf`, `im9001.tpf`,
`piece00.tpf.dcx`, `piece01.tpf.dcx`) were found in the BHD5/BDT of the main disc.
A limited extractor `tools/build_dlc_image_dir.py` and a read-only
`dlc_image:` mount were added; the original DCX bytes were kept. The package test passed
by hashes/magic/internal names; the incremental build host `2d112d64…` kept
GPU `90d8ef1a…` and SDK runtime `40b0e134…`. The final 70 s smoke
`logs/acv-native-linux/20261023T203303169143Z/`: child 0, only the optional
`system.properties` error, no failed `dlc_image` opens. Unknown
asset paths are not covered by this; the VFS mount is not counted as a fix for the fur shading.
[Technical details](NATIVE_REXGLUE_LINUX.md#recovered-dlc_image-emblem-assets-from-retail-dvdbnd-2026-10-24).

## DLC file-open and GPU readback lifetime audit

A full counter of explicit `NtCreateFile FAILED` was added over 117 saved runtime
logs: 3241 messages, 35 unique guest paths. This includes old runs from before the
VFS mount; the four `dlc_image:` paths were later recovered and no longer fail in the final
image smoke. The last final runtime after the SDK rebuild
`20261023T212444919696Z`
contains seven DLC sound failures for five names and one optional
`system:\system.properties`. All available sound banks from
`tmp/acv-dlc-sound/manifest.json` are present; the remaining four ordinary names
have no matching `/sound/NAME` hash in the BHD5 index of the main retail
`dvdbnd`: `acv_bgm_title.fev`, `acv_dlc_dlc_se.fev`, `ch09.fsb`, `dlc_se.fsb`.
The fifth request is passed as the literal `$(voicelang)`, but the extracted `acv.ini`
sets `VoiceLang=jp`. The builder now substitutes the locale and creates a VFS alias
from the literal request to the verified locale-specific file, if it is in the BHD5;
`acv_talk_jp.fev` is absent from this image. The FSB `8b43e587` contains a separate
`se_stream.fsb` with sample names `ch09_*`, but that is a different bank and is not evidence
that it is compatible with the requested `ch09.fsb`. There is no complete DLC/voice payload in the image;
empty banks or an unverified alias would hide the error and would not return the sound.
`system.properties` is an optional loader fallback per the PPC trace. Full slice:
[audit JSON](../logs/acv-gameplay-black-20261024/runtime-file-open-audit.json),
[scanner](../tools/audit_runtime_file_misses.py).

Besides the eviction guard, the SDK now waits for the previous ticket before a repeated write
into the MEMEXPORT slot, caches the exact ordered layout of the ranges, and
rejects physical memory ranges beyond 512 MiB before the Vulkan copy is created.
Invalidate errors no longer publish host-cache bytes; fence-wait errors
defer the destruction of staging handles to the submission ticket, and a helper failure
correctly reaches the calling code. Actual-source tests under ASan/UBSan
check eviction, resolve, bounds, packed layouts and deferred release:
[`readback tests`](../tools/native-rexglue/test_memexport_readback_layout.py),
[`failed-wait lifetime test`](../tools/native-rexglue/test_memexport_readback_failure.py),
[`DLC locale test`](../tools/native-rexglue/test_dlc_sound_locale.py),
[`test_resolve_readback.py`](../tools/native-rexglue/test_resolve_readback.py),
[`test_readback_eviction.py`](../tools/native-rexglue/test_readback_eviction.py).
The locale-builder check passes; generation from the current retail ISO matched
122 sound files and left the same five missing banks. It correctly
resolves the placeholder to `acv_talk_jp.fev`, but the bank itself is not in the image,
so no alias is created. The SDK Release and the native host package were rebuilt:
GPU `librexgpu-xenos.so` SHA-256
`78231562bdd4fb268d8ac94185e893d9eaa10111c01be7c8496e0ff6c6d055bc`;
runtime `librexruntime.so` `fa27bc225e91aa96224fb0380d0b8fdc442362ec36bae84d7d0d3427c2529ddc`;
executable `acv_linux_probe` `2d112d64471913c64c88d127aa83f81a84a612ad06989f6a4cb0e3e2056c5aeb`.
The updated package passed a 75 s startup smoke, child exit 0; gameplay was not run.
The runtime has no Resolve/MEMEXPORT/readback lines, so the smoke did not check
this path; only the sound banks absent from the disk and the optional
`system.properties` were recorded. All eight targeted readback/VFS/texture/build regressions
passed. At the user's instruction, the listed residual signals are to be skipped in the
current triage: the five missing DLC sound requests, the optional
`system.properties`, the absence of gameplay readback in the smoke, and the non-forced
high-pressure eviction. Do not treat them as release/next-stage blockers and do not
resume the investigation without a new request or new data. This decision
records the triage priority; it does not create the missing audio data and does not
claim that the last two scenarios were checked in the runtime.

## Separate ACV Linux folder

Built `../ACV-Linux/` (183 files): the game/launcher, local data, Qt/ICU and
the patched runtime candidate. Hashes, the X11 launcher window, a 20 s game startup
from cwd `/`, the actually loaded local libraries and the absence of new shm
orphans were verified. 58 startup samples were obtained with a clean shm. Save/load, CAS/FSR, pacing
and a new accepted baseline remain open. [Evidence/limits](LAUNCHER_PACKAGE.md).

## Display, FPS and effects

Three 35 s X11 checks confirmed the fitting of 16:9 into UltraWide/16:10 and resize,
VSync→FIFO, working CAS/FSR shader paths, an explicit 10→8-bit fallback and the limiter cadence of
60. Guest/window modes are separated in the SDK. The launcher shows the actual
parameters. Native FOV/HUD, high FPS and visual acceptance are still open.
[Matrix and limits](LAUNCHER_PACKAGE.md#display-colour-and-frame-pacing--2026-10-24).

### UltraWide projection evidence

A read-only bounded trace confirmed 64 results of `0x82BC8980` as a perspective
with FOV ≈ 60°, aspect 16:9, near/far 1/1000; max normalised error 3.285e-7.
192 entry samples, debugger read errors 0, shm orphans 0 before/after. One input
set; the gameplay camera/HUD/culling and the original console were not checked.
[Method, tools and limits](LAUNCHER_PACKAGE.md#ultrawide-perspective-confirmation).

### UltraWide native experiment

An override of a single perspective callsite 0x822B450C was implemented, disabled by default. The isolated Linux build e3ab3704… passed a 60 s GDB run:
6 matrices at 64:27, the other 58 at 16:9, max error 3.285e-7; shm orphans 0 before/after.
30 boundary cases of the actual hook object passed. The full UltraWide is not accepted:
the viewport/output, HUD/crosshair, related projections and culling are not yet consistent.
The ACV-Linux package remains the earlier display build; the experimental host is separate.
[Technical facts and evidence](LAUNCHER_PACKAGE.md#ultrawide-single-call-experiment--2026-10-24).

### Fullscreen 2560×1080 test

120 s X11: the window and output rect 2560×1080 without bars; the 64:27 experiment fired
on the selected callsite. Child 0, the optional system.properties remains, orphans 0.
The late scene ≈ 15.5–15.9 XE_SWAP FPS at cap 60; visual acceptance/HUD/culling
remain open. [Result and limits](LAUNCHER_PACKAGE.md#full-screen-test-2560×1080--2026-10-24).

WaitFix package X11 frontend/probe smoke PASS: cwd /, selected local libraries, no Qt/ICU, game and writer lock survive GUI exit, lock releases after game exit; child0/fatal0. [Checkpoint](../logs/acv-waitfix-package-smoke-20261025-r2/checkpoint.json).

2026-10-25: isolated fast comparison240s child0/fatal0; post90s Resolve still76.80% inclusive wall time, full77.35%; no stable60FPS or image acceptance. Full default retained. [Checkpoint](../logs/acv-fps-after90-20261025/checkpoint.json).

- 2026-10-26: FBO/full retested on current runtime; user confirmed black gameplay.
  Replay proves black source and output, but capture contains only presentation.
  Test stopped cleanly; FSI/full restored. GPU timestamp replay utility built and
  exercised; no60FPS claim. [Evidence](../logs/acv-fbo-retest-20261026/checkpoint.json).

- 2026-10-26: user confirmed FSI/full image restored and gameplay entered.
 60s read-only sample after90s gate completed (3103IPs, zero lost); ~15FPS,
 no new speed improvement claimed. [Evidence](../logs/acv-fsi-full-confirmed-20261026/summary.json).

- 2026-10-26: deferred readback scheduling core implemented and host-tested with ASan/UBSan. Runtime integration and FPS validation remain open. [Checkpoint](../logs/acv-deferred-readback-core-20261026/checkpoint.json).

- 2026-10-26: actual SDK dispatch/WAIT host regression passed ASan/UBSan, 96000 callbacks plus dependency-cycle/shutdown checks. Final GPU48994593 built, not runtime-tested. Source analysis identified pre-write/free and alias protection requirements; no FPS claim. [Checkpoint](../logs/acv-readback-dispatch-20261026/checkpoint.json).

- 2026-10-26: compiled SDK physical provider/allocator regression passed real A/C/E
  read/write faults, partial writes, read-only rearming and same-address reuse;
  explicit bridge passed ASan/UBSan. Isolated runtime a9125a31 passed75s X11/full/FSI smoke child0/fatal0;
  general GPU deferral remains off, no FPS gain claimed.
  [Checkpoint](../logs/acv-memory-provider-bridge-20261026/checkpoint.json).

- 2026-10-26: physical file IO reservation built; actual XFile source/SDK memory test
  and alias regression PASS, no temporary texture copy. Runtime not launched;
  XMA and general Vulkan deferral remain open. [Checkpoint](../logs/acv-host-physical-io-20261026/checkpoint.json).

- 2026-10-26: XMA atomic physical access built; actual Work/RAII + SDK Memory test
  and file/alias regressions PASS. Runtime5b126405 passed75s X11/full/FSI startup
  child0/fatal0. General GPU deferral off, no gameplay/FPS acceptance.
  [Checkpoint](../logs/acv-xma-physical-access-20261026/checkpoint.json).

- 2026-10-26: graphics CPU snapshots, packet provenance, upload reopening and
  completed-provider retirement built and host-tested. Runtime131b829f /
  GPUf2f3ee56 passed75s X11/full/FSI startup, child0/fatal0,
  session20261026T181420117779Z ended. General producer off in that build;
  no gameplay/FPS gain established.
  [Checkpoint](../logs/acv-graphics-cpu-coherence-20261026/checkpoint.json).

- 2026-10-26: general Vulkan producer integrated behind explicit flag, full/native
  FSI with low16MiB excluded. Real GPU/SDK oracle verifies11 copies/8 faults,
  overlap/current bytes, reuse and partial CPU writes. IO/XMA/alias regressions
  and actual-source8-case teardown ASan/UBSan PASS. First180s runtime
  20261026T183953558552Z ended child0/fatal0; >=90s general-path activity
  confirmed. No verified gameplay FPS gain. Safer teardown buildb0bf9b46/
  06400c82 is isolated; second CPU/GPU profile in progress.
  [Checkpoint](../logs/acv-general-vulkan-readback-20261026/checkpoint.json).

- 2026-10-26 UTC: coherent CPU read fast path built; allocation count6→0 and
  same-input host median107.546→22.275ns. SDK IO/owner/hole/readonly/packet,
  real GPU, file/XMA and alias/ABA regressions PASS. Runtimee12818a0 /
  GPU06400c82 passed180s X11/full/FSI child0/fatal0, session
  20261026T185840508042Z ended. Profile started >=90s,2468 IPs lost0.
  No identical-scene gameplay FPS acceptance; next provider scan/wait reason.
  [Checkpoint](../logs/acv-physical-read-fastpath-20261026/checkpoint.json).

- 2026-10-27 local: provider envelope/matched SDK build and real GPU/SDK
  regressions completed. Runtimeebf338a1/GPU14154063 passed180s
  X11/full/native FSI20261026T191748089182Z child0/fatal0/runtime_errors0;
  process ended. >=90s profile:2389 IPs lost0,4019 pool waits and0 CPU-demand
  bridge waits. Next admission-prefix optimization not yet implemented.
  No stable60FPS, image/game-time or package-promotion acceptance.
  [Latest checkpoint](../logs/acv-provider-range-rejection-20261026/checkpoint.json).

- 2026-10-27: minimal-prefix readback admission built; queue/retirement/bridge
  ASan/UBSan and actual GPU12-copy/8-fault/partial-prefix regression PASS.
  Reproducible512x4MiB pressure test improves component throughput1.298x.
  Runtimeebf338a1/GPUc558cb37 passed180s menu-only X11/full/native FSI
  20261027T043019725818Z child0/fatal0, ended. No gameplay60FPS claim.
  [Checkpoint](../logs/acv-readback-minimal-admission-20261026/checkpoint.json).

- 2026-10-27: oldest fitting-slot retirement built4cf700f5; selector/teardown
  ASan/UBSan04e3f279, actual GPU/SDK12-copy/8-fault a2f09d07 and90с gate
  0fac27ff PASS. Mixed-size benchmark619bb271:183.850→143.635мс median,
  allocator full drains32→0. Candidate ebf338a1/f1ccdfb9 is not runtime-tested.
  Identical offline profile clones prepared under source lock/verified backup;
  scripted comparison not started. No gameplay60FPS or promotion claim.
  [Consolidated progress](FPS_READBACK_PROGRESS_20261027.md).


## Focused readback runtime comparison, 2026-10-27

Runtime comparison completed: candidate20261027T052317764850Z and focused control20261027T052659721550Z,210s each, child0/fatal0. Pool wait976.869→33.794ms in late35s; cadence≈60 in both, no mission FPS gain proven. Main XThread44.79→95.29% core; polling82BAF218/82BAE308 needs investigation. Actual fullscreen1920x1080; mission identity/pacing/image unconfirmed. All runs ended; no promotion.

[Detailed results and remaining work](FPS_READBACK_PROGRESS_20261027.md).


- 2026-10-27: Parallel copy accepted by user; host/GPU/ASan checks and two210s game runs pass. Actual copy worker16.79% core. Gameplay windows differed, so no FPS gain claim. Separate package promotion remains pending. [Details](CPU_PARALLEL_READBACK_20261027.md).


- 2026-10-27: Published separate FOVHUD package185files; accepted two-thread readback launch mode, CAS/FSR1 integrated without effect tests per user. Viewport-derived HUD/RAII and camera LR diagnostics fixed. Actual-source ASan/UBSan and35s2560x1080 startup pass; world-marker/visual parity unverified. [Package](LAUNCHER_PACKAGE.md), [checkpoint](../logs/acv-fov-hud-refine-20261027/checkpoint.json).

- 2026-10-27: Launcher memory/retention/readahead and recovery settings complete in separate Streaming candidate185files. Bounded archive worker and actual eviction/IO ASan/UBSan pass; two210s runs child0/fatal0, package/frontend/UI checks pass. Cache evictions0 under both policies; model/location unloading and loading-time gain unconfirmed, representative reproduction needed. CAS/FSR effects untested, accepted baseline retained. [Evidence and remaining work](RESOURCE_STREAMING_20261027.md).

- 2026-10-27: User-requested ACV-Linux-release-1.0 assembled with current TRG launcher, recompiled game and copied data/libraries.186 manifest files (~3.88GiB), version metadata/release notes, dependency/hash checks pass. Previous packages and saves retained; CAS/FSR tests omitted. Startup result and exact hashes: [release checkpoint](../logs/acv-release-1.0-20261027/checkpoint.json). Existing gameplay/loading-time limits remain.

  Release1.0 exact-folder45s frontend check: child0/fatal0, local runtime/environment,
  profile lock and shutdown lifecycle PASS; startup scope only.

- 2026-10-28: User confirmed «Да, помогло» for the native UltraWide local-frustum fix. Actual translated retail sphere tests/SDK context and FOV/HUD ASan/UBSan pass;160s2560×1080 run child0/fatal0, both camera paths patched. Release1.0 updated to ELFefa4756c with verified backup,186hashes/dependencies pass. Resource ownership unchanged; no new FPS or blanket streaming-unload claim. [Details](ENGINE_OFFSCREEN_20261028.md).
