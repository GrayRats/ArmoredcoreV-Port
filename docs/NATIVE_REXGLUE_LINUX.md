# Independent ReXGlue Linux bring-up

Linux is the active platform. This experiment uses no XenonRecomp-generated C++.
The existing primary translation remains preserved.

## Confirmed tool and input evidence

- Original input entry point `0x82B91F88`, image base `0x82000000`.
  XEX SHA-256 `4fac87348f827e84633013375012284cd8c769a3c155a675b2989757e666a12b`.
- Installed ReXGlue 0.10.0, exact SDK source
  `f5337cdc947ff6d4c4196737e2c807a48f2a1fc2`.
- Private experiment: `tmp/acv-rexglue-linux-indexed/`.
  Full extracted data already exists at `tmp/iso_root/`; the experiment's
  `assets/` directory only contains the private XEX copy.
- Stock CMake Linux configuration succeeded, operation
  `ac6eb240-bf20-4615-aa6d-8a4e4071f193`. This is not a compiled game.

## Analyzer performance experiment

Our own codegen process was sampled with GDB; no foreign process or system
setting was changed. GapFill repeatedly visited `FunctionNode::tryResolveAgainst`
through `FunctionGraph::notifyFunctionAdded`. The original algorithm scans all
pending functions for every new function. An isolated target-to-owner index
keeps the existing resolution operation and checks live owners before use.

Only the copied SDK source in `tmp/rexglue-graph-opt/indexed/` is patched.
The installed SDK and existing XenonRecomp checkout are unchanged.
Sources/harness: `tools/rexglue-graph-opt/`. Experimental CLI:
`tmp/rexglue-graph-opt/build/rexglue_indexed`.

Confirmed host algorithm evidence:
[graph tests](../logs/acv-host-20261021/graph-index-verification.json),
[exact patch](../logs/acv-host-20261021/rexglue-target-index.patch),
[provenance](../logs/acv-host-20261021/graph-index-provenance.json).
5026 trace steps and a 21024-registration synthetic workload produce identical
digests with baseline/indexed implementations under UBSan.
CLI build: verified operation `c95648a8-84cd-4752-a6f6-6c3d983327b3`.

On ACV, stock analysis failed validation after 1081.9 s; indexed analysis failed
after 406.7 s. Both produce exactly the same multiset of 39 unresolved target/site
pairs (28 distinct targets), confirmed by operation
`716e2410-f58a-4d97-9dc3-3b33e19b8017`.
[Comparison and saved stream hashes](../logs/acv-host-20261021/codegen-indexed-comparison.json).
This establishes matching diagnostics on this run, not complete analyzer or
runtime equivalence. No `--force` validation bypass is used.

## Boundary correction under test

Original mapped XEX bytes confirm all 39 relative, non-link branches. Their
28 targets have no owner in the saved original `.pdata` table; absence of an
entry is not evidence that the code is unused.
[Original-byte audit](../logs/acv-rexglue-unresolved-20261021/retail-branch-inventory.json).

The separate XenonRecomp function registry was used only to propose boundaries,
then every proposed interval was read from original bytes and disassembled.
No generated C++ or ABI was mixed between tools.

25 targets are entry candidates for short return/tail-dispatch routines or
larger leaf routines. The other three are internal labels:
`0x82DAD42C` inside `[0x82DAD3B8,0x82DAD448)`,
`0x82E2E694` inside `[0x82E2E628,0x82E2E6B0)`,
`0x82E2E7AC` inside `[0x82E2E740,0x82E2E7C8)`.
They share return paths after out-of-line float load/store blocks; they are
not configured as invented standalone functions.

`tools/native-rexglue/validate_boundaries.py` checks reachable control flow:
all 240 words in 28 intervals are reachable from their proposed entries,
conditional local targets remain inside, and paths terminate through explicit
returns, indirect transfers, or direct tail branches. The raw disassemblies
are `logs/acv-rexglue-unresolved-20261021/candidate-*.asm`.
[Checked bounds and exits](../logs/acv-rexglue-unresolved-20261021/boundary-validation.json).
Verified static operation `7437ff6e-f90a-406b-81b2-1be03076ca24`.

Durable hints: `tools/native-rexglue/acv-boundaries.toml`.
Schema source: exact SDK `src/codegen/config.cpp:177` and
`src/codegen/phase_register.cpp:547`. Included by the private manifest.
These are instruction-backed analysis boundaries, not recovered symbols or
retail behavior validation. The first 28 hints removed the original39 unresolved direct-branch diagnostics
but exposed three adjacent tail routines. After adding three more checked hints,
full codegen completed in517.0s and wrote625 C++ files. The saved output reports
95 unresolved conditional branches across19 distinct targets. Exact release
source `src/codegen/builders/context.cpp:316-327` emits `REX_FATAL` when such a
branch has no `CallTarget`, so the generated image cannot be treated as runnable.

The [retail-byte conditional audit](../logs/acv-rexglue-unresolved-20261021/conditionals/inventory.json)
confirms each of the 95 source words is a relative PPC `bc` whose calculated
address matches original XEX. None of the 19 destination addresses has a
`.pdata` owner. Raw target windows are in the same `conditionals/` directory.
Instruction inspection showed that all 19 targets are short exit fragments: 18
are `li r3,0; blr`, and `0x82500F14` returns a value loaded by `lis/addi` then
executes `blr`. The supplemental config
[`acv-conditional-targets.toml`](../tools/native-rexglue/acv-conditional-targets.toml)
records only those byte-backed 8/12-byte extents, with neutral generated names;
this does not establish their containing retail function ownership.

An isolated generation into `generated/conditional-fragments/` completed in
492 seconds and wrote 625 files. A scan of all 311 emitted C++ units found all
19 configured target definitions and zero unresolved-branch `REX_FATAL`
placeholders; the generation operation is verified in SQLite as
`3d924902-d86e-4ebf-bcf2-7cc96efd1f42`, and the byte/config check is
[`config-validation.json`](../logs/acv-rexglue-unresolved-20261021/conditionals/config-validation.json).
This resolves the prior conditional-emission blocker at the code-generation
level only. The game source has not been compiled or launched. Next: point the
Linux CMake build at this generated tree, compile, inspect all compiler/linker
errors, then run the bounded Linux probe. Actual boot and rendering remain
unverified.

The ReXGlue import source audit finds40 warning-only imported-function stubs
among ACV's282 imported functions. Their execution and relevance to startup are
unknown; trace actual calls before implementing game-specific replacements.

## Remaining gates

1. Keep the 19 return-fragment boundaries instruction-backed; exact `.pdata`
   ownership and surrounding function boundaries are still unknown.
2. Reconfigure/build the Linux executable from the isolated conditional-fragment
   source tree; inspect compiler/linker errors and any remaining unsupported PPC
   paths. Do not use `--force` to suppress them.
3. Run the bounded launch with `tmp/iso_root` and output rooted in `tmp/` / `logs/`.
4. Verify entry-point, graphics, resource loading and gameplay from observed logs.
5. Revisit the 40 imported SDK stubs only where an ACV execution trace reaches them.

Windows work waits for a new user request. Full runtime parity, shaders, audio,
input, networking, Xenia/hardware comparison and gameplay remain open.

## Unregistered-function closure, 2026-10-21

Runtime FATALs at `0x822F9E78`, `0x82CEB570`, `0x832C8DD0`, `0x82425360` came from entries
ReXGlue discovery missed. Fixes (all from original bytes; tail/leaf semantics unverified):

- 63 pointer-backed `lwz r3,4(r3); b <target>` thunks (`generate_adjusting_thunks.py`) and leaf
  `0x82CEB570` (`retail_leaves.cpp`). `validate_adjusting_thunks.py` scans all 373 pairs in `.text`
  (269 already generated). Two data-referenced pairs stay unemitted: `0x824FEDC8` -> `0x82500F38`
  (`lwz r3,0(r3); b 0x82746EE0`) and `0x829C6A98` -> `0x829C69E0` -> `0x829C6838` (`bctr` dispatch);
  their targets are outside this thunk class and have not been hit at runtime.
- `propose_data_referenced_boundaries.py` -> `acv-data-referenced-functions.toml`: 414 entries
  (1864 words) from 1098 data-referenced unregistered `.text` addresses; accepted only if the
  separate XenonRecomp registry lists them and an original-byte CFG is contiguous, fully reached
  and exits cleanly. 694 rejected (638 not in registry, 33 interior to `.pdata`/branch labels,
  6 zero words, 17 conditional-exit). Rejected entries may still be hit at runtime.
  Audit: `audit_data_referenced_codegen.py` (0 missing, 0 `REX_FATAL`). Reports in `logs/acv-native-linux/`.
- Bounded Linux launch `20261021T000621326540Z`: 60 s limit reached, exit 0, no FATAL; GPU pipeline
  states are created. Open runtime errors: `system:\` and `dlc_sound:\` devices unmounted,
  `MmAllocatePhysicalMemoryEx: bad protection bits` x2. This does not establish rendering
  correctness or gameplay.

### Corrections and additions, later on 2026-10-21

- The data-referenced set is now 459 entries (2059 words). The earlier "414" figure was
  the first pass, before code-materialized pointers were added and local-label entries removed.
- Two thunk pairs remain unemitted: `0x824FEDC8` (`lwz r3,0(r3); b 0x82746EE0` target class)
  and `0x829C6A98` (`addi; b 0x829C6838`, a `bctr` dispatcher). Not reached at runtime yet.
- The launch past NOW LOADING is observed in `logs/acv-native-linux/20261021T003515443841Z/`.
  The black gameplay screen is not diagnosed; see PROGRESS.md.

## Numeric RenderDoc replay, 2026-10-22

Confirmed, host diagnostics only; no screenshots/image export.
Canonical run [launch.json](../logs/acv-native-linux/20261021T215316118397Z/launch.json)
used `tools/native-rexglue/acv.config`, ARTEM/ARTEM/headless=true; recorded SHA-256
31ef1d2a86eec575c4458ec70d95c36d3edcfe2a5de38e7574d110a082566256 matches the file.
150 s bounded shutdown: child0, no device-lost string in runtime/stdout/stderr;
45 known VFS/protection errors remain. This is not a gameplay acceptance.

Ordinary TriggerCapture produced four RDCs. At110 s, frame73703 records
1326 indexed /40 nonindexed draws,10 dispatches,1570265 indexed elements;
70/90/140 s captures have only1 draw. Explicit Start/End capture path was removed
after device loss; nonzero `--renderdoc-window-ms` is rejected before spawn.

[replay_inspect.cpp](../tools/native-rexglue/replay_inspect.cpp) uses RenderDoc1.46
public virtual replay APIs; local header provenance is in
`logs/renderdoc-replay-headers.json` (runtime evidence stays outside the source repository).
Build (through SQLite logger):

```bash
clang++ -std=c++17 -DRENDERDOC_PLATFORM_LINUX -rdynamic -O1 \
  tools/native-rexglue/replay_inspect.cpp \
  -I tmp/renderdoc-replay-1.46/renderdoc/api/replay \
  -L/usr/lib -lrenderdoc -o tmp/acv_renderdoc_replay
```

`-rdynamic` exports `renderdoc__replay__marker`; without it RenderDoc treated
the executable as a capture target and replay exited143. Marker export and
replay now checked; two upstream rdcstr literal-format warnings remain.
Run `tmp/acv_renderdoc_replay CAPTURE.rdc [EVENT_ID]` through the logger.
Resource `texture_index` is local to this replay inventory, not a guest VA.

At final draw event7374, source1280×720 center RGBA is
[0.00781262,0.0254063,0.0136721,1], maxima
[0.882698,0.722385,0.581623,1]. Output2560×1080 center is
[0.00784314,0.0235294,0.0117647,1], maxima
[0.768627,0.678431,0.545098,1]. This establishes nonzero pixels and a dark
central input sample, not full scene correctness or a gamma root cause.
GetUsage lists only PS-read event7374 for the source; its writer is outside
the recorded usage. A stale-texture defect is only a hypothesis.
[Numeric evidence](../logs/acv-renderdoc/20261021T215316118397Z/acv_frame73703.usage.replay.json).
Dispatch event IDs94,222,295,598,755,5156,5162,5195,5211,5225; preceding
resolve/async boundaries remain to investigate. No clean validation-layer
pass or retail/Xenia parity is claimed.

FIFO control run20261021T220305240062Z: confirmed present mode2 (FIFO),
canonical config SHA unchanged, child0 at150 s,45 VFS/protect errors remain;
no device-lost string.70 s frame10222 has400 indexed/35 nonindexed/5 dispatch;
90/110/140 s captures each contain only1 draw. Not a matched-scene timing or
render-correctness comparison. Replay input source channel fractions below
1/32 are R0.932925/G0.912833/B0.973901; all three histograms contain921600
pixels. This establishes a mostly dark source, not its cause.
[Replay/histograms](../logs/acv-renderdoc/20261021T220305240062Z/acv_frame10222.replay.json).

New opt-in `probe_linux.py --renderdoc-frames=1..8` uses public
`TriggerMultiFrameCapture` (renderdoc_app.h:515-516), no explicit Start/End.
Default1 preserves ordinary captures; optional [capture] renderdoc_frames in
acv.config is honored unless CLI overrides it (canonical file unchanged). Six fake-public-API boundary cases
(default,1,8,0,9,bad) and four parser rejection checks passed, operation
5aa260c0-3975-4ad4-9fa7-b9e1a8df7325. Incremental rebuild changed only
renderdoc_trigger.cpp and relinked; operation951073e9-5ec4-4453-9eaf-9a7f0c832d22.
Eight-frame125 s experiment20261021T220733169087Z completed140 s:8 consecutive
RDCs, child0,45 known VFS/protect errors, no device-lost string, config SHA
unchanged. Frame16580 contains1325 indexed/682 nonindexed/64 dispatch;
frame16581 includes producer image writes: image3 copyDst event90 and
image5 CS_RWResource event95. Final draw109 samples image4, a different
mailbox image. This establishes the asynchronous boundary, not a stale-image
bug. [Writer evidence](../logs/acv-renderdoc/20261021T220733169087Z/acv_frame16581.analysis.json).

[analyze_capture.py](../tools/native-rexglue/analyze_capture.py) converts an RDC
to XML in tmp/ and writes a numeric `.analysis.json` beside the source. It
records full RDC SHA-256, checks source stability, reuses the command counter
and optionally (`--replay [--event N]`) runs the inspector with binary SHA.
Run through the SQLite logger; replay after the game exits to avoid concurrent
GPU load. No image/preview export. Command-only integration matched
400 indexed/35 nonindexed/5 dispatch on frame10222.
Inspector uses native component type, mip-relative center, and filters the
selected action's compute/pixel stage; descriptor inventory is not proof of
actual shader accesses. Buffer indices are replay-local, UINT64_MAX descriptor
size denotes the API's whole-buffer range; never allocate that reported size.

### Producer gamma boundary: confirmed observations

Frame16581 event95 in run20261021T220733169087Z binds UNorm source image3
and UNorm destination image5; neither descriptor view is sRGB. Source center
[9,14,5]/255 becomes [8,26,14]/1023. Captured buffer2 at offset5120, size1024
contains256 packed10-bit table entries, equal channels. Selected entries
index5→8,9→14,14→26,32→93,64→251,128→582,192→925,255→1020.
Thus the selected center agrees exactly with the captured LUT after R/B
swizzle. Shadow darkening is observed at this boundary; an incorrect LUT,
wrong game brightness setting, or GPU shader defect is not established.
[Actual views/LUT/replay data](../logs/acv-renderdoc/20261021T220733169087Z/acv_frame16581.event95.analysis.json),
operationbb708328-7a5e-4c68-a532-bacfd5dccfa8,
decode operationee7f0199-a84a-4940-9170-58184c25f1da.
SDK gamma table conversion source: graphics/vulkan/command_processor.cpp
:328-342/:373-396; RequestSwapTexture reads fetch0 and requests a nonsigned
view, texture_cache.cpp:989-1015. Desktop BT.709 interpretation is still open.

Ghidra read-only instruction evidence (no function-body repair):
0x82BB6C20 calls VdGetCurrentDisplayGamma import0x83537294;
0x82BB6BF8 constructs/caches1024 u16 transfer entries in two tables selected
by direction, cache bases0x838B9A38/+0x800. Loop0x82BB6CA4–0x82BB6D64
selects helpers0x82BB6A98/0x82BB6B48 by returned display type and direction,
rounds/clamps to0..1023. Helpers have piecewise multiplication/power patterns;
constants read BE at0x820FE578–0x820FE5FF agree with sRGB/BT.709 transfer
parameters. Callee0x830D6DF0 numerical parity is not verified here.
0x82BB6D80 remaps3×256 u16 input samples via that table; direct calls
0x82BA1074 (direction0) and0x82BC7214 (direction1). Function metadata is
incomplete; evidence is the actual instructions, not truncated pseudocode.
Operationsf4ac1009-20dd-41f8-9ceb-cc94e45a62ed,
6937489f-ff76-4bc1-ab8f-a3f14c5b4bd1,
0480aad0-ad9f-41ea-9cba-83564a5617d6,
945c3624-1591-4c27-883c-82605ddc24aa.

Control with display-gamma type1 run20261021T221423893732Z completed140 s,
8 RDC, child0,45 VFS/protect errors; two actual hook calls2→1 and unchanged
canonical config SHA confirmed. Frame15264 compute2500 has the same central
input[9,14,5]/255, output[24,68,40]/1023. LUT5→24,9→40,14→68,64→312,
128→624,192→936. This confirms a display-type-dependent shadow attenuation
at the producer LUT. Source/dest views are UNorm, not sRGB. Scene identity
elsewhere is not proven; correct world rendering remains open.
[Control LUT](../logs/acv-renderdoc/20261021T221423893732Z/acv_frame15264.event2500.analysis.json),
comparison operationf40c4949-bb81-4702-97b9-512648c67425. Default config/SDK
gamma unchanged; previous negative visual result does not mean no numeric effect.

At indexed candidate event7302 (6195 elements) in frame73703,64 post-VS float4
slots are finite, w522..581; sampled clip positions fall inside Vulkan's clip
volume. Bound512×256 and1024×512 texture inputs have nonzero ranges.
This rules out universal collapsed geometry/empty material data for that draw,
not incorrect shading/depth or other draws. [Post-VS data](../logs/acv-renderdoc/20261021T215316118397Z/acv_frame73703.event7302.analysis.json),
operationaa0f4840-b3a9-4687-bc2d-93b602110d32. Inspector `--mesh`/analyzer
`--replay --event N --mesh` sample first64 post-VS slots, instance0/view0; no
mesh image output.

Usage-only replay of the same event (`tools/native-rexglue/replay_targets.cpp`,
`replay_texture_usage.cpp`; `GetUsage` + action `outputs`/`depthOut`; no pixels
exported). Event7302 has no color or depth action outputs, so its writes go to
storage buffers. Buffer indices are local. PS_RWResource/CS_RWResource/VS_RWResource
are storage bindings: they do not separate reads from writes, so these usage codes
alone prove neither "written" nor "never read".

Frame73703, buffer1 (10 MiB, EDRAM-size candidate): PS_RW in 1352 draws (23..7302).
Seven compute dispatches bind buffer1 together with buffer0 (512 MiB, guest-memory
mirror candidate): 94, 222, 295, 598, 755, 5156, 5211. PS draws between them:
28/15/17/71/21/753/4 (before 94 … before 5156 … 5156–5211), then 443 draws after
5211 up to 7302 plus 13 more to 7353, with no later dispatch binding buffer1 in
this capture. Dispatches 5162, 5195, 5225 bind buffer0 only and are followed by
vkCmdCopyBufferToImage at 5164/5197/5227 into textures5 (1280×720), 10 and 11,
which PS then reads (candidate texture-load path). Present draw7374 clears and
writes texture2 (2560×1080) and reads texture3 (1280×720), which has no write
usage anywhere in the capture. This is a binding-level candidate chain
(draws → EDRAM → dispatch 5156/5211 → texture load → PS), not proven resolve
semantics: shader identity and data direction are unchecked.
Local buffer evidence: `logs/acv-renderdoc/20261021T215316118397Z/acv_frame73703.event7302.edram_usage.json`,
operations ea22716b-7d7b-4b0f-a154-ce208f6f691c (verified) and
26ececb7-28a3-4eb0-ab50-f959a8da2d32 (textures). An earlier note here claimed
"no read usage, so no resolve"; that inference was wrong for the reason above.

Follow-up run 20261021T224404891347Z (captures at 100–140 s, 2 presents each,
10 RDC; operation be036c98 exit0, marked failed by the probe's runtime dlc_*
error classification): all-buffer scan (d447a003) finds only CopySrc staging
buffers and one CS_Res buffer as read-only bindings. Texture scan (238cbc21):
in every capture the only texture writers are CopyDst uploads, the present
target (Clear+ColorTarget at the last draw) and, in four second-present frames
(42853, 44672, 46810, 48599), one compute pass writing a 1280×720 texture. In
all 10 frames the texture read by the last PS event has no write usage in that
capture. Frames 40146, 44671, 48598 contain no 10/512 MiB buffers (UI only).

Dispatch direction resolved from SPIR-V (`replay_dispatch_shader.cpp`, evidence
[73703](../logs/acv-renderdoc/20261021T215316118397Z/acv_frame73703.dispatch_bindings.json), [42853](../logs/acv-renderdoc/20261021T224404891347Z/acv_frame42853.dispatch_bindings.json);
operations 60774c34-5d57-4c9c-938e-5eeb21ce5ad4, 56d34677-b4fc-48f4-afa1-405726838fd3). RenderDoc
reflection reports these storage buffers as read-write, but the module decorates
set0/binding0 NonWritable and set1/binding0 NonReadable. Frame73703 dispatches
94, 222, 295, 598, 755, 5156, 5211 (8×8) read buffer1 whole (EDRAM-size) and write
buffer0 ranges (302178304+7249920, 302301184, 309551104, 309673984,
328482816+589824, 279470080+3768320, 299720704+983040): EDRAM→guest-memory
resolve candidates. Dispatches 5162/5195/5225 (4×32) read the same buffer0 ranges
and write staging buffer3 (3686400/14745600/921600 B = 1280×720/1920×1920/640×360
×4), followed by CopyBufferToImage 5164/5197/5227 into textures5/10/11: texture
loads. Frame73703 therefore has resolve→load→PS-read for textures5 and 11, but
the 443+13 draws after the last resolve (5211) have none, and the present draw7374
reads texture3 without an in-frame load. Frame42853 shows the full path at
binding level: resolve 7544 → buffer0 [283238400, +3768320], load 7580 from the
same range, CopyDst texture3 at 7582, swap compute 7587, present draw 7601
(earlier resolve 7358 → [275701760,+3768320], loaded 7364). Slots 275701760,
279470080, 283238400 are spaced 3768320 B (triple-buffer pattern, unverified
against the guest swap packet). The 1280×720 texture read by the final draw of
42853 already has min/center [0,0,0] with max 0.9971, so the dark centre exists
before the present and swap post-effect; the stage that produces it (EDRAM
contents before the resolve) is not yet measured. Whether frame73703 is a repeat
present of an unchanged front buffer or a lost resolve is open.

Buffer-range statistics (`replay_buffer_stats.cpp`, layout-independent byte-lane
counts, frame42853; operations 20992b6e-ac81-4e13-ac72-b87fa37c7dc8, 36bbe664-0d01-4556-be7c-c6fb9cf2cf92,
7bee6d6b-32a7-4d58-995b-282b483eef33, cf298eea-8044-4df7-b2a4-9b362936a4e0). Buffer1 at event7543 (before the
resolve): 2 621 440 words, 17% zero, 78% with a lane ≥128, lane means
[3.0, 54.6, 56.5, 144.9] — EDRAM as a whole is not dark, but it also holds depth and
other targets, so this is not the colour target. Resolve destination slot
[283238400,+3768320]: identical statistics at events1, 7543, 7544 and 7581
(lane means [6.28, 5.96, 3.69, 202.29], max [255,194,99,255]); slot
[275701760,+3768320] differs at 7357 and equals its event1 value again at 7358
(means [188.35, 34.26, 87.27, 14.9]). Reading: the resolve output equals what the
slot already contained, as for a repeating (static) frame; the presented slot is
near-black in colour lanes, the other slot is not. Lane order/endianness and tiling
are not decoded, so lanes are not mapped to RGBA, and these captures do not show
whether the dark image is a fade/transition or the scene-shading defect. Not
shown: any per-pixel cause.

FSI vs FBO bisection (user report: gameplay stays black). FSI run
20261021T224404891347Z, source texture before the present draw (histogram over
[0,1], 4 swap frames): per-channel mean 0.4–3.8 %, 44–99 % of values in the lowest
3 of 256 bins, hues differ between frames (not a uniform fade), max ≈ 0.99.
FBO run 20261021T231105435173Z (`--extra=--render_target_path_vulkan=fbo`, same
captures at 100–140 s, 10 RDC, child exit0; probe flagged failed by runtime dlc_*
noise, operation c81fbe1e-fb5e-474e-90b2-cee21ab6b4a4): in all 10 frames the source
and output textures are exactly zero (mean 0, 100 % black), including frames with
1692 and 2471 draws. Target content after each target's last writer
(`replay_target_content.cpp`, frames 77730/79584,
[data](../logs/acv-renderdoc/20261021T231105435173Z/acv_frame79584.target_content.json),
operations 5984e87f-785e-4e3e-9b5a-3975077373f3, a394aa74-0400-49cf-8541-810a7dd65091): the main colour target
(1280×2048 UNorm, 1037–1091 writer draws) already has hist means
[0.014, 0.006, 0.006] with max up to [1.0, 0.56, 0.14]; other targets differ
(e.g. 640×4096 mean [0.54, 0.22, 0.22], 640×1024 all zero). The 1280×2048 extent
includes unused rows, so means over the 1280×720 area are about 2.8× higher —
still dark. Reading: the draw output is already dark before resolve, gamma and
present, so those stages are not the primary cause; the FBO path additionally
loses the image on its way to the front buffer (separate defect, FSI remains the
usable profile). Cause inside the draws (texture inputs, lighting constants,
shader translation or guest-computed values) is not identified.

Source note: Vulkan draws whose pipeline is still compiling are skipped silently
(`async_shader_compilation=true` default, `command_processor.cpp:3864`) and such a
frame is not presented (`vulkan_async_skip_incomplete_frames=true`). The runtime
log has one such warning (pipelines are created mostly in the first ~10 s), so
this is a side observation, not shown to affect the 100–140 s captures.

Probe stdout/stderr now flush to disk per received chunk, making live hook
evidence readable before EOF; pipe boundary test operation
2ffdf468-cd00-4446-94ef-95db4835fccb. No settings changed.

### Black material input traced, 2026-10-22

[Numeric evidence](../logs/acv-runtime-triage-20261022/findings.json). Requested run
20261022T144521925243Z exits normally after window close; it has no RDC because
it ends before the scheduled 110 s capture. Its 45 error messages alone do not
identify the dark-scene cause.

FBO capture acv_frame77730, draw3790 (4158 elements), pixel640,512: RenderDoc
trace has1482 steps. Texture0 sample _256 is [0,0,0,0], texture7 _434 is [1,0,0,1],
final colour is [0,0,0,1]. Texture246 (1024x512) is entirely zero; neighbouring
draws3795/3800 also bind entirely zero base images247/244. These are three draws,
not proof about every material or the complete FSI scene.

Fetch0 words are 88000002 1F6B3054 003FE3FF 06A80D10 003E0803 00000200:
valid type2, tiled DXT4_5/BC3, endian8-in-16, physical base0x1F6B3000.
The GPU shared-memory mirror at this address contains524288 bytes:32768 identical
16-byte blocks 00ff9249492424920000000000000000. After the declared byte swap,
alpha endpoints are255/0 with every alpha index1; colour endpoints and indices
are zero. This encodes transparent black, consistent with the sampled image.
This is a GPU mirror dump, not a live CPU-memory dump: missing upload/invalidation
and a blank guest-produced texture remain distinct hypotheses.

The old debugger limitation is now identified: reflection reports unsupported
RoundingModeRTE. replay_debug_pixel.cpp fixes the grid-step argument and a
post-FreeTrace read; --debug-relax-rte makes a temporary replay replacement
without the explicit RTE capability/execution mode and cleans it up afterwards.
Original/replacement GPU colour at the selected pixel is identically [0,0,0,1]
(operation a4c500a5-2bad-440b-b0ea-95c67bcd8c8b). This enables investigation,
not a runtime patch or an FP-equivalence claim; FSI/interlock support is unproven.

Synchronous readback probe20261022T150409540603Z uses canonical acv.config,
FSI, gamma_render_target_as_unorm16=false, readback_resolve=full and
readback_memexport_fast=false (explicit diagnostic overrides). It ends at85 s,
child exit0,8 RDC; the logger flags existing VFS/allocation errors. Frame66671
has967 indexed draws. Source RGB histogram means are4.26/4.40/4.04 %, with
79.6-80.1 % in the lowest3 bins; output means3.18/3.31/3.01 %.
This is not a matched-scene comparison or an accepted rendering fix.

SDK evidence: graphics/command_processor.cpp:48-69/:172-190 defines resolve modes
and default enabled, delayed memexport; vulkan/command_processor.cpp:4264-4383
tracks readiness of delayed memexport slots, :4403-4689 implements staging
resolve readback. Thus [Xenia Edge issue223](https://github.com/has207/xenia-edge/issues/223)
is relevant research, not evidence that its missing-host-buffer defect exists here.

### Consolidated Linux test build, 2026-10-22

User requested consolidation and recompilation. Build operation
e7ce11db-28dc-4d0b-b173-795a9fe706e2 completed with exit0; existing generated
translations were retained (codegen reported module up to date). Host sources
were recompiled and the executable relinked. The canonical app header now
propagates through CMake, the host revision appears in runtime.log, and
build_linux.py records ELF/runtime/plugin/config hashes in acv-test-build.json.

ELF SHA-256:89a85871940278e8b096193b558c2540470857665c12ba15dc67ae6f830cabfd.
Host revision:51bbb6955a6bcd8735ed297e2abcee87f79334e4adb0be126d429139631ab13f.
Previous ELF and source files are preserved in logs/acv-test-build-20261022/before/.
The canonical acv.config hash remains31ef1d2a86eec575c4458ec70d95c36d3edcfe2a5de38e7574d110a082566256.

The12-second run20261022T151046740018Z logs this revision and exits0 on the
bounded close. No fatal/device-lost marker;15 existing VFS/allocation errors
remain, and the probe/logger therefore reports failure. This verifies startup
and artifact identity only, not dark-scene resolution or gameplay.
[Verification](../logs/acv-test-build-20261022/verification.json).
[Manual test launcher](../tools/native-rexglue/test_linux.sh) uses the canonical
config and existing profile, with a300-second default bound.

### UTF-8 BOM configuration support, 2026-10-22

The user supplied [Xenia config fix a192b94](https://github.com/Atvaark/xenia/commit/a192b94df4266bf94cf9a04b4289e0534227186a).
The same failure reproduced in probe_linux.py: prefixing canonical acv.config
with UTF-8 BOM raised ConfigParser MissingSectionHeaderError. The loader now
decodes utf-8-sig, retaining the original bytes for the metadata hash.
Actual CLI guard checks confirm identical plain/BOM configs both reach the
duration guard, and missing config remains rejected; no game was launched.
[Before](../logs/acv-user-references-20261022/bom-before.json),
[after](../logs/acv-user-references-20261022/bom-after.json).
Canonical config and native ELF remain unchanged; no new graphics fix is claimed.

### Shared-memory compute-write access scope, 2026-10-22

A source-level synchronization defect was found in SDK
`src/graphics/vulkan/shared_memory.cpp`, `VulkanSharedMemory::GetUsageMasks`:
`kComputeWrite` used `VK_ACCESS_SHADER_READ_BIT`. The unscaled resolve dispatch
in `vulkan/render_target_cache.cpp:1258` writes shared memory, and `Use` derives
both source and destination barrier access masks from this helper. The source
scope therefore omitted shader writes when transitioning to texture-load or
transfer reads, and consecutive compute writes lacked write scopes.
The SDK's separate EDRAM compute-write mapping already uses SHADER_WRITE.
The installed plugin disassembly at0x17B280 confirmed the old compute-write
case returns0x20 (SHADER_READ), not0x40 (SHADER_WRITE).

The helper now returns `VK_ACCESS_SHADER_WRITE_BIT`. Regression
[script](../tools/native-rexglue/test_shared_memory_barriers.py) compiles the
actual `Use`/`GetUsageMasks` bodies and actual Usage enum with a recording
command processor, checks compute→read, compute→compute, transfer→read and
unchanged-read behavior under UBSan, and rejects the archived old source.
Operations8561bfb2-207c-424c-a743-0b33bc6ffd3b and
82db8056-3843-4551-93bd-2b6114d3da6a passed. This proves host access-scope
behavior, not GPU execution correctness or a fix for the dark scene.
Local Khronos synchronization.adoc access-scope rules identify the omitted
write accesses as outside the memory dependency.

[Evidence directory](../logs/acv-gpu-sync-20261022/) contains original source,
checkpoint hashes and pinned glslang-header provenance. The
[private plugin build](../tools/native-rexglue/gpu-plugin/CMakeLists.txt) reuses
the SDK GPU source inventory and installed runtime/static dependencies;
internal glslang headers match SDK submodule f4f1d8a352ca1908943aea2ad8c54b39b4879080.
Installed SDK is preserved. Isolated
fixed-plugin build SHA86c7d50359133a28efc0f4f2f84096bc2b54e53f00c09864dcbf0821f1928a50
is staged in `tmp/acv-gpu-sync-test`. Process maps confirmed that exact path was
loaded. The85-second run20261022T155908068499Z exited0 with8 RDC and15 existing
VFS/allocation errors. Captures had1/59 draw actions, not a gameplay comparison.
[Build manifest](../logs/acv-gpu-sync-20261022/build.json),
[startup verification](../logs/acv-gpu-sync-20261022/verification.json).

### CPU-origin texture upload compared with GPU mirror, 2026-10-22

An optional private plugin variant records the intersection of a selected
physical range with each CPU→staging memcpy in VulkanSharedMemory::UploadRanges.
It snapshots the bytes copied to staging before the deferred GPU copy is queued,
plus submission, physical offset, byte count and source-buffer offset. It does
not read later live CPU memory. Production SDK source has no diagnostic changes;
[CMake](../tools/native-rexglue/gpu-plugin/CMakeLists.txt) builds a separate
instrumented source copy with `ACV_CAPTURE_UPLOADS=ON` and
[helper](../tools/native-rexglue/gpu-plugin/upload_probe.h). Limits:32 snapshots,
selected range at most8 MiB. Tests checked exact intersection/metadata, count
limit and rejected out-of-bounds physical ranges under ASan/UBSan
(operation8276d35f-d1a7-4cfd-8e3b-a7cb025badf8).

Run20261022T160513877989Z used canonical ARTEM config, copied profile, FSI,
fixed access scope, explicit timed internal SDL keys8:Return then Space at
18/28/38/48/58 s.110 s ended child0,12 RDC,45 existing VFS/allocation errors;
no fatal/device-lost markers observed. This is not a clean runtime pass.
At95 s, frame58287 has2661 draw actions/64 dispatches, while final source center
remains [0.00781262,0.0254063,0.0136721]. The access-scope correction has not
established a fix for dark gameplay; the scene is not matched to a baseline.
[Replay summary](../logs/acv-gpu-sync-20261022/upload-replay-summary.json).

Exactly one upload intersected physical0x1F6B3000..0x1F733000: submission2889,
524288 bytes,32768 repeated blocks00ff9249492424920000000000000000. Its SHA-256 is
e4428980736f99488a28373777a97ddf74eba5b2a63a21fc53d863c162e70777.
At frame58287 event13126, unique512-MiB GPU shared buffer inventory index0 has
exactly the same bytes at that range. This is also the hash from the old FBO
sample. [CPU/GPU comparison](../logs/acv-gpu-sync-20261022/cpu-gpu-comparison.json),
[raw snapshots](../logs/acv-gpu-sync-20261022/uploads-first/).
The source was therefore already transparent-black before this CPU-origin
upload. The copy event itself was outside sampled frames; this is not an
atomic CPU/GPU snapshot, proof of current material identity, or a general
exclusion of cache/invalidation bugs. The writer and material purpose remain open.

A manifest filter found60 extracted1024×512 BC3 TPF candidates. None matched
the raw mip0 prefix; none contained8 repeated target blocks in raw/8-in-16/
8-in-32 byte order. [Prefix comparison](../logs/acv-gpu-sync-20261022/asset-prefix-comparison.json),
[pattern control](../logs/acv-gpu-sync-20261022/asset-pattern-control.json).
Only those60 candidates were read; no ISO rescan. This does not prove a
procedurally generated placeholder or an asset decoding failure.

`replay_buffer_dump` now accepts `auto-shared` only for a unique512-MiB buffer,
checks range arithmetic without overflow, rejects negative/overflowed numbers,
and requires a complete read. Existing numeric indices remain supported.
The actual capture comparison validated its automatic selection and524288-byte
read (operation96f973b8-df22-4edf-8ec5-316ad19167e6).

### Allocation callers and current test package, 2026-10-22

Opt-in `ACV_TRACE_PHYSICAL_ALLOCS=1` in the handwritten import shim records at
most128 calls, original/effective protection, return value and LR. For the
verified wrapper0x82B90210 only, it also reads the saved parent LR at current
SP+0x58: raw prologue stores LR at old SP-8 and reserves0x60 bytes.
Default execution forwards unchanged; experimental READWRITE normalization
still requires its separate toggle.

Trace run20261022T161735195655Z identifies callers0x82AEF00C/0x82AEF028,
return LRs0x82AEF010/0x82AEF02C, for the0x40000/0x800000 failures. Raw PPC shows
results stored at SP+0x78/+0x80, copied into a parameter block at SP+0x90,
fields+0x54/+0x5C, then passed in r4 to0x82AF1D38. Field names and downstream
handling are not established. [Caller evidence](../logs/acv-gpu-sync-20261022/allocation/callers.json).
Ghidra's saved listing skips this region and its body at0x82AEEF40 is only20
bytes: raw read-bytes plus full Capstone decoding were used instead. The live
DB was reopened from its existing saved program with --no-analyze and not repaired.

The earlier regular test ELF had SHA
c11292b4de6822e2724a8c0e880bc69d8469632cbb2bb6a1a98a2ec1d2114ded,
host revision de3cd420ba6c080e1901c149eeaddfff5ba38f9bc91c40cbe18d776a8d5c2765.
The corrected nondiagnostic GPU plugin SHA86c7d503… is staged next to it;
the upload-snapshot plugin is separate. The final12-second startup exited0
with15 existing VFS/allocation errors. [Final package verification](../logs/acv-gpu-sync-20261022/final-test-build.json).
The older ELF and GPU checkpoints remain in the earlier evidence/private copies.

`build_linux.py --gpu-plugin-artifact tmp/acv-gpu-sync-test/librexgpu-xenos.so`
stages the explicitly selected private plugin after the incremental host build
and records its source path/hash. Invalid or outside-tmp artifact paths fail
before building. Use this option when preserving the SDK correction across
future rebuilds; normal SDK staging alone may restore the installed old plugin.
No installed SDK files or canonical config bytes were changed.

Allocation normalization comparison run20261022T161947900981Z used the same
scripted SDL schedule and profile origin, corrected GPU plugin and only the
existing --compat-mm-wc-rw override. Both initial allocations returned nonzero
(0xBFC60000/0xBF460000), protection diagnostics disappeared, child0/110 s,
12 RDC and13 VFS errors. However the95-second captures have1/59 draw actions,
while the baseline has2661: scenarios do not match. Neither a shading fix nor
an allocation-induced regression is established; keep normalization opt-in.
[Comparison result](../logs/acv-gpu-sync-20261022/allocation/ab-result.json).

### Texture converter provenance and FSI consumer, 2026-10-22

The replay fetch scanner [replay_find_fetch.cpp](../tools/native-rexglue/replay_find_fetch.cpp)
validated the old FBO draw3790 oracle, then inspected all2661 FSI draws in
frame58287. Event5940 is the only populated matching fetch and its pixel shader
actually declares slot0:4158 indices,1024x512 BC3, physical0x1F6B3000. Its decoded
texture is uniformly RGBA0; post-VS positions are finite and vary in perspective
W. This establishes a consumer in the FSI scene, not its material name or sole
responsibility for the darkness. [Fetch evidence](../logs/acv-texture-writer-20261022/fsi-fetches.json),
[consumer](../logs/acv-texture-writer-20261022/fsi-consumer.json).

Opt-in ACV_TRACE_TEXTURE_WRITES records at most256 physical invalidation rows;
run20261022T164416141605Z emitted128 first-page-write callbacks, all with saved
LR0x82C83164. Raw PPC identifies copy wrapper0x82C83100 calling0x830D54A8 and
converter finish0x82C83DE8 calling the wrapper at0x82C83EC0. The assertion path at
0x82115AA0 identifies DLTextureConverter_Xbox360Dx.cpp. Saved LR is a context
snapshot, not the faulting guest PC. Live Ghidra listing is incomplete; read-bytes
and Capstone supply the instructions, without modifying the database.

The handwritten weak-entry override always calls original __imp__sub_82C83100.
With tracing enabled only, it snapshots the verified converter copy before the
original memcpy. In run20261022T164946627340Z its524288-byte output equals both
CPU staging chunks in submission2724, hash e4428980….
[Byte comparison and nested asset inventory](../logs/acv-texture-writer-20261022/nested-texture-index.json).
No title function or guest buffer is replaced by the diagnostic.

Raw0x82C833F8/0x82C836D8 establish that converter+0x14 points to a per-level
16-byte table: source pitch/pointer, destination pitch/pointer. The historical
trace filename .source-object.bin contains this table, not an identified resource
class. A later X11 run captured equal512KiB input staging and output, both
uniform transparent-black blocks. Its input snapshot is taken after conversion;
it alone does not establish the pre-conversion bytes.
[Comparison](../logs/acv-texture-writer-20261022/input-output-comparison.json).
The before-input-copy probe at return0x82C8391C then captured the original
source0xFE111000 in run20261022T165910395240Z. The same converter0x708BF080
produced the watched output; all three512KiB dumps are byte-identical, hash
e4428980…. This moves the unresolved producer boundary before converter staging.
[Before-copy comparison](../logs/acv-texture-writer-20261022/origin-comparison.json),
[linked findings](../logs/acv-texture-writer-20261022/writer-findings.json).
The original resource's observed format field+0x20 is9, destination8; field names
remain provisional. A128-byte neighborhood dump does not establish object extent
or ownership of every adjacent word. No GPU descriptor in that neighborhood is
attributed to the source without further evidence. SDK PhysicalHeap mapping
maps0xFE111000 to physical0x1E112000 (E alias adds0x1000).
ACV_TEXTURE_WATCH_START optionally selects a page-aligned physical start within
512MiB for the same bounded write watch. All tracing remains off by default;
the game copy is forwarded unchanged.

The full extracted BND3 manifests cover33165+6503 files. Selective parsing found
105 supported TPF containers/320 textures, six1024x512 entries, no matching
payload; the earlier direct/DCX inventory had60 BC3 candidates with no match.
This does not establish procedural origin or intentional placeholder use.
Wayland run20261022T165626380279Z did not reach the converter despite scheduled
SDL keys; X11 did. Do not equate elapsed time or input scheduling with scene
identity, or claim a general Wayland input defect from this observation.

The original-source physical watch then emitted128 first-write rows with saved
LR0x82B8F6A0. Raw PPC identifies aligned copy dispatcher0x82B8F618, called via
0x82CD81C8. A separate before-copy override captured527360 source bytes; its
first524288 bytes again hash e4428980…. Callsite0x82C82268 belongs to finish
0x82C82130; its assertion path0x82115988 names
DLTextureCompressor_Xbox360Dx.cpp. Thus the blank source is already compressor
output; original disk data or the compressor input has not yet been identified.
[Physical writer](../logs/acv-texture-writer-20261022/origin-writer.json),
[producer snapshot](../logs/acv-texture-writer-20261022/producer-input.json).
Stack code-like words are only candidates, not a reconstructed call stack;
0x82C82268's actual instructions independently verify the copy call.

A paired boundary probe now captures source0x432B4920 immediately before
0x82C82090 and again before finish0x82C82130. The two2097152-byte inputs are
identical and entirely zero, SHA5647f05e…. The same converter0x708BF080 outputs
524288 transparent-black BC3 bytes, SHAe4428980…. Caller0x82948C78 invokes
compression;0x82948C90 invokes finish (raw82948be0.asm). This excludes loss
inside this compression call for the observed zero input; it does not identify
why the input is blank or whether this material should contain an image.
[Paired boundary proof](../logs/acv-texture-writer-20261022/compressor-boundary-comparison.json).
The latest incremental ELF and fixed nondiagnostic plugin are recorded in
[current manifest](../logs/acv-texture-writer-20261022/current-test-build.json);
all new overrides are disabled unless ACV_TRACE_TEXTURE_WRITES is explicitly set.

### Resolve/readback audit and preserved checkpoint, 2026-10-22

Progress checkpoint: [manifest](../logs/acv-readback-checkpoint-20261022T171359Z/manifest.json),
[verified archive](../logs/acv-readback-checkpoint-20261022T171359Z/verification.json).
Sources/state are in sources-and-state.tar.gz; the ELF/runtime/fixed plugin are
copied separately to tmp/acv-readback-checkpoint-20261022T171359Z. This preserves
artifacts and investigation context, not a suspended OS process.

The customization GPU-baking/readback explanation supplied by the user is the
next working hypothesis. Zero compressor input is proven; no particular Resolve
has yet been tied to its source. The earlier compute-write barrier fix and
CPU-to-GPU byte equality do not exclude every upstream synchronization issue.
Previous full resolve/readback experiments did not establish a shading fix.

SDK VulkanCommandProcessor::IssueCopy (command_processor.cpp:4384) routes copy
mode into VulkanRenderTargetCache::Resolve (:1127). The latter emits a compute
EDRAM-to-guest-format resolve, tracks destination extent and marks texture ranges
resolved (:1289). This is not necessarily a plain vkCmdResolveImage operation.
IssueCopy_ReadbackResolvePath (:4403) copies the resulting buffer into staging,
queues TRANSFER_WRITE→HOST_READ (:4648), awaits completion for full mode (:4657),
invalidates mapped staging (:4680), then memcpy to guest physical memory (:4684).
The VkResult of invalidation is currently ignored there. Fast/some modes may read
the alternate slot; some copies only on cache miss (:4672). This resolve branch
has no explicit per-slot completion check at that point, unlike memexport's
submission_written check (:4357). Check surrounding completion guarantees before
calling this a demonstrated race. These are source-review findings, not an ACV
root-cause claim.

Trace hooks should correlate allocation generation, Resolve physical extent,
registers/format/pitch/rectangle, submission ID/completion, selected staging slot,
readback mode/cache decision, invalidation result and guest copy bytes. Match
these to the original resource and compressor input, not merely the final BC3
physical address. Dump GPU resolve bytes, completed staging bytes and copied guest
bytes at their actual boundaries; an absent Resolve or an already-zero GPU source
requires producer/state investigation rather than additional host barriers.

Reference helper [readback_reference.hpp](../tools/native-rexglue/readback_reference.hpp)
implements same-queue compute-write→transfer-read, copy, transfer-write→host-read,
submit/fence wait, noncoherent invalidation and CPU publication. It is not wired
into the runtime and requires owned dedicated fully mapped staging, valid ranges,
external queue synchronization, an unused unsignaled fence and retained resources
on timeout. FSI EDRAM fragment-write→compute-read dependency precedes this helper;
other queues need semaphore/ownership handling. A GPU buffer and guest CPU memory
are separate copies: vkInvalidateMappedMemoryRanges alone cannot populate guest
memory or refresh an emulator generation/cache entry.

[Host contract tests](../tools/native-rexglue/test_readback_reference.cpp) compile
with ASan/UBSan and check barrier scopes, waiting before invalidation, no CPU
publication on wait timeout/device-lost/invalidate failure, and coherent memory
still waiting. Operation62946412-11b8-494a-8b29-72c5df37623e passed. This is not a
real-driver test or a new game build. Official reference:
https://docs.vulkan.org/refpages/latest/refpages/source/vkInvalidateMappedMemoryRanges.html
and local Vulkan-Docs/chapters/memory.adoc:5267-5292.

### Accepted ACV resolve-readback profile correction, 2026-10-22

This result supersedes the earlier "readback is only a hypothesis" boundary for
this material. Before compressor input copy0x82C82514, the source resource at
0x4350A980 points to native header0x4350A930 and guest pixels0xFE191000:
physical0x1E192000,1024x512,2MiB, observed engine format0x2E. Native fetch format6
is linear8_8_8_8/endian8-in-32. This is the actual RGBA source, not the later BC3
copy at physical0x1F6B3000.

Paired identical-profile-origin runs use the same original resource/native-header
bytes: default disabled readback produces2MiB of zeros; --readback_resolve=full
produces2066418 nonzero bytes, SHA52fdd6d7…. Compressor input before/after matches
the original snapshot; output BC3 becomes varied. [Copy-boundary comparison](../logs/acv-readback-origin-20261022/ab-chain.json).
The converter may transform/reorder bytes; do not assert byte equality across a
conversion or between different update generations. Its captured input/output
preserve the16-byte block multiset, while the first converter snapshot differs
from the later upload. Uniform old data had hidden this distinction.

The actual fix adds --readback_resolve=full to probe_linux.py's acv-fsi profile.
Explicit SDK config and --extra retain precedence; sdk profile is unchanged.
Canonical ARTEM/headless config bytes remain unchanged. No hardcoded physical
address controls production behavior. No new global fence patch was necessary:
SDK full mode already copies Resolve results, waits for queue completion,
invalidates mapped readback memory and publishes guest bytes. Disabled mode
skipped this CPU update, so the title compressed a zero allocation.

Default-profile validation run20261022T172345196638Z:120s, child0,8 RDC and45
existing VFS/allocation errors. CPU-origin upload submission25699 contains varied
BC3 at0x1F6B3000, SHA00c279ff…. In frame34282/event75 a4158-index FSI draw actually
declares and samples slot0 with that fetch. Its512KiB GPU shared-memory bytes
match the same-run upload exactly. Decoded texture233 is no longer all-zero:
max RGBA[1,0.666667,0.709804,0.709804], center
[0.223529,0.215686,0.223529,0.14902].
[GPU proof](../logs/acv-readback-origin-20261022/gpu-material-proof.json).
This accepts restored dynamic texture content, not global gameplay/visual parity.
The full mode blocks for readback and performance remains to be measured.

Incremental build succeeded; the ordinary package keeps nondiagnostic fixed GPU
plugin86c7d503…. Upload diagnostic plugin remains isolated in tmp/acv-readback-fixed-test.
[Accepted artifact/launcher manifest](../logs/acv-readback-origin-20261022/accepted-fix.json).
[Launch-boundary tests](../tools/native-rexglue/test_probe_readback_profile.py)
passed ACV full default, unchanged SDK profile, CLI/config overrides, BOM and
missing-config failure without launching a game (operation48ab947f-541d-4081-99ba-f9dbe367ffea).

### Targeted asynchronous readback design reference, 2026-10-22

User-requested VFS/allocation/performance follow-up is a design/reference step;
the proven full ACV profile remains active. VFS is not established as a necessary
trigger of the restored texture: unchanged resource headers/profile origin plus
full readback recovered its image. Existing system:/dlc_* failures remain separate.
Trace NtCreateFile_entry (xboxkrnl_io.cpp:103) → VirtualFileSystem::ResolvePath
(virtual_file_system.cpp:105) → NtReadFile_entry (:187) → actual read completion.
Record handle-open generation, effective64-bit offset, requested/actual bytes,
status/APC completion and SHA. Then correlate BHD5/BDT range, BND3 entry bounds,
DCX/zlib exact decoded size/end-of-stream and TPF payload hash with the buffer
actually enqueued for atlas rendering. Reuse read-only extracted manifests as
an oracle; asynchronous buffers must stay owned until the consumer uses them.

The two earlier physical requests0x40000/0x800000 with protect0x20000400 failed;
they are not evidence of duplicate successful allocation. Track successful
allocations/frees, backing identity, address-space tag, range, resource ownership,
allocation epoch and queued GPU references. Canonicalize physical aliases through
SDK Memory/PhysicalHeap::GetPhysicalAddress, not address masking: the E alias has
an extra0x1000 offset. EDRAM is a separate address space, not another CPU RAM alias.
CPU staging, RGBA source and compressed/tiled output are legitimate distinct
representations; delayed work must not publish into a recycled allocation.
Repeated workload after completed retirements distinguishes retained pools from
unbounded unowned allocations; overlapping ranges alone do not prove a leak.

[async_readback_reference.hpp](../tools/native-rexglue/async_readback_reference.hpp)
provides an owned-slot fence submission/poll model, with logical resource,
allocation/GPU/CPU-write generations. It includes VkBuffer-copy of the existing
Xenos guest-format Resolve output and a separate ordinary single-sample RGBA8
VkImage dirty-rectangle example using vkCmdCopyImageToBuffer. Neither path is
wired into SDK execution or measured on the GPU. Caller owns allocations,
command buffers, queue synchronization, source state tracking and publication
locks; whole dedicated staging memory is mapped. Busy/poisoned slots cannot be
reused. A stale allocation/version is discarded after fence completion. Coherent
memory still requires completion; noncoherent memory is invalidated after it.

Integration should schedule the snapshot immediately after the matching guest
bake/Resolve has entered the GPU command stream, download only dirty CPU-consumed
ranges and publish their exact generation. Ring slots are not per-frame
substitutes for readiness. An immediate guest CPU dependency must still wait for
its own ticket; skipping the wait or returning an older frame is not equivalent.
First use the existing graphics queue; separate transfer queues additionally
require semaphore dependencies and ownership transitions. For FSI keep the
existing resolve/tiling/endian conversion; a native Vulkan image copy alone does
not reconstruct guest EDRAM bytes. Partial rectangles need guest-pitch row scatter
or tile-aware range copies, and cannot mark untouched portions synchronized.

[Host mock tests](../tools/native-rexglue/test_async_readback_reference.cpp) passed
ASan/UBSan: unsignaled fence prevents publication; busy slot prevents reset/reuse;
allocation reuse, newer GPU generation and CPU writes reject stale data; failures
poison slots; ordinary-image ROI and resolved-buffer paths record host barriers.
Operation eefd29b7-88f4-4a5a-9b91-ab2199038ab8. These tests do not establish game
correctness, lower stalls or new runtime behavior. Official Vulkan references:
https://docs.vulkan.org/guide/latest/synchronization_examples.html
and https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdCopyImageToBuffer.html.

### Integrated Resolve completion and resource tracing, 2026-10-23

SDK `src/graphics/vulkan/command_processor.cpp:4403` adds
`AwaitReadbackSubmission`: verify the actual ticket completed even if a failed
submission/wait did not report device loss. Resolve slots now carry
submission_written/written_size, wait before reuse/reallocation, and check
completion before host publication. Fast/some may use a previous slot only when
its copy is complete; these modes still have explicitly delayed semantics.
Full remains the ACV default and publishes the current Resolve. Invalidate
VkResult is checked; failure returns without copying to guest memory. Resolve
staging growth rounds to64KiB instead of16MiB; MEMEXPORT policy is unchanged.
A submission closed by buffer reallocation is reopened before recording copies.
[Portable patch](../tools/native-rexglue/patches/resolve-readback-completion.patch).
This is correctness hardening and allocation granularity reduction, not a
validated nonblocking replacement for full or an FPS improvement claim.

[Actual-source host test](../tools/native-rexglue/test_resolve_readback.py)
passed ASan/UBSan: pending/short previous slot, wait/invalidate/device failures,
full-current and explicit delayed-mode policy (operation263eea70-a05b-4c6f-810b-84d5c8bd538c).
Shared-memory barrier regression and launcher default/override checks passed.
Private GPU build exit0, installed SDK binaries unchanged.

`ACV_TRACE_RESOURCE_IO=/absolute/unique/file` enables bounded JSONL import
tracing in handwritten acv_compat_shims.cpp: open number survives handle reuse,
requested/observed offsets, IOSB actual bytes/status, sampled FNV1a64/nonzero
counts, SDK physical allocation extents/epochs/overlaps and free requests.
The later open tracer records the resolved XFile device/entry. Maximum16384
records and8MiB sampled per read. Implicit offset is a pre-call XFile position
snapshot, not atomic with Read; byte verification is required under concurrency.
Free exports return void: a free_request is not proof of successful release,
GPU retirement, complete guest allocator coverage or absence of leaks.

Two120s runs20261022T174836433699Z/20261022T175236415490Z, each child0/8RDC,
retain45 existing VFS/allocation diagnostics. Both reproduce the accepted2MiB
RGBA SHA52fdd6d7… and BC3 compressor SHAe481cf47….
Diagnostic frame53712/event69 has933 draws; declared slot0,4158indices,
1024x512BC3. Its512KiB GPU data exactly equal two contiguous same-run CPU upload
chunks and accepted SHA00c279ff….
[Same-run proof](../logs/acv-integrated-readback-20261022/gpu-byte-proof.json).
Normal GPU frame37095/event69 has916 draws and identical material bytes.
[Normal-plugin proof](../logs/acv-integrated-readback-20261022/release-gpu-proof.json).
This verifies this material, not global scene correctness or stable FPS.

[Bounded VFS verifier](../tools/native-rexglue/verify_resource_io.py) checked177
reads/33554426bytes against original game files with no checksum/count/offset
anomalies.16 source DCX entries containing verified runtime reads strictly decode
to the existing decoded SHA manifest. Guest decompressor output and render-queue
ownership are not captured by these source-slice checks.
[VFS results](../logs/acv-integrated-readback-20261022/vfs-payload-check.json),
[DCX results](../logs/acv-integrated-readback-20261022/consumed-dcx-check.json).
Two failed requests0x40000/0x800000 still return0; no live-overlap candidates in
the observed import records. This is not evidence of double allocation/leak.

Current package adds resolved-device/entry trace metadata: ELF SHA dde92b53…,
normal GPU55937953…, runtime d8fb56af…; latest build manifest in the build dir.
Its120s run20261022T182231377426Z also exits child0 with45 existing diagnostics;
this later run does not have the same-run upload/GPU proof above.
Canonical config SHA31ef1d2a… remains unchanged. Build with explicit plugin
`--gpu-plugin-artifact tmp/acv-gpu-integrated/librexgpu-xenos.so`; the prior package
is preserved in logs/acv-integrated-readback-20261022/previous-package.
The generic async reference remains separate from production execution.

### Resolve-to-first-CPU-read chain established, 2026-10-23

[Exact2MiB proof](../logs/acv-resolve-consumer-20261023/complete-proof.json)
identifies the upstream bake range: physical0x0FCA7000, first2MiB publication
submission1298/frame627. Its full bytes equal the same-run source copied at
retail0x82C83918 (LR0x82C8391C), SHA6af9819e…. This is earlier than the previously
tracked RGBA0xFE191000/physical0x1E192000.

The actual sequence is Resolve0x0FCA7000 → private converter input in virtual
system RAM → tiled-to-linear conversion → copy-out to physical0x1E192000 →
compressor staging → BC3 → final texture converter/upload. In the proof,
publication precedes the first CPU source copy by28038us. This is a diagnostic
clock interval, not an FPS/latency benchmark. Later converter and compressor
stages are connected by pointers plus4KiB samples; only the first2MiB Resolve/
CPU copy has a new full-byte comparison here. Existing accepted compressor and
GPU material proofs remain separate.

Read-only hooks: sub_82BA3900 at verified compressor lock callers; sub_82C835E0
and sub_82C83DE8 at0x82948B48/0x82948B60; sub_82C83100 at source/destination copy
boundaries. `ACV_TRACE_BAKE_CHAIN=/absolute/unique/prefix` enables bounded CPU
JSONL; optional private GPU `ACV_CAPTURE_RESOLVES=ON` records matching publications.
ACV_BAKE_TRACE_PHYSICAL_START/LENGTH restrict only diagnostics. Unique2MiB
snapshots are created once; normal execution and original PPC implementations
remain responsible for the work. `bake_chain_trace.h` is included in revision inputs.

The converter may detach source_resource(+8) after copying; a null field is
expected in copy-source mode. The retained table, destination dimensions and
retail0x82C836D8 instructions establish this, not a guessed resource identity.
A physical write watch at0x1E192000 captures128 pages written with LR0x82C83164:
copy-out of converted bytes, not direct GPU Resolve writes.
[Raw instructions](../logs/acv-resolve-consumer-20261023/82ba3900.rpc.asm),
[caller](../logs/acv-resolve-consumer-20261023/82948a00.rpc.asm).
The generic analyzer's initial zero overlap at0x1E192000 was resolved by tracing
this earlier converter; it was not a Vulkan memory-mapping defect.

`src/graphics/command_processor.cpp:1197` EVENT_WRITE_SHD currently stores the
guest completion value immediately. Any deferred readback must publish pending
bytes before a guest-visible fence/writeback can let the CPU proceed. An async
copy alone is insufficient. A production deferred mode and its memory-allocation/
CPU-write-generation guards are still open; full remains the working default.

### Private deferred Resolve experiment, 2026-10-23

Two70s tests completed child0 with45/27 existing VFS/protection errors.
Private GPU bb29a5c4… and host fbf7f9ce… use the measured CPU copy boundary
LR0x82C8391C to request publication on the GPU worker.51 jobs published;47 CPU
reads match latest publication by size and4KiB sample. First2MiB exactly equal
the same-run Resolve and full oracle SHA6af9819e….44 flushes came from CPU
requests,2 from slot reuse,3 from eviction. First2MiB request waited2429us;
this diagnostic measurement does not establish FPS or lower stalls.
[Proof](../logs/acv-resolve-consumer-20261023/deferred-test/second-final-proof.json),
[runs](../logs/acv-resolve-consumer-20261023/deferred-test/runtime-summary.json).

Actual-header ASan/UBSan tests reject CPU changes/failed completion, verify
wait outside memory lock and overlapping slot retirement. Unique active tracing
is mandatory for this experiment; failure/exhaustion prevents unchecked CPU reads.
Test: tools/native-rexglue/test_deferred_resolve.py, operation5ff94476-c71d-4e20-996b-6c0eb5e40ded.
The fixed leased range is a reused bake pool, not a generic resource identity.
Allocation ABA, other CPU readers, shutdown concurrency and performance remain
unproven. Normal GPU55937953… and default full are retained.

User priority is now remaining black gameplay models. This deferred prototype
is not a fix for all scene shading; continue at a concrete full-readback draw.

### Texture rendering fix confirmed by user, 2026-10-24

Пользователь подтвердил, что предыдущие правки помогли именно с текстурами
в самой игре. Принятый вклад — исправление result exponent bias в Vulkan
SPIR-V translator, а не эксперимент deferred readback.

`src/graphics/pipeline/shader/spirv_translator_fetch.cpp:2032` извлекал
`exp_adjust` из dword4, содержащего LOD bias. По `xenos.h:1236–1254`
и `dxbc_translator_fetch.cpp:1957–1958` правильное поле — signed bits13:18
из dword3. Теперь оно загружается отдельно; dword4 остаётся источником LOD.
[Переносимый patch](../tools/native-rexglue/patches/texture-fetch-exponent-word.patch).

На сохранённом frame37095/event69/pixel560,480 texture sample RGB≈0.21
умножался на2^-16: после выборки≈3.2e-6, при упаковке RGB округлялся до0.
Для этого descriptor dword3 задаёт exponent0, то есть множитель1.
[Причина и значения](../logs/acv-gameplay-black-20261023/root-cause.json).
Это replay с диагностически снятым FSI/RTE ограничением; он устанавливает
вычислительную причину, а визуальное улучшение подтверждено пользователем.

[Test](../tools/native-rexglue/test_texture_fetch_exponent.py):2048 случаев
знакового exponent/texture slot, реальный descriptor ACV, ASan/UBSan;
negative control со старым dword4 отвергается. GPU build прошёл, plugin
SHA90d8ef1adcad93dd043750aecdd24753eb52f17260f8398deb532dbb3a061f78.
Текущий host SHA8b10aa52…, runtime d8fb56af…. Полный manifest и source
checkpoint: [accepted-fix.json](../logs/acv-texture-fix-accepted-20261024/accepted-fix.json).
Сохранённый fixed plugin: `tmp/acv-gpu-texture-fix/librexgpu-xenos.so`.
Для следующих host builds использовать
`--gpu-plugin-artifact tmp/acv-gpu-texture-fix/librexgpu-xenos.so`,
чтобы не вернуть прежний plugin55937953… или installed SDK artifact.

Full readback продолжает восстанавливать CPU-generated atlas; это отдельное
ранее проверенное исправление. Текущий пользовательский config содержит
mm_wc_rw=true; отдельный вклад этого флага в texture fix не измерялся.
Подтверждено улучшение текстур, полная визуальная parity всех сцен не заявлена.

### Same-input texture colour regression, 2026-10-24

Frame37095/event69/pixel560,480 replayed with identical nine interpolated
inputs and identical texture sample RGB≈0.21. Correcting only the descriptor
word used by exponent extraction changes scale2^-16→1 and final RGB
[0.0000031726,0.00000315055,0.00000288745]→
[0.207919,0.206474,0.189232]. Ratios65535.84–65536.03 reflect printed precision;
packed RGB changes from0 to nonzero.
[Proof](../logs/acv-gameplay-black-20261024/same-input-color-proof.json),
operation ed190804-dd8b-43f7-bd2a-9cabee4198e9.
The diagnostic replacement corrects two fetches and preserves original dword4
LOD accesses; both compared replays remove unsupported interlock/RTE for the
debugger. This is a matched arithmetic proof, not an interlock-output oracle.

The existing105s test20261023T192728538829Z used the accepted fixed GPU and
current user config98d1c2ed…, ended child0/abort_reason=null. Only error-level
message was missing `system:\system.properties`; dlc_sound was mounted.
[Run](../logs/acv-native-linux/20261023T192728538829Z/launch.json).
Eight RDCs exist, but frame42120 has no4158-index target draw; these captures
do not establish a regression across all gameplay models. The user's visual
confirmation remains the accepted in-game evidence.

### Build retains selected GPU artifact, 2026-10-24

`build_linux.py` now retains the preceding manifest GPU override when no
explicit artifact is supplied. A missing/out-of-workspace/non-ELF or changed
recorded artifact fails before CMake; a mutation during the host build fails
before private staging. Explicit artifact selection records a new expected hash.
Fresh workspaces without an override still use SDK defaults.

[Test](../tools/native-rexglue/test_build_gpu_retention.py) executes the actual
wrapper with a fake CMake that stages the old SDK plugin, and checks all five
selection/failure scenarios (operation2684a64e-e31d-426c-83a1-bf5dc76ff797).
Real `build_linux.py --jobs4` preserved host8b10aa52… / GPU90d8ef1a…
(operation0cfa9293-a4ba-48bb-aa97-cc84665a8f66).
[Retained manifest](../logs/acv-gameplay-black-20261024/retained-build.json).
Explicit `--gpu-plugin-artifact tmp/acv-gpu-texture-fix/librexgpu-xenos.so`
remains the recovery path if another GPU build changes a recorded mutable file.
This changes build staging only; no new gameplay/visual claim.

The extra material probe event79 did not retain identical interpolated inputs
after replay replacement, even with DebugPixelInputs.primitive=0. Treat its
before/after values as unmatched; event74 had no supported pixel trace. The
matched event69 proof is unaffected. Full multiple-material in-game regression
remains open.

### Missing system.properties is handled, 2026-10-24

The failed `system:/system.properties` request is a startup properties override,
not a texture payload. Read-only retail inspection finds UTF16BE path0x8210FA38,
its pointer0x83573270, and switches `--DLsysprop`/`--DLsysprop_nooverwrite`.
Constructor0x82C23700 selects the default path and allows command-line replacement.
Loader0x82C22B90 calls file-status check0x82C33458 at0x82C22BF4; a false result
branches0x82C22C00→0x82C22C8C, skips parser0x82C25810, destroys the local file
wrapper and returns. Caller0x82C23DF4 does not test a return value and continues
initialization. No abort or fabricated replacement is required on that branch.
[Classification and exact addresses](../logs/acv-gameplay-black-20261024/system-properties-classification.json),
[raw loader](../logs/acv-gameplay-black-20261024/system-properties-read.rpc.asm),
[caller continuation](../logs/acv-gameplay-black-20261024/system-properties-tail.rpc.asm).

This is static absence-path evidence plus the recorded failed NtCreateFile and
normal runtime completion; no new guest hook correlated the loader at runtime.
It does not reconstruct every default property or all override behavior. Six
plausible dvdbnd path hashes had no matches; that bounded search is not proof
that no differently named archive entry exists. SDK VFS diagnostics remain
visible; no dummy file, false-success import shim or mount was added.

### Bounded profile UTF-8 decoding, 2026-10-24

The handwritten profile shim's previous BMP-only decoder accepted overlong
sequences/surrogate scalars and replaced valid supplementary characters byte
by byte. `profile_text.h` now validates scalar ranges and continuation bytes,
keeps the established '?' policy for malformed input, and emits complete
UTF-16 surrogate pairs without splitting at the guest-name capacity. The
header participates in host revision hashing; original XAM completion still
runs. ARTEM and canonical config are unchanged.

[Test](../tools/native-rexglue/test_profile_text.py) checks1112063 valid non-NUL
Unicode scalars, Cyrillic, malformed/truncated UTF-8, pair capacity and the
actual StoreGuestWide UTF16BE/NUL/canary bounds under ASan/UBSan
(operation cb7209b6-bcd6-47ac-a218-385746ea3f9f).
New host f61ee442… built with retained fixed GPU90d8ef1a….55s smoke reached
XamShowKeyboardUI pilot-name ARTEM and child0;13 known system/DLC-image errors.
[Proof](../logs/acv-gameplay-black-20261024/profile-text-smoke-proof.json).
Prior working package is preserved in `tmp/acv-profile-text-previous-package`.

This fixes config/profile conversion only. The installed SDK's non-headless
KeyboardInputDialog still needs its own byte-length/truncation, cancel and
conversion fix/verification; tutorial font glyph rendering is also separate.
No claim that a GUI dialog or all in-game text is now correct.

The build-retention guard additionally verifies copied GPU bytes before atomic
replacement and records the selected/staged hash rather than rereading the
mutable origin. Regression416f6bcc-1f66-4ced-a91b-887f8c6b95c0 includes a
source mutation after copy: manifest keeps the actual staged identity and the
next implicit selection refuses the changed origin.

### Fixed-build material capture follow-up, 2026-10-24

The 105s launch produced a multi-draw capture at
`logs/acv-renderdoc/20261023T195121046588Z/acv_frame18547.rdc`. Draw309 has
3129 indices and shader-declared fetch slot0, BC1 512x512, physical0x1C3E2000.
The fixed fragment SPIR-V reads `fetch_constants[0].w` (dword3) for signed
`exp_adjust`; the captured dword is0x06A80D10, exponent0, multiplier1.
The raw BC1 mip0 has 131072 bytes, SHA-256
`124e73fa788a2d3c76c0ac1a981a039836049b280a3a22c92c4e105afb7df4f3`; decoding
shows941 RGB values and122691 nonzero-luma pixels. This rules out an all-zero
source for this draw, but the texture's object/material identity is unknown and
black texels are present. RenderDoc pixel-debug probes did not produce a trace
for this EDRAM/interlock draw, so they do not prove visible model output.

Neighbor captures contain 158 draws/273 declared fetches in frame18547; the
other captured frames have few draws. The launch log shows Setup/emblem-related
VFS requests and does not establish a mission. Exact artifacts are indexed in
`logs/acv-gameplay-black-20261024/fixed-capture-neighbor-coverage.json`,
`fixed18547-all-fetches.json`, `current-draw309.json`, and
`texture309-512x512.raw` (raw bytes are not an image preview). A reproducible
mission plus mapping these draws to mech/environment materials is still the
first open graphics acceptance item.


### Bounded keyboard dialog conversion, 2026-10-24

The SDK `KeyboardInputDialog` now sizes its ImGui UTF-8 buffer for the maximum
UTF-16 unit count (up to three UTF-8 bytes per unit plus NUL), sanitizes and
truncates the initial text through the shared bounded decoder, reads the actual
NUL-terminated input, and decodes the result to at most `buffer_length - 1`
UTF-16 units before guest storage. Zero and overflowing guest lengths are
rejected. Portable source delta is
[`keyboard-dialog-utf8-boundaries.patch`](../tools/native-rexglue/patches/keyboard-dialog-utf8-boundaries.patch);
it applies to SDK base `c94f5ebdcb3c9d1a460ca48e04f9758448f8d518` and depends on
[`profile_text.h`](../tools/native-rexglue/profile_text.h).

`test_profile_text.py` passed Unicode, malformed/truncated input, capacity and
canary tests under ASan/UBSan. SDK runtime was rebuilt privately (SHA
`40b0e13445ddafa9178b21be898af74d21f42bde9b8f324fc2c8a69fbbbdd36b`) and
staged with host `799f0054…`, fixed GPU `90d8ef1a…` and config `98d1c2ed…`.
The 105s non-headless smoke at
`logs/acv-native-linux/20261023T202212361942Z/` reached
`XamShowKeyboardUI` with pilot name ARTEM and exited 0 without abort/exception.
It did not establish that typed input or Cancel works; UI-driven Unicode/cancel
and tutorial glyph rendering remain open.

### Recovered dlc_image emblem assets from retail dvdbnd, 2026-10-24

The runtime's `dlc_image:` requests were not all truly absent: four requested TPF
containers are present in the main retail `dvdbnd.bdt`, although no separate DLC
payload exists in the ISO. `tools/build_dlc_image_dir.py` maps only the observed
requests to verified BHD5 path hashes, validates the BDT byte ranges, checks TPF
magic and internal texture names (strictly decoding DCX for validation), and
copies the original bytes into a minimal VFS tree. The DCX wrapper is preserved
for `piece00/01.tpf.dcx`. Its manifest is generated only under `tmp/`.

`acv_linux_probe_app.h` mounts this directory read-only at `dlc_image:` when
present. The bounded probe discovers `tmp/acv-dlc-image` and supplies the root;
without that generated asset directory no mount is made. The regression test
[`test_dlc_image_mount.py`](../tools/native-rexglue/test_dlc_image_mount.py)
checks all four paths against actual BHD5/BDT bytes, hashes, magic and file set.

The rebuilt host `2d112d64…` kept GPU `90d8ef1a…`, private runtime
`40b0e134…`, and config `98d1c2ed…`. In the 70s runtime smoke
`logs/acv-native-linux/20261023T203303169143Z/`, child returned 0 at the
configured time limit, `dlc_image:` mounted, and the only runtime error was the
previously classified optional missing `system:\system.properties`. No failed
`dlc_image:` opens remained, including both emblem and thumbnail requests.
This resolves those four observed paths only; unobserved DLC content is not
reconstructed. It is a VFS/Setup improvement and is not claimed as the cause or
fix of gameplay mech shading.

### Readback eviction lifetime and file-open audit, 2026-10-24

`VulkanCommandProcessor::EvictOldReadbackBuffers` previously unmapped and
destroyed both slots as soon as the map exceeded its cap or a resource aged out,
without checking `submission_written[]`. A staging buffer could therefore be
freed while its queued transfer still referenced it. The method now skips any
candidate with a ticket newer than `submission_completed_`, marks it
`eviction_pending`, and retries on a later frame after completion. It does not
wait in the eviction path. `ShutdownContext` remains ordered behind
`AwaitAllQueueOperationsCompletion()` before bulk destruction.

The regression compiles this exact SDK method with Vulkan lifetime mocks under
ASan/UBSan. It verifies no unmap/destroy/free while pending and reclamation after
the ticket completes, including map-cap pressure. Evidence is logger stage
`gpu_lifetime_test`; a real GPU run has not forced eviction under queue load.

[`audit_runtime_file_misses.py`](../tools/audit_runtime_file_misses.py) scans
explicit `NtCreateFile FAILED` records only, excluding VFS root-resolution
noise. Across117 runtime logs it found3241 historical failures/35 unique paths.
Most were repeated requests before the `dlc_image:` and `dlc_sound:` mounts
were added. Final post-rebuild log `20261023T212444919696Z` is post-mount: all
four known image files resolve, and no manifest-backed sound bank fails to open.
Only the unavailable sound names below and optional `system.properties` still fail.

The five unresolved sound names are `acv_bgm_title.fev`,
`acv_dlc_dlc_se.fev`, `ch09.fsb`, `dlc_se.fsb`, and literal
`acv_talk_$(voicelang).fev`. The first four have no matching lowercase
`/sound/NAME` hash in the main BHD5 index; candidate directory variants were
also checked for the FSB names. Existing `acv_voice_ch09.fev` metadata has no
corresponding `ch09.fsb` entry. No payload was found in the current retail ISO,
so manufacturing dummy banks or guessing aliases would conceal missing content.
The extracted `0000_acv.ini` sets `VoiceLang=jp`; the sound builder now resolves
the literal token to `acv_talk_jp.fev` for lookup and will create a read-only
VFS alias only when that verified localized bank exists. It does not exist in
this disc image, so the fifth request remains unresolved. Complete audit:
[`runtime-file-open-audit.json`](../logs/acv-gameplay-black-20261024/runtime-file-open-audit.json).

The follow-up MEMEXPORT changes additionally validate guest physical ranges,
key staging buffers by exact ordered range topology, wait before reusing a slot,
propagate readback failures, check mapped-memory invalidation, and defer staging
destruction by submission ticket when completion waits fail. Regression tests
compile the actual SDK methods with ASan/UBSan. Release SDK and native package
were rebuilt; GPU artifact SHA-256 is
`78231562bdd4fb268d8ac94185e893d9eaa10111c01be7c8496e0ff6c6d055bc`.
Updated package startup smoke ran 75 seconds and exited child0. It did not enter
gameplay and emitted no Resolve/MEMEXPORT/readback log lines, so it does not
verify the readback path end to end. It repeated the four startup sound misses
and optional `system.properties`; `ch09.fsb` was not requested in this menu run.
All targeted readback/VFS/texture/build regressions passed. Real GPU eviction
under forced queue pressure remains unobserved.

User triage instruction (2026-10-24): skip these remaining signals in routine
triage and do not reopen them without new evidence or a new request: the five
missing DLC sound requests, optional `system.properties`, no gameplay readback
in the startup smoke, and no forced queue-pressure eviction. This is a priority
decision only; it does not claim the absent audio payloads or runtime scenarios
were supplied or verified.

### Manual gameplay capture, 2026-10-24

The 300-second RenderDoc run `logs/acv-native-linux/20261023T205031330068Z/`
exited with child status 0 at the configured time limit. Four captures were
written under `logs/acv-renderdoc/20261023T205031330068Z/`. The largest capture
(`acv_frame89178.rdc`, 149 MiB; around 170 seconds) replays to one fullscreen
4-vertex draw and one dispatch. Its 1280×720 pixel descriptor has non-black
numeric samples (center RGB about `[0.343, 0.298, 0.274]`); this does not identify
the gameplay scene or validate mech materials. The other captures have at most
29 non-indexed draws and no indexed elements. They therefore do not provide a
material draw to tie to the invalid fetch warning.

The runtime logged 3,853 invalid-fetch warnings. A persistent constant was
`8A000000 0F9FF097 0059E4FF 00A80D11 00000003 00000200`, with base address
`0x0F9FF000`, size 1280×720, and format value 23 (`24_8_FLOAT`) under the Xenos
bitfield decode. This is consistent with a depth/stencil-like surface, but its
consumer was not identified; other warnings referenced `0x1C1...` regions.
The allow-invalid flag remains off: earlier A/B evidence did not show that it
fixes dark gameplay. Vulkan device/driver for this run were NVIDIA GeForce GTX
1660 SUPER, driver 615.78.08, Vulkan 1.4.0. The only runtime error was the
already classified missing optional `system:\system.properties`; no Vulkan
device-loss or synchronization error appeared.

This run did not change the implementation. The user separately confirmed the
black mech textures are fixed; this capture only classifies an unrelated warning
and does not replace that visual confirmation. It did not contain a mech/material
draw, so it adds no further visual finding; the user has removed additional
mission/material regression from the active task list.

### SDK build options used as test tools, 2026-10-24

Option semantics come from `tools/rexglue-sdk/CMakeLists.txt:21-32,230-269` and were checked by
configuring and building private trees under `tmp/`. A build writes its libraries to
`tools/rexglue-sdk/out/linux-amd64/` regardless of the build directory: copy the two libraries
(`librexruntime.so`, `librexgpu-xenos.so`) to a private `tmp/` directory right after the build
and never let it replace the accepted artifacts. `tmp/acv-backups/<stamp>/sdk-out-release/`
holds the `out/.../Release` tree as it was before this work.

Base configure line (graphics/compiler values of the accepted SDK build). The four tool options
are NOT meant to stay off: they are switched on per test purpose with the presets below. The
accepted artifact build simply does not need them, and the earlier `OFF` values only reproduce
that build.

```bash
BASE="-G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_FLAGS=-march=x86-64-v2 \
  -DREXGLUE_USE_VULKAN=ON -DREXGLUE_USE_D3D12=OFF"
SDK=tools/rexglue-sdk
# UB hunt (sanitizers + tests; run ctest on this tree too):
cmake -S $SDK -B tmp/acv-sdk-ubsan-build $BASE -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DREXGLUE_ENABLE_SANITIZERS=ON -DREXGLUE_BUILD_TESTS=ON \
  -DREXGLUE_ENABLE_TRACY=OFF -DREXGLUE_ENABLE_PERF_COUNTERS=OFF
# Timing / stalls (perf counters + tests; add -DREXGLUE_ENABLE_TRACY=ON only after the submodule step below):
cmake -S $SDK -B tmp/acv-sdk-perf-build $BASE -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DREXGLUE_ENABLE_PERF_COUNTERS=ON -DREXGLUE_BUILD_TESTS=ON -DREXGLUE_ENABLE_TRACY=OFF
# Tests only (fast regression gate for SDK/recompiler edits, no extra instrumentation):
cmake -S $SDK -B tmp/acv-sdk-tests-build $BASE -DCMAKE_BUILD_TYPE=Release -DREXGLUE_BUILD_TESTS=ON \
  -DREXGLUE_ENABLE_SANITIZERS=OFF -DREXGLUE_ENABLE_TRACY=OFF -DREXGLUE_ENABLE_PERF_COUNTERS=OFF
ninja -C tmp/acv-sdk-tests-build && ctest --test-dir tmp/acv-sdk-tests-build --output-on-failure
# Accepted-artifact style (all tool options off): -DCMAKE_BUILD_TYPE=Release with the four options OFF.
```

| Option (default) | Use it for | Limits |
|---|---|---|
| `REXGLUE_ENABLE_SANITIZERS` (OFF) | **Use for UB investigations.** Undefined-behaviour checks (misaligned/overflowing/invalid shift access) while chasing memory-corruption or UB suspicions in SDK or recompiled code | UBSan only: `-fsanitize=undefined`. ASan is deliberately not offered because it conflicts with the custom mapping at `0x100000000`. Rebuild everything that links the SDK, expect slower runs, and keep the result out of the accepted artifact set. |
| `REXGLUE_BUILD_TESTS` (OFF) | **Use before and after SDK/recompiler changes.** PPC instruction tests and SDK unit tests (`tests/ppc`, `tests/unit`, Catch2 from `thirdparty/catch2`); run with `ctest --test-dir <build>` | Adds `enable_testing()`. Configure takes longer; not needed for the game build. |
| `REXGLUE_ENABLE_TRACY` (ON in the SDK) | **Use for stall profiling once the submodule is present.** Tracy frame/zone profiling for GPU/command-processor stalls | Defines `REXGLUE_ENABLE_PROFILING` only for non-Release configurations: use `RelWithDebInfo`, not `Release`. `thirdparty/tracy` is an uninitialised submodule here, so configure fails with "Git submodule 'tracy' is not initialized" until `git submodule update --init thirdparty/tracy` is run. That fetches external code and needs an explicit decision; the accepted configuration keeps it OFF. |
| `REXGLUE_ENABLE_PERF_COUNTERS` (ON in the SDK) | **Use for frame-time checks.** Lightweight counter registry/overlay (frame time, FPS counters in `include/rex/perf`) for checking a frame-limiter or readback change | Also compiled out in `Release` (non-Release only). Use it in a private `RelWithDebInfo` tree and compare against a `Release` tree before judging timing. |
| `REXGLUE_ENABLE_FIDELITYFX` (OFF) | Enables `REX_HAS_FIDELITYFX_SDK` in `rexui`: presenter effects `cas`, `fsr`, `fsr2`, `fsr3` (`present_effect`, `src/ui/presenter.cpp`) | See the next paragraph. |

Findings from the FidelityFX attempt (private tree `tmp/acv-sdk-ffx-build`):

- Configure succeeds on this host (Vulkan 1.4.363, `glslc`/`glslangValidator` present). It
  fetches `github.com/rexglue/FidelityFX-SDK` at the commit pinned in
  `cmake/rexglue_fidelityfx.cmake`; this is network input and stays inside the build tree.
- On Linux the FSR2 shader permutations are generated by `FidelityFX_SC.exe` through
  `/usr/bin/wine`. Every generation step fails with `Macro redefined ...
  FFX_FSR2_OPTION_REPROJECT_USE_LANCZOS_TYPE`. Cause: the generated command lines contain
  `-DNAME={0,1}`, and ninja runs them through `/bin/sh`, which is `bash` here, so bash brace
  expansion turns the argument into `-DNAME=0 -DNAME=1`. 72 occurrences exist in `build.ninja`.
  The build never reaches the SDK sources; the libraries from the failed build were not used.
- Fix applied: after configure, `build.ninja` is rewritten so those arguments are single-quoted
  (`'-DNAME={0,1}'`). The build then completes (97 steps, 0 failures) and produces
  `librexruntime.so` (contains `fsr3` and `present_cas_additional_sharpness`, so `REX_HAS_FIDELITYFX_SDK`
  is compiled in), `librexgpu-xenos.so` and `libamd_fidelityfx_vk.so`. Reproduce with
  `python3 tools/native-rexglue/build_sdk_ffx.py`; output goes to `tmp/acv-runtime-ffx/`.
- `librexruntime.so` has an absolute RUNPATH into `tools/rexglue-sdk/out/linux-amd64`; `patchelf` is not
  installed, so a relocatable package would need `$ORIGIN` set first.
- Not verified: `present_effect=cas|fsr` on screen. The launch with these cvars was not run, and cvar
  value validation happens after start-up (an invalid value was not rejected by `--help`-time parsing).
  `acv_settings.py` therefore marks `cas`/`fsr` experimental and passes them only if
  `probe_linux.py` finds `cas_additional_sharpness` in the staged runtime. `fsr2`/`fsr3` stay unavailable.

Tests run, 2026-10-24 (`tmp/acv-sdk-tests-build`, Release, `REXGLUE_BUILD_TESTS=ON`, other tool options OFF,
SDK tree = v0.10.0 + the working diff + the `frame_limit_fps` and `ACV_XUID` additions):

- `thirdparty/catch2` and `thirdparty/tracy` were uninitialised; both were fetched with
  `git submodule update --init --depth 1 thirdparty/catch2 thirdparty/tracy` at the commits pinned in
  the SDK (`88abf9b`, `05cceee`). Without them configure stops with "Git submodule ... is not initialized".
- `ctest -j8`: 1671 tests, 1666 pass, 4 skipped (BitStream write cases), 5 fail:
  `unit.calendar` decomposition/recomposition/round-trip (4, `tests/unit/core/chrono_test.cpp:148-154`;
  NT epoch decomposes to year 2185 instead of 1601) and `unit.Depfile escapes characters make treats as syntax`.
  These areas (chrono, depfile) are not touched by our diff; the baseline v0.10.0 failure was not rerun,
  so "pre-existing" is not yet proven. Treat these five as the known set when comparing later runs.
- The build also relinked `out/linux-amd64/librexruntime.so` and `librexgpu-xenos.so`; the accepted
  artifacts live in `tmp/acv-gpu-lifetime/` and the copies from before in `tmp/acv-backups/`.

### Start-up profile, 2026-10-24

Tool: `tools/native-rexglue/profile_startup.py` (gdb sampler; `perf` and `strace` are not installed and
`ptrace_scope=1` forbids attaching to unrelated processes, so gdb starts the game itself). It handles the
runtime's SIGSEGV/realtime signals with `nostop pass`, interrupts the process every second and stores the
top frames of every thread in `logs/acv-startup-profile/<stamp>/samples.json`. Guest functions appear as
`sub_82XXXXXX`. A second ad-hoc run read the blocked wait calls at 12/40/70 s; wait arguments were not
readable (no block info in the release binary).

Observations (146 samples over 150 s, accepted build, no profile, default cvars):

- There is no CPU-bound loading phase. 52 of 56 threads are parked in futex/`nanosleep` waits in every
  sample, including `Main XThread` (144/146 in `NtWaitForSingleObjectEx` via guest `sub_82B9A318` <-
  `sub_82C3B6A8`), `FileMan`, `Core.Res.*`, `SmallJobRunner`, the GPU command and pipeline threads.
  `BootDataLoad` is active in 6 samples only (seconds 3-9) and then waits.
- One thread burns a core: `DrawThread` (guest worker). 133 of 146 samples are inside the spin loop
  `sub_82AF5708` at `loc_82AF5738`, which calls the trivial leaf `sub_82BB58A8` (queue free-space query:
  stores `[r3+16968]`, `[r3+16968]-[r3+16976]`) and repeats while the result stays above a threshold.
  This is the title's own idle polling for work, not the cause of the wait; it explains the ~150-200 %
  CPU of an idle game. A yield/sleep inside that loop would cut CPU but needs a guest hook; not done.
- The same state (all waiting, `DrawThread` polling, no new log lines) holds at 12, 40 and 70 s, with and
  without launcher settings, with and without a profile, and with the older `acv-gpu-sync-test` build
  (Oct 22). So the long "loading" seen in the launcher is not caused by the 2026-10-24 changes.
- The only run in the log archive with sustained rendering after start-up is
  `20261023T205031330068Z` (300 s, manual input): the log is quiet until ~38 s (first `acv_dlc_dlc_se.fev`
  open), rendering warnings begin at 01:51:19 and reach ~50 lines/s at 01:52:44. That fits a title that waits
  for user input (Start = Return in the keyboard profile) rather than a slow loader, but it is not proven:
  what the window shows during the quiet period was not observed.

### Black window and SIGBUS: leaked guest-memory files in /dev/shm, 2026-10-24

Symptom: after a series of manual launches the game window stayed black and, later, every start died with
`child_returncode -7` (SIGBUS) right after `Guest memory arena mapped`. `coredumpctl info` showed the abort
(`raise`) inside `rex::memory::Memory::Initialize()` (`librexruntime.so`).

Cause (measured): `/dev/shm` held 58 `xenia_memory_<ticks>` files, 4.8 GB sparse each, 19 GB of the 24 GB
tmpfs in use, 4.8 GB free. The SDK creates the arena with `shm_open` and unlinks the name only in
`~Memory` (`src/system/xmemory.cpp:123-127`, `src/core/memory_posix.cpp:491-495`); the title ends with
`Title terminated; hard-exiting process.`, and killed or crashed runs never reach the destructor, so every
run leaked its touched pages. With too little free tmpfs the arena cannot be backed and the process aborts.
Two other findings of the same session are independent of the leak:

- The duplicated `--user_language` was a real bug (CLI11 parse error dropped every option including
  `--gpu_plugin`; fixed by `acv_settings.dedupe_options`).
- Under SDL's Wayland backend the game window is never shown and does not react to the close button, so the
  process stays alive (the launcher then keeps "game running" and Play disabled). Same build, profile and
  settings with `--video-driver x11` (XWayland): `VulkanPresenter: Created 2560x1052 swapchain` and a normal
  window that closes cleanly (`logs/acv-native-linux/20261023T234803426685Z`; user confirmed it started).
  Wayland runs always stopped after `Skipping Vulkan frame presentation due to async placeholder draw usage`.
  The probe now selects X11 when `DISPLAY` is set and `SDL_VIDEODRIVER` is not; the launcher has a
  "Видеодрайвер" selector (X11 default). Why Wayland never maps the window was not investigated.

Fixes:

- SDK patch `tools/native-rexglue/patches/launcher-runtime-limiter-xuid-shm.patch` (also holds the
  `frame_limit_fps` pacing and `ACV_XUID` additions): `shm_unlink` immediately after the object is sized.
  Verified with the rebuilt FidelityFX runtime: the number of `xenia_memory_*` files is unchanged before, during
  and after a `kill -9` of the game (13 -> 13 -> 13).
- Runtimes without the patch (the accepted artifacts) still leak. `acv_shm.py` lists/removes only files owned
  by the current user that no live process maps (`/proc/*/maps`); the launcher shows free `/dev/shm`, the
  number of orphans and a confirmed "Очистить" button, and refuses to start with less than 6 GB free.
  Nothing is deleted automatically.
- The tmpfs usage dropped from 19 GB to 5.9 GB during this session without action from this tooling (13
  orphans, 6.2 GB, remained); removal is therefore left to the explicit button/`acv_profile_cli.py shm-clean`.

Start-up profile (previous section): the "all threads waiting" picture was measured while the arena was already
short of tmpfs, so treat the absence of progress in it as unreliable until re-measured on a clean `/dev/shm`.
