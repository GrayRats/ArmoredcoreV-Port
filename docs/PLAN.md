# ACV — Current Status


TimerQueue fix2026-10-26: actual shared-library1ms workload CPU10.03%→1.19%, callback/cancel/deadline/join checks pass. Isolated runtime7b1f69a0 is running in user-confirmed gameplay; diagnostics start after90s. Runtime90s measurement confirms timer CPU9.33%→0.64%,6393 IPs without loss. Game left open; stable60FPS/image acceptance and normal exit remain open, no baseline promotion.

FPS diagnostics remain gated after90s. Split timers identify full current-result waits (~57%) and memcpy (~13%); pending-slot waits0. Fast incorrectly waited for a new result when its previous generation was unfinished; fast-only fix passes actual SDK-tail ASan/UBSan. Isolated GPU45dbaab6 completed180s child0/fatal0, still15–17FPS; nonblocking fast-submit GPUd2404533 completed180s child0/fatal0 with user-confirmed gameplay, late22–25FPS/GPU88.4%. This is not identical-input A/B; image acceptance is pending. Separate TimerQueue CPU waste is under evaluation. Full/default retained; stable60FPS and fast image acceptance remain open.
Acceptance update 2026-10-25: the user confirmed the mission works. Session without GDB `20261025T165411391172Z` on the isolated profile completed180s, child0/fatal0. The original save has not been migrated; a full save/load round trip is open. The separate last abort was GDB (breakpoint.c:9182); return observers were replaced and tested. Next priority: mission FPS without a debugger.


CPU fix2026-10-25: fractional1ms WaitMultiple busy polling removed. Audio worker78.05%→0.56% of one core;190s child0/fatal0. Candidate ../ACV-Linux-TRG-WaitFix-20261025/. Heavy-scene16–21FPS remains; stable60FPS is open.
Documentation updated 2026-10-25; latest runtime/launcher evidence includes 2026-10-25. The detailed implementation queue is in
[PLAN_GUI.MD](../PLAN_GUI.MD). This file keeps the short facts about the native Linux
line and the user's decisions.

[Русская версия](PLAN.RU.MD)

## Goal and boundaries

The goal is to run Armored Core V natively on Linux through ReXGlue. The original ISO is
read-only; executables and temporary data live in `tmp/`, logs and evidence in `logs/`.
Do not mix ReXGlue with the independent XenonRecomp branch, and do not treat the generated
PPC C++ as a recovered source.

The regular diagnostic run uses [acv.config](../tools/native-rexglue/acv.config) and the
ARTEM/ARTEM profile. The diagnostic ELF is
`tmp/acv-rexglue-linux-indexed/out/build/linux-release/acv_linux_probe`.
Settings, profiles and the launcher implementation follow [PLAN_GUI.MD](../PLAN_GUI.MD).

## Confirmed

- The user confirmed the fix for black fur textures. The established cause of the shader
  fetch problem is a wrong `exp_adjust` dword; the accepted GPU plugin `90d8ef1a…` is kept.
  [Technical evidence](NATIVE_REXGLUE_LINUX.md#texture-rendering-fix-confirmed-by-user-2026-10-24).
- Resolve/readback and resource lifetime received synchronization, range checks, error
  handling and deferred release of staging resources. Tests and the build passed. By the
  user's decision, the remaining DLC/readback signals are accepted as known: missing sound
  payloads, the optional `system.properties`, the absence of gameplay readback in the
  startup smoke test, and non-forced eviction under load.
- Four required `dlc_image:` containers are extracted from the retail `dvdbnd` and mounted
  read-only. Five remaining sound payloads are missing from the available ISO; accepting the
  requests as known does not mean more data is available.

## Current decisions and acceptance limits

| Area | Current state | Acceptance boundary / next evidence |
|---|---|---|
| Mission 2 | User confirmed the transition on join-fix runtime `2b346825…` | This accepts the reported transition, not every mission or the full game |
| Other mission selection | User confirmed the mission works with isolated name trial and runtimef3600aca | Original save migration and regression across other missions remain open |
| Launcher | TRG built: six pages, asynchronous backend, game/lock and ISO import verified; Qt checkpoint retained | Complete hardware/gameplay acceptance in stage C; source and package evidence in LAUNCHER_PACKAGE |
| Profiles/saves | Backend checks exist; gameplay save/load round trip is open | Create progress, save, exit and load; verify profile isolation and backup restoration |
| Display/graphics | Aspect fit and parts of runtime telemetry verified; adaptive FOV/HUD, high FPS and CAS/FSR image acceptance remain open | Use the per-feature criteria in PLAN_GUI; submitted-frame cadence is not proof of game-time pacing |
| Standalone package | Separate ACV-Linux-TRG-20261025 candidate without Qt/ICU; original Qt package retained | Verify TRG package from arbitrary cwd, manifests and persistent XDG data before replacing the accepted package |

Mission evidence: [latest native result](NATIVE_REXGLUE_LINUX.md#mission-2-accepted-by-user-another-mission-selection-abort-2026-10-24).
Older freeze hypotheses describe earlier sessions and must not override that result.
These rows summarize the queue; priorities and detailed criteria stay in PLAN_GUI.

Use [TOOLS.MD](../TOOLS.MD) before choosing GUI, extraction or binary-analysis tools.
ReAnswered is a reference for analogous work, not a source of interchangeable ACV hooks.
Check actual tool availability and pinned revisions; a checkout is not a tested binary.

For continuation, follow the current user request, then the relevant PLAN_GUI item,
SQLite task and latest technical evidence. Documentation changes do not resume game
execution. Preserve ARTEM/ARTEM, the original `acv.config`, saves and accepted artifacts.
No extra DLC search or deferred readback stress work is implied by this update.

## Documents and archive

- [PLAN_GUI.MD](../PLAN_GUI.MD) — the current detailed implementation queue and its
  completion criteria.
- [PROGRESS](PROGRESS.md) — completed stages and limitations.
- [STATE](X360_PROJECT_STATE.md) — short pointer for resuming work.
- [NATIVE_REXGLUE_LINUX](NATIVE_REXGLUE_LINUX.md) — runtime evidence.
- [AGENTS](../AGENTS.md) — working rules.
- [TOOLS](../TOOLS.MD) — tool selection and availability checks.

The previous version of this plan is preserved in
[`docs/fine/PLAN_worktree_before_compression_20261024.md`](fine/PLAN_worktree_before_compression_20261024.md);
the checksum and the original Git blob are recorded in
[`PLAN_worktree_before_compression_20261024.json`](fine/PLAN_worktree_before_compression_20261024.json).
An earlier plan merge is described in [`planning-merge.json`](fine/planning-merge.json).

- 2026-10-26: FBO/full retest still gives black gameplay; rejected for working use. FSI/full restored. Scene resolve diagnosis and accurate deferred readback remain open;60FPS not achieved. [Evidence](../logs/acv-fbo-retest-20261026/checkpoint.json).

- General deferred readback started: scheduling core passes host ASan/UBSan. Read providers and runtime integration pending; full remains active, no measured FPS improvement. [Checkpoint](../logs/acv-deferred-readback-core-20261026/checkpoint.json).

- Readback dispatch locking and restricted WAIT_REG_MEM service built and host-tested (GPU48994593). General memory providers still pending; direct file writes and allocation reuse require synchronization before mutation. No measured FPS gain. [Checkpoint](../logs/acv-readback-dispatch-20261026/checkpoint.json).

- 2026-10-26: SDK physical providers and alias allocator guards now pass compiled fault/reuse tests. Direct host access, parent allocator/reset paths and Vulkan staging/teardown integration remain open. Isolated runtime a9125a31; general deferral off, no measured FPS gain. [Checkpoint](../logs/acv-memory-provider-bridge-20261026/checkpoint.json).

- Physical host file IO now has range reservation and pre-write provider synchronization;
  compiled actual-source tests pass. XMA/multi-range and Vulkan integration remain open.
  [Checkpoint](../logs/acv-host-physical-io-20261026/checkpoint.json).

- XMA atomic physical access now passes actual Work/SDK Memory and file/alias
  regressions. Runtime5b126405 passed75s full/FSI startup. General GPU producer
  remains off; staging/graphics consumers/teardown and7F mirror eligibility open.
  No gameplay FPS gain measured. [Checkpoint](../logs/acv-xma-physical-access-20261026/checkpoint.json).

- Graphics CPU coherence built and host-tested: packet wrapping/inline addresses,
  current bytes after GPU waits, upload submission reopening and provider retirement.
  Runtime131b829f / GPUf2f3ee56 passed75s X11/full/FSI startup child0/fatal0.
  No post90s gameplay/FPS result. General Vulkan producer is being integrated in
  source under an opt-in flag; accepted package unchanged.
  [Checkpoint](../logs/acv-graphics-cpu-coherence-20261026/checkpoint.json).

- General Vulkan current-generation readback now built under an opt-in flag.
  Real GPU/SDK byte oracle, IO/XMA/alias regressions and8 fault-injected teardown
  cases PASS. First180s X11/full/FSI runtime ended child0/fatal0; after90s
  52709 resolves/52700 publications used3217 ordered prefix waits. Nested timers
  are inclusive, not exclusive bottleneck shares. No verified FPS improvement.
  Safer teardown candidate runtimeb0bf9b46/GPU06400c82 is under CPU profiling;
  image/gameplay, identical-scene FPS and broader acceptance remain open.
  Accepted package preserved. Estimate remains65% done/35% remaining (±10pp).
  [Checkpoint](../logs/acv-general-vulkan-readback-20261026/checkpoint.json).

- Coherent CPU read fast path verified:6→0 allocations/read and4.83x faster
  same-input host word reads; this is not a gameplay FPS ratio. Compiled SDK
  IO/alias/hole/packet and real GPU regressions PASS. Runtimee12818a0 /
  GPU06400c82 passed180s X11/full/FSI child0/fatal0; >=90s CPU profiling
  points next to provider-list traversal. GPU-worker CPU66.47% vs prior81.21%
  observed across uncontrolled scenes. Stable60FPS/image acceptance still open.
  [Checkpoint](../logs/acv-physical-read-fastpath-20261026/checkpoint.json).

- Latest follow-up completed: provider range rejection and CPU/pool wait metrics
  built and SDK/real GPU-tested. Runtimeebf338a1/GPU14154063 passed180s
  X11/full/native FSI20261026T191748089182Z child0/fatal0; process ended.
  Current capture:4019 pool waits (~31.6% inclusive wall), CPU-demand waits0.
  Next retire only the oldest prefix needed for staging admission; not implemented.
  Remaining estimate35% (±10pp): stable60FPS/game-time, mission/save regression,
  UltraWide/HUD/culling, CAS/FSR/color, controller/HiDPI and runtime promotion.
  Accepted separate package preserved; no exact gameplay FPS gain claimed.
  [Latest checkpoint](../logs/acv-provider-range-rejection-20261026/checkpoint.json).

- Minimal-prefix staging admission built and GPU/ASan-tested. Same-input GPU
  transfer benchmark improves component throughput1.298x (not gameplay FPS).
  Runtimeebf338a1/GPUc558cb37 passed180s menu-only X11/full/native FSI;
  no mission FPS acceptance. Next oldest-fitting-slot reuse; mission choice
  requested for representative validation. Accepted package unchanged.
  [Checkpoint](../logs/acv-readback-minimal-admission-20261026/checkpoint.json).

- Oldest fitting-slot retirement is built and ASan/real-GPU tested. Mixed-size
  component throughput1.280x; no gameplay FPS claim. Latest candidate
  tmp/acv-fitting-slot-fix/ (runtimeebf338a1/GPUf1ccdfb9) is not game-tested yet.
  Verified offline profile clones and the previously working input sequence are
  ready for control/candidate comparison; it has not started.
  [Complete progress/remaining work](FPS_READBACK_PROGRESS_20261027.md).
