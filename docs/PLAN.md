# ACV — current plan

2026-11-05: ReXGlue's game Vulkan backend now requests API 1.4, checks the
loader version-query result and rejects loaders/devices below 1.4. Linux Release
and Windows MinGW-targeted Clang `rexui` builds pass. Full Windows SDK/game build
still stops in vendored FFmpeg's MSVC CRT configuration; Wine gameplay, native
Windows WSI/package and macOS acceptance remain open. ReXGlue's pinned header
stack is 1.4.357.0; LunarG 1.4.363.1 remains the launcher SDK/runtime.
[Windows implementation queue](tools/acv-context/PLAN.Windows.MD).

2026-11-04: Windows/all-OS Vulkan checkpoint — launcher Vulkan .exe linked
against LunarG SDK 1.4.363.1; API 1.4 is required. Wine with LunarG loader
1.4.363 found no usable Wine ICD bridge; separate Wine built-in-loader run
completed the 80-frame Vulkan renderer smoke but had no Windows Python backend.
Linux SDK 1.4.363.1 build, Vulkan/OpenGL 80-frame smokes and CTest 3/3 passed;
Python portability tests 39/39. Native Windows WSI/game, full Windows ReXGlue
build/bridge/package, macOS/MoltenVK runtime and cross-OS FPS remain open.
Profiling is NOT ASSESSED; no game release was promoted.

2026-11-04: separate [ACV-Linux-release-2.1](tools/ACV-Linux-release-2.1/start.sh)
compiled and packaged. 243 native TUs rebuilt; two libraries linked; package
hashes/shell syntax/dependency resolution PASS. No v2.1 launch or gameplay test;
it remains a candidate, and 2.0 plus the accepted runtime baseline are preserved.
[Build/package evidence](kb:ACV_LINUX_RELEASE_2_1_20261104).

2026-11-04: persistent driver cache теперь также используется двумя lazy
render-target ownership-transfer pipelines; handle и callback снимаются до
teardown владельца. Изменение включено в v2.1; compile/package checks прошли,
GPU и gameplay acceptance открыты.
[Status](kb:FPS_VULKAN_RENDER_TARGET_CACHE_20261104).

2026-11-04: all seven optional Vulkan paths implemented: persistent driver cache,
scoped sync2, timeline retirement, vertex-input GPL, dynamic cull/front-face,
staging suballocation and second same-family copy queue. Native runtime/GPU
rebuilt243TUs without stale PCH. ASan/UBSan, GPU byte/pixel and deferred/patch
replay PASS; cold/warm135s each child0/fatal0/errors0; cache reload=true. FPS/gameplay
acceptance open; release2.0 preserved. [Result](kb:FPS_VULKAN_SEVEN_20261104).

2026-11-04: Opt-in exact descriptor-bind dedup built a8abd3a1.
Host sanitizers/replay/loader/check-only PASS. GPU oracle compiled, deferred
while user game active; no FPS/render claim. Release2.0 preserved.
[Next check](kb:FPS_DESCRIPTOR_BINDS_20261104).

2026-11-03: Separate ACV-Linux-release-2.0 ready with stencil3/API updates.
GPU3TU and TRG rebuilt; unchanged guest ELF/runtime/FFX reused. Sanitizers,
backend,UI600frames,189hashes/loader and135s startup PASS. Sibling-release path
selection fixed without writing saved settings/config;1.0 preserved. Mission/FPS/
stencil-image acceptance remains open.

2026-11-03: User-authorized Vulkan API changes deployed into final1.0 package.
GPU/env/packaging baseline updated;189file manifest/release135s smoke PASS.
Rollback bytes preserved. Three additional stencil filters built/host-tested,
default off in separate candidate; GPU/render acceptance pending.

2026-11-03: Sol Vulkan state dedup240s/counters135s runs clean.
40.04s counters:173122index binds (6.54%) and2638push updates (4.15%) omitted.
Vertex/blend/depth unexercised; FPS/render acceptance open. [Result](kb:FPS_VULKAN_STATE_DEDUP_20261103).

2026-11-03: Sol built opt-in index/vertex/push/blend/depth-bias API dedup.
Host ASan/UBSan and compute push GPU oracle PASS; rendering/gameplay untested.
Copy-byte240s run clean:3516copies/9sizes; no causal FPS claim.

2026-11-03: Sol added staging copy_bytes to GPU timestamp records.
ASan/UBSan, actual GPU byte/guard/metadata oracle, 3TU build and loader PASS.
New gameplay capture with sizes pending; no FPS gain claim.

2026-11-03: Astra6 implemented opt-in GPU timestamps (candidate38b2e377).
Host/GPU oracle/build/replay/load PASS; actual240s run completed cleanly,
1092 samples verified, no drops/truncation/API failures. Next investigate long
inclusive draw regions and staging copies. No FPS gain or scene/render/game-time
acceptance; default/release unchanged. [Result](kb:FPS_GPU_TIMESTAMPS_20261103).

2026-11-02: opt-in direct fence-status polling implemented by Astra6. Host/GPU/build7029daf1 PASS;blocking waits/prefix/lifetime preserved. Microtest saves≈0.1µs/call,no FPS claim,default off. Next: GPU timestamp profiling of FSI/Resolve/copy. [Result](kb:FPS_FENCE_STATUS_20261102).

2026-11-02: coherent0/1 bounded runs240s each,exit0/fatal0/errors0. User confirms
subjective improvement of current batching1/descriptor1/coherent1 combination.
Workloads differ materially (Resolve1800 vs41837,GPU29.9% vs91.85%); swap rates
29.998 vs19.373 do not establish a gain/regression from coherent skip.
No release/default change. Next: matched scene/render/game-time validation.
[Evidence](tools/acv-context/logs/readback-coherent-invalidate-20261101/runtime-comparison.json).

2026-11-01: opt-in coherent readback invalidate elision implemented by Astra6. Host/GPU oracle32→0invalidate calls,exact bytes;noncoherent coverage host-injected only. Default off,FPS unproven. [Result](kb:FPS_COHERENT_READBACK_20261101).

2026-11-01: immutable Resolve descriptor cache64keys implemented by Astra6. Host/GPU oracle128→4descriptor updates,correct bytes/guards;default off,gameplay hit-rate/FPS unmeasured. [Result](kb:FPS_VULKAN_API_20261101).

Runtime0/1 completed240s each,exit0/errors0: observed14.57→17.58swap/s and1%low12.29→15.58,batchmean3.295/max4. Scene/route not confirmed: no causal FPS acceptance or release promotion. Next matched-scene/render/game-time validation.

Current request: Astra6 implemented opt-in Resolve batching4jobs/16MiB. Host/GPU oracle PASS,40→14submits with equal bytes/memory; isolated candidate ready. Next: matched first-mission batch0/1,actual batch distribution,frametime/render/game-time acceptance. No FPS gain claimed. [Result](kb:FPS_RESOLVE_BATCHING_20261031).

Updated: 2026-10-30. [Русская версия](kb:PLAN_RU).

## Current package candidate

[ACV-Linux-release-2.1/start.sh](tools/ACV-Linux-release-2.1/start.sh) contains
the TRG launcher, statically recompiled game, game data and required libraries.
It is compiled and package-verified, but v2.1 has not been launched or gameplay-
tested. Release2.0 remains preserved as the prior package; v2.1 is not yet the
accepted runtime baseline. This is a native Linux ReXGlue build, not fully
recovered game source.

## Completed

- MangoHud distinguishes Host FPS from Guest swap cadence; capped startup check agrees at 30/s. Dithering help, three validated presets and adaptive quality VRAM budgets are in the local package. All 19 settings / 137 values audited. Mission FPS discrepancy remains unreproduced. [Details](kb:LAUNCHER_SETTINGS_20261030).

- Mission fixes and parallel readback copy accepted by the user and included in the release.
- UltraWide FOV/HUD and edge-frustum correction implemented; the user confirmed the fix helps.
- Profiles, backups/import/restore, settings and launcher workflows implemented.
  Preview is read-only; refresh retains edits; graphics reset keeps launcher edits unsaved.
- Texture-cache budgets, retention and bounded archive read-ahead integrated.
- Russian window title fixed; FPS limits 30/60/120 and 36 resolution presets with group filters available.
- CAS/FSR1 integrated without effect tests; FSR2/3 removed from the interface.
- Current package, launcher workflows and startup checks pass; original config,
  saves and personal settings are preserved.

## Remaining within the resource-loading work

- Reproduce delayed loading or premature unloading in a specific mission/action.
  The accepted frustum correction fixes edge visibility; resource lifetime is a separate issue.
- Identify the remaining resource-build, decompression or lifetime bottleneck;
  apply retention/preloading changes with bounded memory, then verify repeated
  transitions and memory use. Faster loading and model/location lifetime are not yet proven.

Native file/folder dialogs are integrated; interactive desktop selection is not manually verified.
Stable 60 FPS and a full gameplay save/load round trip remain unverified; they are
not yet accepted; FPS work resumes under the explicit2026-10-29 request. Do not test CAS/FSR effects; resolution checks
are limited to 2560×1080. Gameplay profiling starts only after 90 seconds.

## Active FPS request — other solutions, 2026-10-30

Target >30FPS with correct rendering/game-time remains open. Astra6 built two
opt-in candidates: idle-slot reclaim d9998da1 and latest-required-fence5bf5a83f.
Host ASan/UBSan and GPU data/fence oracles pass. Reclaim reduces capacity-wait
callbacks31→23 but its transfer-only test is5.2–19.5%slower. Latest-fence reduces
prefix32 to1passed handle, preserving completion and error handling; FPS unmeasured.
Both default off; these FPS changes were not installed into the release.
Previous prepared comparison: same latest candidate flag0/1, actual fence-prefix distribution and matched
first-mission raw frametime/1%low/render/game-time check. Its runtime is current
release8e2bae61; older6090/d999 checkpoints retain544bdaae. Submit batching requires
recorded/submitted/failed lifecycle changes. Forward128 GPU copy≈3%slower;
its earlier gameplay control ended before capture, so no FPS A/B exists.
[Alternatives](kb:FPS_READBACK_ALTERNATIVES_20261030), [report](tools/acv-context/report-fps.md).

## Details

[Implementation queue](tools/acv-context/PLAN_GUI.MD) · [Launcher/package](kb:LAUNCHER_PACKAGE) ·
[Streaming](kb:RESOURCE_STREAMING_20261027) · [Frustum fix](kb:ENGINE_OFFSCREEN_20261028).

Previous plan text is preserved with SHA-256 in [the archive](tools/acv-context/logs/plan-compression-20261028T175039Z/manifest.json).
