# Launcher settings and resource streaming — 2026-10-27

Latest request: finish launcher settings and reduce location/model/resource loading
and premature unloading. This expands the prior FOV/HUD-only scope. CAS/FSR effects
remain untested by user instruction; game tests request only2560×1080.

## Implemented and packaged

Separate candidate: start.sh (`../../ACV-Linux-TRG-Streaming-20261027-r2/start.sh`, исторический пакет; каталог сейчас отсутствует).
185 files, TRG without Qt/ICU; prior accepted FOVHUD package and baseline unchanged.
Full artifact hashes, operation IDs, protected-data hashes and comparison:
[checkpoint](../logs/acv-resource-streaming-20261027/checkpoint.json).

Shared settings/backend now expose target/hard texture-cache limits, retention
and archive readahead. Defaults:1024/1536MiB,180s, auto16MiB on capable runtimes.
The streaming preset selects1536/2048MiB,300s,32MiB. Compatibility restores
384/768MiB,30s and disabled preloading. These bound the texture cache, not total
VRAM or the guest heap. In-flight textures are never evicted to enforce a limit;
temporary overshoot remains possible until GPU completion. Other GPU allocations
and applications require additional VRAM headroom.

Launcher adds custom resolution input and backend-observed display modes, shows
last-run display information automatically, and rechecks build/data availability
before starting. An invalid experimental option no longer disables unrelated
recovery controls. Saving still validates the complete settings atomically.
Both cache budgets can be edited before one Save; availability no longer depends
on the previously saved counterpart. Presets use the shared schema; profiles, XUID, original config and saves are retained.
CAS/FSR1 and sharpness remain integrated.120–165FPS, YCbCr and temporal FSR2/3
remain unavailable with reasons;10-bit still depends on the actual surface.
These unavailable modes have not been made functional by the new UI.

Linux PosixFileHandle can preload upcoming ranges from read-only BDT/BHD/TPF/DCX/FSB
archives of at least1MiB. A persistent worker uses POSIX_FADV_WILLNEED, with at most
8 queued hints, coalescing and at most128MiB per hint. Duplicated CLOEXEC descriptors
keep the correct inode alive across close/reuse. EOF/partial reads/writes retain
the original semantics. Saves and writable handles bypass preloading. Advice is
optional; queue pressure or advice failure does not fail the real read. The kernel
page cache remains reclaimable; there is no second unbounded copy of archive data.
This prepares archive bytes, **not semantic model/location dependency graphs**.

Durable sources: tools/native-rexglue/asset_read_ahead.h and patches
bounded-archive-read-ahead.patch / texture-retention-telemetry.patch. SDK mirrors
built successfully; zero-fuzz patch replay matches. Existing GPU readback
ownership, allocation epochs, fences and parallel-copy protocol are preserved.

## Verification and measured limits

Actual PosixFileHandle and cache-eviction code pass focused ASan/UBSan checks:
random/partial/EOF reads, writable bypass, descriptor lifetime, bounded queue,
advice failures; soft/age/hard eviction and in-flight lifetime. Five shared
settings/recovery checks pass. Candidate environment parsing rejects unknown,
malformed, duplicate and injected values. All six UI pages render successfully.
These checks do not prove whole-engine or long-run leak freedom.

Sequential isolated210s runs: off20261027T154205893802Z and
on20261027T154536891074Z, both child0/fatal0/runtime_errors0. Sampling starts at
process age120s for35s; retention/preload diagnostics start after90s. Both use
full/native FSI, general deferred readback and two-thread copy, bilinear, noFXAA,
no dither/CAS/FSR, requested fullscreen2560×1080. Original profiles are cloned;
the machine page cache is not cold-controlled and scene equivalence is unproven.

| Late34s observation | Previous policy | Tuned policy |
|---|---:|---:|
| Texture-cache bytes at last report |190877184|190509056|
| Texture evictions after90s |0|0|
| Process read characters |212762324|239311370|
| Process disk read bytes |851968|3649536|
| Mean RSS MiB |1780.54|1758.33|
| Reported GPU memory MiB, mean |2815.0|3050.91|

Read characters include all process reads and are not archive-only traffic.
GPU-memory reporting is not the texture-cache counter and does not establish a
causal policy cost across different scene timing. Both texture caches stayed
below the old384MiB soft limit: **premature texture eviction was not reproduced**.
The tuned worker completed399 hints,17 queued coalescences,0drops/0failures;
6529871564 advised bytes are cumulative hint lengths, not unique disk bytes loaded.
Most late reads already hit RAM; no loading-time or gameplay-FPS gain is claimed.

Standalone verification checks185 hashes, dependency resolution and absence of
Qt/ICU.45s frontend run from cwd/ proves exact package libraries/environment,
unbounded session, profile lock retained after GUI exit, second writer rejected,
normal child exit and lock release. Requested windowed2560×1080 was constrained
by the work area to2560×1044; this is startup/launcher integration, not a second
resolution benchmark or fullscreen/HUD acceptance.

## Engine investigation and remaining work

Retail script registration Scr_UnloadInGameResource points through82589490 to
82586050. Read-only raw capture confirms four conditional calls from fields
+0x164/+0x14/+0x18/+0x10; no ownership/refcount suppression was applied.
82A0E748 is a virtual dispatch through vtable+4, not evidence of a fixed unload
interval.829F1838 calls829F1678 and829F13F8. Ghidra body boundaries/gaps are
recorded; snapshots are evidence, not recovered C++ ownership semantics.
[Root listing](../decompilation/evidence/acv/82586050-20261027T155029.640765Z/listing.txt).

SDK src/kernel/xboxkrnl/xboxkrnl_io.cpp:213 forces NtReadFile through the
synchronous branch even for asynchronous handles;244 converts the already
completed result to PENDING. This is a confirmed implementation limitation,
**not measured proof that it causes this reported delay**. Simply removing
true|| would enter unfinished async code. Any future async implementation needs
owned file/event/caller lifetime, IO-status/APC/completion ordering, guest-buffer
coherence and shutdown/cancellation tests before replacing the working path.

Still open: reproduce the reported reload in an identified mission/action,
measure its archive reads/decompression/resource-build/lifetime transitions,
then implement dependency-aware preloading/retention with ownership evidence.
A text question about entry/camera/return and mission identity is pending.
Do not equate an increased GPU cache or successful page-cache hint with a fix
for engine model/location unloading. Do not enlarge the guest heap blindly or
skip scripted cleanup. A representative return-to-area/mission cycle and a longer
memory plateau check are needed before claiming this part complete.

Previous active documents preserved byte-for-byte with SHA-256 in
[docs snapshot](../logs/acv-resource-streaming-20261027/docs-before/sha256.json).
