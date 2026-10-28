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

### Ultrawide output and exit code, 2026-10-24 (`logs/acv-ultrawide-2560x1080-20261024/`)

- Output: `acv-display output source_width=1280 source_height=720 x=0 y=0 width=2560 height=1080`, swapchain
  2560×1080, FIFO. An earlier run had `x=320 width=1920` because `--letterbox=false` is not a cvar; the
  correct name is `--present_letterbox=false`. Full-rectangle output is accepted as confirmed.
- Projection experiment: `acv-aspect-experiment Enabled aspect=2.3703703703703702` (64:27). Projection only;
  HUD, reticle, culling and output geometry are NOT verified and need a human visual check.
- Performance: the heavy-scene cadence of ~15.5 FPS against a 60 FPS limit is recorded only in `result.json`
  (`swap_cadence_*`). Its source is not in `runtime.log` (no XE_SWAP lines there), so the number must be
  re-measured before it is used.
- Exit code: the probe returned 1 after normal closes because of the harmless
  `ResolvePath(system:\) failed` error, and the launcher reported "процесс завершён, код 1". The pattern now lives
  in `acv_settings.BENIGN_RUNTIME_ERRORS`; the probe counts only fatal errors (`fatal_error_count`). Verified with a
  15 s launch: child 0, fatal 0, one harmless error logged.

### Freeze on mission 2 load, session 20261024T121958909808Z

- The game was started through the launcher (X11, profile ARTEM, build `acv-ffx-test`, settings with FSR).
- Log timeline: start 17:19:59; swapchain 2560×1080 at 17:20:00; `dlc_sound` misses at 17:20:00–01 (known) and
  at 17:20:53 (`ch09.fsb`, `dlc_se.fsb`, `acv_dlc_dlc_se.fev`); one `Skipping Vulkan frame presentation` at
  17:21:14; a burst of pipeline creation at 17:24 (716 lines in that minute); **no line after 17:24:40**;
  window closed by the user at 17:26:16 (`Window closing`, exit 0).
- So about 90 s with no log output while the game was supposed to load mission 2. No pacing line was ever
  written (it needs 300 presented frames), so no FPS was measured in this session.
- Possible cause, NOT proven: `ch09.fsb` is a sound payload the retail image does not contain (see the DLC
  section above; the project refuses to fabricate banks). Whether mission 2 is the request that wait on it was
  not established: the request happened before the pipeline burst, not at the freeze. The next freeze needs a
  stack dump of the running game. `gdb -p` on a launcher-started game fails under `ptrace_scope=1` (not a
  descendant of the debugger); `profile_startup.py` runs the game as gdb's child and can be used for a repro.
- Launcher changes made from this: a hang indicator (no log line for 60 s is shown as a possible freeze), and the
  session summary reports "вывод не зафиксирован" instead of `0×0` when no output record exists.

### Mission-2 freeze: stacks captured, cause found in the host main thread, 2026-10-24

- Capture: `tools/native-rexglue/hang_watch.py` runs the game under gdb (game = gdb child, so ptrace is allowed) and
  writes `thread apply all bt 40` when runtime.log is silent for 90 s. Session `logs/acv-hang-watch/20261024T173613/`
  (`bt.txt`, `hang.json`: reason silence, log 553996 bytes, 17:40:40). The game was detached, not killed, and was
  stopped by hand afterwards.
- Finding: the host process main thread (LWP 27347, "acv_linux_probe") is blocked in `__lll_lock_wait_private` inside
  `pthread_create` **called from `librenderdoc.so`**. The window cannot process events while the main thread is
  blocked, which is what "зависла" looked like. The guest threads were waiting on NtWaitForSingleObjectEx
  (`Main XThread`, `ResourceBuild`, `FileMan`, `Core.Res.*`), so the guest was waiting on a host worker.
- Why RenderDoc is in every run: `RenderDocAPI::CreateIfConnected()` calls `Load("librenderdoc.so")`, which succeeds
  on this host, then `RENDERDOC_GetAPI`, which installs RenderDoc's in-process hooks. The implicit Vulkan layer
  is NOT the cause (it is only enabled by `ENABLE_VULKAN_RENDERDOC_CAPTURE`, which the game did not have).
- Change: `tools/native-rexglue/patches/renderdoc-opt-in.patch` makes CreateIfConnected() opt-in via
  `ENABLE_VULKAN_RENDERDOC_CAPTURE` (the variable `probe_linux.py --renderdoc` already sets). Rebuilt with
  `build_sdk_ffx.py`; staged into `tmp/acv-ffx-test/`. Check: without the variable, `librenderdoc` is not in the
  process maps and a Vulkan device is created (25 s run).
- Not yet verified: that this removes the mission-2 freeze. This is the hypothesis to test by playing mission 2.
  The earlier freeze was not caused by the reported "invalid" texture-fetch warnings alone; those repeated in the
  same session but the stacks show the host thread, not a shader loop.

### Mission-2 freeze, second capture (renderdoc opt-in build), 2026-10-24

- Capture `logs/acv-hang-watch/20261024T174322/bt.txt` (silence 90 s, 17:51:03). `librenderdoc` is no longer in the
  process: the earlier pthread_create block is gone.
- Host main thread (LWP 35700, process main): `main` -> `SDLWindowedAppContext::RunMainMessageLoop` ->
  `ProcessEvent` -> `Window::OnPaint` -> `Presenter::PaintFromUIThread` -> `VulkanPresenter::PaintAndPresentImpl`
  -> NVIDIA driver -> `drmSyncobjTimelineWait`. The presenter calls `vkAcquireNextImageKHR(..., UINT64_MAX, ...)`
  (src/ui/vulkan/vulkan_presenter.cpp:1605), so it waits without a timeout for a swapchain image that the
  presentation engine never returns.
- Guest threads wait on host work (`Main XThread`, `ResourceBuild`, `FileMan`, `DrawThread`); the pipeline
  creation threads are idle; GPU utilisation was ~4% at the time (nvidia-smi), so no GPU work was running.
- Common factor of the three freeze sessions (20261024T121958, T122900, T124322): `--present_effect=fsr`, i.e. the
  FidelityFX guest-output pass on the UI presenter. The clean accepted-build run (T234857, 99 s, no FSR) did not
  reach mission 2, so it does not clear this.
- Next test (not run): the same build with output bilinear (no FSR) and `hang_watch.py`, playing to mission 2.

### Session 20261024T124322094949Z: mission 2 reached, freeze at 17:49:30 (pacing available)

- Pacing (`[acv-display] pacing`, 300-frame windows, limit 60, VSync on, output FSR): 17:44:24 35 FPS;
  17:44:51–17:47:20 about 17–19 FPS; 17:47:34 21 FPS; 17:48–17:49:20 19–30 FPS. So frames were presented at
  low frame rate for ~5 minutes, which means the FSR output pass was running and the FSR-only hypothesis is
  weaker than it looked: the swapchain worked for minutes before the stall.
- Pipeline-creation burst at 17:47:35 (load of the new scene), then at 17:49:11–17:49:30 the log filled with the
  `Texture fetch constant (8A000000 0F9FF097 ...) has "invalid" type` warning (1925 lines in the minute). After
  17:49:30 nothing was logged; the main thread stack (bt.txt of 20261024T174322) is in
  `vkAcquireNextImageKHR` with an infinite timeout.
- The same constant (`8A000000 0F9FF097 0059E4FF 00A80D11`) is the one the earlier notes called a depth/stencil-like
  surface (base 0x0F9FF000). Its burst preceded the freeze in both mission-2 sessions.
- Next test (not run): the same build with `--gpu_allow_invalid_fetch_constants=true` (the SDK's own suggestion in
  the warning), plus `hang_watch.py`, playing to mission 2. The docs record earlier A/B evidence that this flag did
  not fix dark gameplay; it was never tested against the freeze.

### Invalid fetch constant warning and the unbounded present wait, 2026-10-24

What the warning means (checked in the SDK):
- `TextureCache::BindingInfoFromFetchConstant` (`src/graphics/pipeline/texture/cache.cpp`): for
  `kInvalidTexture` without `--gpu_allow_invalid_fetch_constants` it warns and returns. The key stays invalid, and the
  Vulkan texture cache binds its null image (`texture_cache.cpp:104,511`: R8G8B8A8, zero colour). So the shader reads
  zeros: the warning does not by itself mean a wrong binding, and the flag, which makes the SDK bind the constant as a
  real texture, is the less safe choice.
- The constant in the freeze (`8A000000 0F9FF097 ...`) has dword_0 low bits 0 (invalid type) and a real-looking base
  address. The game draws with it every frame; that is why the warning repeated 1900 times a minute.

Changes made (rebuilt with `build_sdk_ffx.py`, smoke run 60 s: child 0, fatal 0, pacing 18–32 FPS):
1. `src/ui/vulkan/vulkan_presenter.cpp`: `vkAcquireNextImageKHR` uses a 2 s bound instead of `UINT64_MAX`. On
   `VK_TIMEOUT`/`VK_NOT_READY` the paint is skipped and one error is logged ("no swapchain image was released within
   2 s"). The window's event loop keeps running; before, the main thread stayed in the driver forever.
2. `src/graphics/pipeline/texture/cache.cpp`: the invalid-fetch warning is printed once per distinct constant
   (dword_0 and dword_1), not per draw.

What is NOT solved: why the presentation engine stopped returning images. The stacks show the GPU idle (~4%) while the
UI thread waited for a swapchain image, so a submission or semaphore is never signalled. The timeout turns the freeze
into a visible, recoverable error; it does not remove the cause. Verification still needed: play to mission 2 with
`hang_watch.py` and see whether frames resume or the "no swapchain image" error repeats.

### Error register: black screen and mission-2 load freeze, 2026-10-24

Sessions (all: X11/XWayland, profile ARTEM, `tmp/acv-ffx-test` runtime, VSync on, limit 60):

| Session | Output | Result |
|---|---|---|
| `20261024T121958909808Z`, `T122900263727Z` | FSR | Freeze during mission-2 load; log silent after a burst of `invalid` fetch warnings |
| `T123613325669Z` | FSR | Freeze; stacks: main host thread blocked in `pthread_create` inside `librenderdoc.so` (fixed by opt-in patch) |
| `T124322094949Z` | FSR | Mission 2 reached, 17–35 FPS for ~5 min, freeze at 17:49:30; stacks: main thread in `PaintAndPresentImpl` -> `drmSyncobjTimelineWait` |
| `T125707099398Z` | FSR | Black screen reported by the user; frames presented (pacing 19–25 FPS) |
| `T130129215018Z` | bilinear, no FXAA/dither/CAS/FSR | Black screen again; freeze in mission-2 load, stacks `logs/acv-hang-watch/20261024T180129/bt.txt` |

Errors:

1. **Main host thread blocks forever in the paint path.** Capture `20261024T180129`: `RunMainMessageLoop` ->
   `Window::OnPaint` -> `VulkanPresenter::PaintAndPresentImpl` -> NVIDIA -> `drmSyncobjTimelineWait`. The 2 s bound on
   `vkAcquireNextImageKHR` (present in the binary, never logged) did not fire, so the wait is another one:
   `VulkanSubmissionTracker::AwaitSubmissionCompletion` calls `vkWaitForFences(..., UINT64_MAX)`
   (`src/ui/vulkan/vulkan_submission_tracker.cpp:122`), reached from `PaintAndPresentImpl` before drawing
   (`vulkan_presenter.cpp:1573`). The previous paint submission waits on the acquire semaphore
   (`vulkan_presenter.cpp:2246`), which the display server signals when it releases the swapchain image. If the image is
   never released, that fence never signals and the next paint blocks the window's thread for good.
   Guest threads (`Main XThread`, `DrawThread`, `FileMan`, `ResourceBuild`) were all waiting; the GPU was idle (~4%).
   Not established: why the image is not released.
2. **Black screen is independent of FSR.** Present effect bilinear, no post effects, still black. Pacing shows frames
   being presented, so the black picture is produced upstream or shown by the display server; the cause is not found.
3. **Window state of a frozen session** (X properties, not a screenshot): game window `0x1e0002f` 2560x1080+0+0,
   `_NET_WM_STATE_FULLSCREEN`, `IsViewable`, desktop 0; the active window was a different one (`0x200000`), not the game.
   This fits "the display server stops returning images of a window that is not in front" but was not proven.
4. **Harmless but noisy:** `Texture fetch constant (8A000000 0F9FF097 ...) has "invalid" type` (null image is bound, now
   logged once per constant); `ResolvePath(system:\)`; `dlc_sound` misses (`ch09.fsb`, `dlc_se.fsb`,
   `acv_dlc_dlc_se.fev`, `acv_bgm_title.fev`, `acv_talk_$(voicelang).fev`) - not in the retail image.

Fixes already applied: opt-in RenderDoc, bounded acquire timeout, one-time fetch warning, shm unlink, X11 default.
Open: bound the fence waits in the paint path (skip the frame instead of blocking), and test whether the freeze happens
only when the game window is not the active one.

### `system:` device mounted (empty), 2026-10-24

- Change: `acv_linux_probe_app.h` mounts `system:` as `\Device\AcvSystem`, a read-only `HostPathDevice` over the empty
  directory `tmp/acv-system-root` (passed by `probe_linux.py` as `ACV_SYSTEM_ROOT`). No file is created.
- Result (`logs/acv-native-linux/20261024T131124940099Z`, 40 s, child 0): `[acv-vfs] system: mounted as an empty
  read-only device`; `NtCreateFile FAILED path='system:\system.properties' -> 0xc000000f`, the same status the guest got
  before; runtime errors 0 (the `[error] ResolvePath(system:\) failed - device not found` line is gone).
- Guest behaviour is unchanged by design: the loader's file-status check (0x82C33458) still returns false and parsing is
  skipped (analysis above). The classification entry in `acv_settings.BENIGN_RUNTIME_ERRORS` stays for older builds.
- The ELF in the accepted build directory was rebuilt for this (`build_linux.py`, retained overrides). Its hash is now
  `08045fae38fba792...`; the previous ELF is saved in `tmp/acv-backups/pre-system-mount/`. `RUNTIME_BASELINE.json`
  still holds the older ELF hash (it was already out of date before this change: the file in the build directory had
  hash `e3ab3704...`), so it needs a deliberate re-record.
- `dlc_sound:` / `dlc_image:` misses were left alone on request.

### Re-verification of the fixes and bounded paint waits, 2026-10-24

Re-check against the live frozen session (PID 47232, `logs/acv-hang-watch/20261024T180129/`) and the binaries:

| Fix | Check | Result |
|---|---|---|
| RenderDoc opt-in | `/proc/47232/maps` has no `librenderdoc`; `bt.txt` has no renderdoc frame; binary has `ENABLE_VULKAN_RENDERDOC_CAPTURE` | holds |
| `/dev/shm` leak | 0 `xenia_memory_*` files while the game ran and after `kill -9` | holds |
| One-time invalid-fetch warning | 3 warnings in the whole session (was 1900/min) | holds |
| Libraries used | maps show `tmp/acv-ffx-test/librex*.so` | as intended |
| Acquire timeout | message is in the binary, never logged in the freeze | holds, but it is **not** the blocking call |
| Patch files | the previous `launcher-runtime-limiter-xuid-shm.patch` no longer matched the tree (other changes landed in `command_processor.cpp`) | regenerated; all three patches dry-run cleanly on pristine HEAD files |
| Non-FidelityFX build | my timeout constants sat inside `#if REX_HAS_FIDELITYFX_RUNTIME` (a defect: the SDK would not build without FidelityFX) | moved out; the presenter and tracker compile in the non-FFX configuration |

New in this step:
- `VulkanSubmissionTracker::AwaitSubmissionCompletion(index, timeout_ns = UINT64_MAX)` (`submission_tracker.h`,
  `vulkan_submission_tracker.cpp`); after a timeout the older fences are only polled.
- `PaintAndPresentImpl` waits for the previous paint submission for at most 2 s before a paint; if it is not complete,
  one error is logged ("previous paint submission N did not complete within 2 s; skipping the frame") and the frame is
  skipped (nothing is acquired at that point).
- The three waits that happen with an image already acquired stay unbounded, but `AwaitReportingStall` logs once after
  5 s with the location (`old guest output image`, `intermediate image replacement`,
  `swapchain effect pipeline rebuild`). The next freeze will name the wait.
- Patch: `patches/present-acquire-timeout-and-fetch-warning.patch` (presenter, tracker, texture cache hunks).
- Smoke test of the rebuilt runtime on the new ELF (70 s, plain settings): child 0, runtime errors 0, no stall messages,
  pacing 30 FPS.

Not verified: that the bounded wait lets the window recover in the real freeze (needs mission 2). Also unchanged: the
black screen.

### system.properties: search for the file, its keys and related stubs, 2026-10-24

Same method as for the sound banks (hash lookup in the dvdbnd BHD5 index) plus a read-only look at the XEX data:

- Hash search: `h = h*37 + c` over the lower-cased path (the function of `tools/build_dlc_sound_dir.py`) against
  `assets/manifests/acv-dvdbnd-all.tsv` (19 728 files). 1560 candidates (names `system.properties`, `sysprop.*`,
  `system.ini/cfg/xml/txt`, ... under 47 prefixes with `/`, `\`, `system:` and `game:` forms): no hit. Control: `/sound/acv_bgm_bgm.fev` is found.
- Content search: 64 153 extracted files (`assets/original/acv-dvdbnd-all`, `acv-bnd3-all`, `-extracted`, `-nested`) scanned
  for `system.properties`, `sysprop`, `.properties` as ASCII, UTF-16LE and UTF-16BE: no occurrence. The name exists only
  in the XEX.
- XEX data (Ghidra RPC `read-bytes`, read-only), UTF-16BE strings around the path `0x8210FA38`:
  switches `--DLsysprop` (0x8210FA20), `--DLsysprop_nooverwrite` (0x8210F9F0); engine banner
  `dantelion2 v8.8.34#0 (build:Thu Sep 15 18:10:20 2011)` (0x8210FBF8); property keys `Core.System.PlatformString`
  (default `Xbox360`), `Core.System.Region`, `Xenon.IO.CacheDrive.MemorySize`, `Xenon.IO.CacheDrive.ClusterSize`,
  `Core.IO.Install.AutoMount`, `Core.IO.Cache.ClearOnMount`, `Core.IO.Cache.AutoMount`,
  `Xenon.IO.FileSystem.BufferSize`; region words (Asia, America, languages) and the `YES`/`yes` and ` = ` tokens, i.e.
  a `key = value` text file. Decompiling the parser (`0x82C25810`) returns a truncated `noreturn` body, so the
  value handling was read from strings and the saved listings, not reconstructed.
- Conclusion: `system:/system.properties` is a developer-side override file for Dantelion2 engine settings (platform,
  region, cache-drive size, mount behaviour, file buffer size). It is not shipped; when absent the guest skips the parser
  and keeps compiled-in defaults (loader `0x82C22B90`, absence branch `0x82C22C8C`). The failed open is correct behaviour.
- Related kernel stubs that follow the request in every log (`Stub XFileFsDeviceInformation!` x3,
  `IoDismountVolumeByFileHandle(0xf80011ac / 0xf80013ac) - stub`): `xboxkrnl_io_info.cpp:434` answers FILE_DEVICE_UNKNOWN
  with characteristics 0 (the ACV shim changes it to CD-ROM for the disc, `[acv-compat] FileFsDeviceInformation device type
  -> CD-ROM`), `xboxkrnl_io.cpp:731` returns success. They probe the volume of the cache/disc files that the
  `CacheDrive`/`Install` settings concern. No failure follows in the log, but the volume information is not real; whether
  the engine uses it to choose cache sizes was not shown.
- Not done: writing a `system.properties` (defaults are unknown, and an invented file would change engine behaviour
  unobservably). It could be tried as an experiment through `--DLsysprop=<path>` if a specific setting needs testing.

### Error: mission-2 freeze on the accepted build; the instrumented waits did not fire, 2026-10-24

Session `logs/acv-native-linux/20261024T135710111433Z` (accepted build after the fixes: ELF with `system:` mount, FidelityFX
runtime with RenderDoc opt-in, bounded paint waits; plain settings, bilinear output, no FXAA/dither/CAS/FSR; VSync on, limit 60;
under `hang_watch.py`, capture `logs/acv-hang-watch/20261024T185710/`, silence 90 s, taken 19:04:23).

- Timeline: user-reported freeze around 19:02-19:03; the last log write is 19:02:52. Before it: 26 pacing windows, 15.9-28 FPS;
  the `Texture fetch constant (8A000000 0F9FF097 ...) "invalid" type` warning (printed once now) followed by two more pacing
  windows (about 12 s) and then silence. The same order was seen in the earlier freezes: that constant (1280-wide tiled
  surface, format 23 = 24_8_FLOAT, base 0x0F9FF000) is bound shortly before the stall.
- Stacks (`bt.txt`): host main thread again in `RunMainMessageLoop` -> `Window::OnPaint` -> `Presenter::PaintFromUIThread`
  -> `VulkanPresenter::PaintAndPresentImpl` -> NVIDIA -> `drmSyncobjTimelineWait`. `Main XThread`, `DrawThread`, `FileMan`,
  `Core.Res.*` wait in `NtWaitForSingleObjectEx`; `GPU Commands` waits in `CommandProcessor::WorkerThreadMain`; GPU idle.
  No `librenderdoc` frame.
- The new diagnostics did not fire: no "did not complete within 2 s" (bounded wait before the paint) and no "waiting more
  than 5 s" (the three waits after the acquire) in the log, and the 2 s acquire timeout was not logged. So the blocking call
  is inside `PaintAndPresentImpl` but not one of those waits. Candidates with no timeout: `vkQueuePresentKHR`
  (`vulkan_presenter.cpp`, present call) and `vkQueueSubmit` of the paint. Not established which.
- Consequence: the bounded waits do not prevent this freeze. The fix list in the register above stays valid for the leak,
  RenderDoc and the log flood, but the window freeze is not solved.
- Prepared, not built into any runtime yet: `PaintStage` + watchdog thread in `vulkan_presenter.cpp` (names the Vulkan call
  the UI thread has been in for more than 5 s: `vkAcquireNextImageKHR`, `vkQueueSubmit (paint)`, `vkQueuePresentKHR`).
  It compiles in the non-FidelityFX configuration; the FidelityFX runtime was not rebuilt (the rebuild/launch was declined).
- Next test: rebuild with the watchdog and run once with VSync off (FIFO is then not forced, MAILBOX/IMMEDIATE are allowed),
  to see whether the freeze depends on FIFO presentation, and read the watchdog message otherwise.


### Mission-2 watchdog runtime and VSync-off test, 2026-10-24 22:03

PID 147295 and the previous observer were already absent; no process was stopped.
Built the FidelityFX-enabled runtime (operation `01a54baf-79f9-47f1-99c4-b236a006858f`).
PaintStage now also covers the second `vkQueueSubmit (UI fence)`, in addition to acquire,
paint submit and present. The diagnostic reports stalls; it cannot cancel a blocked driver call.
Updated `tools/native-rexglue/patches/present-acquire-timeout-and-fetch-warning.patch`;
application to pristine SDK files checked by operation `b435277f-a54f-47f1-a56a-8aa1192e840b`.

Isolated candidate: `tmp/acv-mission2-watchdog/`, manifest `watchdog-build.json`.
ELF `08045fae…` and GPU `fe3b0dc4…` match the accepted build; runtime is
`708f8755466ed2e8ca4a5bf1bdf8d17e9cd2c48bcd5a160ce65c6a6928773d22`.
The accepted build directory and launcher settings were not replaced.
Settings comparison verified exactly three changed cvars: vsync=false and allow IMMEDIATE/MAILBOX=true.
Bilinear, no FXAA/dither/CAS/FSR, cap60, profile c29e3d83-4642-4133-853f-5f0105946e16 remain as before.

Launched under hang_watch: runtime session `20261024T170256271467Z`, observer
`logs/acv-hang-watch/20261024T220256/`, initial PID243687. Startup check
`1b1b7ced-2c1e-48ad-a1f0-33ede06892e3` confirms all three runtime/GPU/FidelityFX libraries
loaded from the isolated candidate and actual presentation mode0 (IMMEDIATE), 2560x1080,
format44. Guest output remains 1280x720 fitted to 1920x1080 at x320, matching the previous test.
Initial swap cadence ~60; this is not a mission performance result.
Observer silence threshold90s; logger session bound30min. Game left running for manual mission-2 reproduction.
No mission-2 success or freeze diagnosis established at startup. A successful start does not resolve the bug.


### Mission-2 IMMEDIATE freeze: guest thread join defect, 2026-10-24

User reproduced the freeze in session `20261024T170256271467Z` with VSync off / actual IMMEDIATE.
Last cadence log22:08:22.663; invalid fetch warning22:08:06.961. Manual capture22:08:55:
`logs/acv-hang-watch/20261024T220256/bt.txt`. No watchdog stall report or acquire timeout.
Thus disabling FIFO does not resolve the freeze; the previous submit/present diagnosis was a hypothesis.

The UI snapshot return address is runtime+0x2f716a. Saved `acquire-callsite.txt` and `runtime-maps.txt`
identify the acquire call, including timeout0x77359400 (2,000,000,000ns), followed by clearing PaintStage.
This single sample does not prove a permanently blocked acquire. `thread-sampling.json` shows the UI
thread consuming CPU and changing wait states; the watchdog thread remains alive in nanosleep.

More actionable: Main XThread frame1 is `PosixCondition<Thread>::post_execution`, then
`PosixConditionBase::Wait` / `XObject::Wait` / `NtWaitForSingleObjectEx`, through guest
0x82C98B48 / 0x82B4FA28 / 0x82B4F1F0. SDK `src/core/threading_posix.cpp` called
pthread_join and destroyed the suspend semaphore on EVERY successful wait on a finished thread.
The thread remains signaled, so subsequent waits repeat both operations. Rejoining is invalid and a
reused pthread_t can refer to an unrelated thread. Actual target/reuse in this freeze has not been captured.

Regression `tools/native-rexglue/repeated_thread_wait_test.cpp` calls the real runtime Thread/Wait APIs
three times, interposes pthread_join to count calls and prevents undefined repeated native joins in the baseline.
Baseline708f8755 reports3 joins instead of1 (operation `27053d2f-ea72-44de-abf0-b80c84d70c89`).
Initial test harness also intercepted TimerQueue teardown; corrected to track only the target pthread.
Fix guards reaping with a mutex-protected completion flag, checks join result, and destroys the semaphore once.
Patch `tools/native-rexglue/patches/posix-thread-wait-reap-once.patch`. Gameplay attribution remains provisional.


Join-fix validation: runtime build `f44f24a4-657c-4e42-a30e-4fa1425fe2c4` succeeded;
same regression passes with exactly1 join for3 successful waits (`50fdd9aa-2caa-428a-9af5-3ec03b90edc6`).
Patch applies to pristine SDK HEAD. Candidate `tmp/acv-mission2-join-fix/build.json` pins
runtime `2b346825c1a115c653bd37fb785686e8d2cc6e8694ef86e4264591d7c5bc7d12`;
ELF, GPU plugin, FidelityFX library and settings copied unchanged from the IMMEDIATE trial.
After saving evidence, stopped only captured test PID243687 (SIGTERM).
New gameplay session `20261024T171241321091Z`, observer `logs/acv-hang-watch/20261024T221241/`;
left running for manual mission-2 reproduction. The SDK regression is fixed; mission-2 outcome is pending.


### Mission 2 accepted by user; another mission selection abort, 2026-10-24

User confirms mission2 is now resolved on runtime2b346825 after the join fix. Scope is that reported transition,
not all missions. Same session `20261024T171241321091Z` later aborts at22:17:22.058 in Ac4SortieScene,
PID253093/TID254492: InvalidFunctionTrap guest target0. Process is gone.
System core is present for PID253093; extracted analysis in `logs/acv-mission-select-20261024/`:
`core-debug.txt`, `object-core.txt`, `factory-inputs.txt`. No replay required to obtain these facts.

Crash stack: sub8274D8E8 -> sub8274AF08 -> sub8274E4C8 -> sub825F68A8 -> sub825F7688.
Host return site0x4844758, generated shard251:9429; PPCContext recovered from host r14,
guest base from rbx=0x100000000. r3=r30=0, LR8274D9FC, CTR0.
Generated translation shows sub82753C48 returns an object, then caller loads its vtable+48 without
checking null (guest indirect call8274D9F8). Factory failure branch exists; which branch failed is not yet known.
Caller0x410263F0, manager0x41026D30. Input fields+68=4244FB10 and+76=40FBAB80 are nonzero;
manager+8/+56/+60/+64 are zero, manager+76=42610920. These are core observations, not recovered type names.
Ghidra saved-project daemon was unavailable; no live DB modified. Guest semantics here are provisional
translation/core analysis and still need original instruction verification before a guest fix.

Missing dlc_image thumbnails (briefingmap07, e1020/e2020/e0240) precede the abort; causal connection not proven.
Do not skip the null call or invent an object as a fix. Next: identify the selected mission and the factory failure path.

hang_watch previously detached on SIGABRT without capturing because only silence/manual triggers were handled.
It now records signal stops and skips detach on normal process exit. Real GDB tests /bin/true and SIGABRT pass
(operation `4cbe86a3-81e6-4095-b039-faae66dfeb43`, `watch-tests/` evidence).


### Mission selection: registry and initializer investigation, 2026-10-25

Further read-only core analysis (`logs/acv-mission-select-20261024/cause-status.json`):
registry44019520 has52 entries, including ac_0 ->479F8140 and ac_2 ->47238110.
The recovered creation descriptor contains key `ac_0` (stack7091FC10). Both AC resources have
vtable820C2008 and state4 at+26236. Original accessor82945CD0 blocks states1/2/3;
this snapshot does not show a missing key or that blocked loading state.
Registry enumeration source: virtual lookup82767300 ->82A22F90 ->82A241F0;
node key pointer+16/value+60, head44019F00. Dumps and vtables saved beside the JSON.

Creation chain narrowed to82753C48 ->82753448 ->82732868 ->82751AD8 ->828DB650 ->828DB268.
Stale guest-stack return words include828DB758 (failure cleanup),82751DCC,82732A5C and828DB2A0.
These support the reconstructed chain but cannot prove the exact earlier conditional branch.
Failed object418EE060 has been destructed (base vtable82002A50 and fields112/120/128 zero);
its zero fields must not be mistaken for pre-cleanup input evidence.
Original PPC words for crash/lookup branches saved in retail-lookup-words.txt and retail-branch-words.txt.
XEX SHA4fac87348f827e84633013375012284cd8c769a3c155a675b2989757e666a12b rechecked.

Root cause of initializer rejection remains open. No guest logic bypass or synthetic object added.
Prepared `tools/native-rexglue/mission_select_trace.py`: bounded6000 JSONL rows,13 verified native
function entry symbols and finish breakpoints; reads PPCContext arguments/returns and object fields.
`hang_watch.py --diagnostics` loads the script after starti. Selected runtime/ELF/GPU unchanged.
New manual reproduction session20261025T110650354101Z, observer
`logs/acv-hang-watch/20261025T160650/`, trace scene-create.jsonl reports trace_ready.
Startup runs X11; game left open to repeat the problematic selection. Trace is diagnostic and affects timing.


### Mission selection: empty resource name and headless UTF-16 corruption, 2026-10-25

Reproduction `20261025T110650354101Z` aborted at16:08:13, PID6020. Diagnostic51 rows:
`logs/acv-hang-watch/20261025T160650/scene-create.jsonl`, bt.txt and cause-proof.json.
All13 traced allocations succeeded. First recorded failure828E8220 returned0; base initialization
828E71A8/8290B6E8 returned1, then828DB268 returned0 because required field+120 was0 before cleanup.
This supersedes the earlier uncertainty about post-cleanup fields.

Important correction: the descriptor's first string `ac_0` is not the resource lookup key.
828E80C0 dispatches descriptor vtable slot12 ->824E2550, which loads descriptor+596.
That points to an empty buffer708EFE5C. Descriptor708EFC00, actual-key.json.
Name originates from nested object+512, set by825E34E0 then copied by82751AD8 to descriptor+592.
Input40FA0878 has type1/index0; table835664A0 contains4,5, so825E34E0 takes the alternate
wide-name branch, not the ac_%d branch. Alternate name at40FA7030 contains bytes
41005200540045004D000000 (UTF-16LE ARTEM), contrary to guest UTF-16BE. See name-source.json/name-branch.json.
The empty converted key causes lookup rejection, factory null return and unchecked virtual call8274D9F8.

SDK defect independently reproduced: native shim StoreGuestWide writes UTF-16BE then aliases default_text
and buffer for XamShowKeyboardUI. SDK headless code passed raw mapped_wstring.value() to
copy_and_swap_truncating, swapping the already guest-endian bytes a second time. Same bug exists in
no-ImGui fallback. MappedPtr<char16_t>::value is a raw u16string_view, not an endian-converting accessor.
Fix uses bounded guest-to-guest CopyGuestUtf16Be on host_address pointers, memmove for overlap,
BE preservation and no trailing high surrogate. Interactive UTF-8-to-guest conversion retains its existing swap.
Source: tools/native-rexglue/profile_text.h and SDK src/kernel/xam/xam_ui.cpp.
Incremental patch keyboard-guest-endian-copy.patch applies over prior keyboard changes; shared header accompanies it.

Actual SDK headless lambda + real MappedPtr regression: baseline fails with byte swap
(`53670720-ea85-49d0-9a3a-7e6af0e86053`); fix passes ASan/UBSan (`ba8df2a9-7c25-4a1e-92f9-1695141cc0fc`).
Covers alias/disjoint copy, null input, terminator/guards and surrogate truncation.
Existing profile text tests pass all1112063 Unicode scalars (`47063e28-f5c9-45bd-810a-26680c7fcdcd`).
Initial test stub used pointer value() unlike SDK string_view; corrected to real MappedPtr before final validation.
Runtime build5a40a028-db83-40b1-aacd-723bb8eac953 succeeded after using host_address at the SDK boundary.

Isolated candidate `tmp/acv-mission-select-keyboard-fix/build.json`, runtime
f3600acaaa0889053507d92455720937e63f473a2dc9f58405a06a5234af5c97;
ELF08045fae / GPUfe3b0dc4 and settings unchanged from join-fix trial.
New game session20261025T112001461079Z, observer logs/acv-hang-watch/20261025T162001/;
trace now also records the actual lookup name. Game left running for mission-selection verification.
Existing save bytes were not migrated. Confirm the corrected input reaches lookup before accepting the gameplay fix.


### Resume checkpoint: selection still aborts with old save, 2026-10-25 16:40

User reported another failure on the keyboard-fix candidate. Session20261025T112001461079Z,
runtimef3600aca, PID22427/TID26571, ended SIGABRT16:40:11 at the same null virtual call.
Observer `logs/acv-hang-watch/20261025T162001/` contains59 trace rows, bt.txt and hang.json.
Trace confirms825E34E0 takes82A20498 (alternate wide-name conversion), actual resource_name remains
empty,82776540/828E8220 return0, then mandatory field+120 remains0 and scene creation fails.
No XamShowKeyboardUI call occurred in this session. Thus the input-boundary fix is tested, but it did
not repair the loaded data or resolve this mission-selection crash. Do not mark the gameplay fix accepted.

Read-only new-core extraction: name-source.json/name-core.txt in that observer folder.
Input40FA0878, wide name40FA7030 still has bytes41005200540045004D000000 (ARTEM in LE order).
This is consistent with pre-existing saved data; causal confirmation requires an isolated migration/load test.
It does not prove the on-disk format globally expects BE: file encoding, load conversion and integrity rules
must be checked before modifying the user's save.

Current save exists, contrary to the older PLAN statement:
`tmp/acv-user-data/profiles/c29e3d83-4642-4133-853f-5f0105946e16/userdata/00094AF2A434841E/4E4D0853/00000001/GAMEDAT0000/APGD.DAT`.
Size230672 (0x38510), SHA256051fc16d6d4bc094edf1b0a1d87ac57a717157d6023e5f77a5c58ef98e7eae81.
Two aligned ARTEM UTF-16LE sequences found at0x27728 and0x37C08. Matches for BE search at preceding
odd offsets overlap the same bytes and are not independent BE strings. GAMEDAT0000_CONTENTINFO is16bytes,
SHA6a15a9c4762f51c220816c86f3be29d1e89efcec7aa4982487c5dfe1cfc268d6; PAINT.DAT also exists.
CRC32/Adler32/basic sums were sampled read-only; no integrity algorithm was established.

Next concrete step: preserve/hash the original profile, create an isolated test copy, establish the encoding
and integrity handling for these two fields, then test a narrow repair with the fixed runtime and trace.
No save was edited, no isolated migration copy was created, no repair was launched before the user's
request to save progress. Continue from this checkpoint, not from a claim that mission selection is fixed.

### Isolated save-name trial and probe error accounting, 2026-10-25

Verified backup `20261025T114543011388Z` was made under the source profile lock.
[Trial manifest](../logs/acv-save-name-trial-20261025/trial.json) records the original
userdata hashes and a separate copy under `tmp/acv-user-data-name-trial/`.
Only the two10-byte ARTEM fields at0x27728/0x37C08 were changed in the copy, LE→BE.
Original APGD SHA remains051fc16d6d4bc094edf1b0a1d87ac57a717157d6023e5f77a5c58ef98e7eae81;
trial SHA2eca8c41bfc302ee118661d8497ca77efde6387fd234522125e7b4d59bfbf522.
This is an experiment, not a validated save-format migration or checksum repair.

Session20261025T114554846407Z used the copy through new hang_watch --profiles-root.
Observer `logs/acv-hang-watch/20261025T164554/scene-create.jsonl` contains only trace_ready.
Bounded synthesized keyboard inputs did not reach the failing selection. No game remained live
at the resume check; there is no completed launch.json for this GDB-observed session.
Neither loading the modified save nor integrity acceptance is established. The original data
were rehashed unchanged. Mission selection remains blocked on a reproducible menu transition.
The GDB observer dry-run releases the profile lock before attaching its own child; unlike the
normal probe session, it does not establish inherited writer-lock protection. Keep these diagnostic
runs isolated from real profile writers; normal launcher/probe inheritance is tested separately.

An independent probe bug was found during TRG integration: follow_runtime assigned
fatal_error_count without declaring it global, so the first fatal log record raised
UnboundLocalError in the observer thread. Actual-function regression failed before the fix
(operation96debd6c-3b27-47ec-8473-bbc8cd6decc5), passes after the one-line fix
(adb45aa1-2e32-4b26-a617-ec28ae7afafa). This corrects error accounting, not mission logic.


### Mission accepted and debugger failure, 2026-10-25

The user confirmed that the mission now works after the isolated profile trial.
Normal launch `20261025T165411391172Z` (PID38590) used the same runtimef3600aca,
ELF08045fae and `tmp/acv-user-data-name-trial/`, without GDB. It progressed through
the recorded menu replay and continued running until the180s limit: child0/fatal0
(operation02e3d1f5-b24d-4728-b4be-c1a0a1c518ae).
This accepts the reported mission, not all missions or an automatic save migration.
The original profile remains separate. Trial name edits are still an experiment;
on-disk integrity handling and a complete save/write/exit/load cycle are open.

The immediately preceding disappearance in session20261025T165115172727Z was a
debugger failure. System coredump identifies `/usr/bin/gdb`, PID36997, SIGABRT
at21:52:21; the observed game PID37021 subsequently ended with-9, with no guest
fatal log. Native GDB caller instructions at executable offset0x26181E call the
internal-error handler with source `../../gdb/breakpoint.c`, line9182 (0x23DE),
function `find_program_space_for_breakpoint`, assertion `thr != nullptr`.
Core/callsite evidence operations4093a9eb-6ddc-4e2d-8cda-e7649bf220fa,
db48516a-8285-44bf-9c5b-befb2f0224cd,9924d4d3-e58a-4221-81aa-65852a2f8828,
1c257802-63c5-40b9-8bc7-3db942391530. The precise upstream GDB defect is not patched.

Our workaround removes FinishBreakpoint from the scene/name/storage observers.
`tools/native-rexglue/gdb_return_probe.py` matches native x86-64 return address,
thread ptid and post-return stack pointer, with global breakpoints and explicit
thread-exit cleanup. It does not modify guest memory or game behavior.
Regression139606fa-7bfb-4c09-8c0e-69864e3741a9 exercises160 concurrent/exiting workers,
3840 return callbacks, duplicate observers and abandoned pthread_exit calls.
The optional per-frame input trace is off by default and bounded to600 calls
when explicitly enabled. Existing diagnostic input overhead must not be reported
as game performance.

The earlier hang_watch dry-run lock limitation is superseded: hang_watch now
starts the normal probe under GDB and follows its game exec; the probe and game
retain the inherited profile lock. Actual GDB supervisor/detach/lock regression
998eddf3-2e48-4243-963c-70015bdac2a9 passed.

User-reported6MB warning was investigated read-only in
`logs/acv-hang-watch/20261025T215114/storage.jsonl`.
XamContentGetDeviceData(device1) returned success and free_bytes0xC0000000
(3GiB); repeated missing `con:\GAMEDAT0000_CONTENT` is a separate observed
file-open failure. Raw cache-volume queries also reported a NullDevice1MiB
capacity; no causal link to the6MB warning was established. Do not mask it with
invented files or an unconditional storage-success shim. The next normal run
loaded the mission; the warning is not a confirmed current blocker.

Retail encoding evidence is now independently exercised:
`test_mission_name_codec.py` invokes the exact generated82C9AF88 leaf after
checking its raw-PPC origin and live codec globals. Guest-BE ARTEM converts to
five ASCII bytes; old LE bytes stop at unmapped U+4100 and produce zero bytes.
UBSan host regression8d04601f-4402-4e26-b216-062d111d81b6 passed.
This explains the old empty name; it does not establish every save-field format.

Next: measure warm-cache mission cadence plus CPU/GPU utilization without GDB,
then address the measured bottleneck. Target60FPS comes from PLAN_GUI/settings;
draw-call, memory and load-time budgets are unset and not assessed.


### WaitMultiple fractional sleep and measured CPU fix, 2026-10-25

Inputs FOUND: normal runtime logs, /proc per-thread samples, NVIDIA utilization,
Linux user-IP perf samples, SDK source and actual shared-library host tests.
Performance YAML/helper are absent. Target60FPS/16.67ms comes from PLAN_GUI and
effective settings. Draw-call, memory and load-time budgets are unset: NOT ASSESSED.

Baseline session20261025T170034719156Z ran190s without GDB, child0/fatal0.
135 CPU/GPU samples are in logs/acv-fps-warm-mission-20261025/. Retained cache
does not eliminate compilation of newly encountered pipelines. Heavy-scene
submitted cadence was about22FPS, with GPU often60–70% busy.

A separate100s perf_event_open run collected9517 user-IP samples, zero lost,
no unavailable threads: logs/acv-fps-user-ips-20261025/summary.json.
Audio Worker contributed3930 samples, dominated by pthread_mutex_trylock,
WaitMultiple, semaphore predicates and unlocks. Sampler uses per-task CPU_CLOCK
50Hz without ptrace/debugger or injected signals. Actual hot-symbol regression
88c0309f-4e80-4edf-8673-f01606d8437e passed.

Confirmed defect: PosixConditionBase::WaitMultiple truncated the remainder of
a1ms alertable slice to integer milliseconds. sleep_for(0) repeatedly checked65
mutexes for essentially the whole slice. The contention retry also yielded
without checking the deadline. Fix preserves steady_clock duration precision
and applies the same bounded sleep/deadline under contention. Signal inspection,
consumption and atomic WaitAll logic are unchanged. Existing once-only join stays.
Source: SDK src/core/threading_posix.cpp.
Patch: tools/native-rexglue/patches/posix-wait-multiple-fractional-sleep.patch.

Actual shared-library wait_multiple_cpu_test.cpp: baseline120ms wait used119.37ms
CPU (99.5%, expected failure eda3665e-6e82-4f5c-8be6-f0c6fbbaa34b); fixed used1.30ms
(1.1%,483487a0-51e2-499e-81c1-a3e30467acf1). Timeout, auto-reset/index,100 concurrent
semaphore releases, WaitAll atomicity and alertable callbacks pass. Repeated wait
still calls pthread_join once (7f6eb0e4-5894-481f-8329-fb2c646d1317).

Runtime SHA256 ca04c1c60bbd3b2cb3ed147fb777bd395cba641d4c46781f360cd54e97efa7ca.
Candidate tmp/acv-wait-poll-fix/build.json retains ELF08045fae, GPUfe3b0dc4 and
FFX0933159a. Normal session20261025T171212311161Z mapped the exact fixed library,
ran190s, child0/fatal0. Audio-thread CPU in window60..135s decreased78.05%→0.56%.
[Comparison](../logs/acv-wait-poll-fix-20261025/comparison.json).
These are observed windows, not identical-frame gameplay A/B.

CPU waste is fixed; stable60FPS remains OPEN. Later heavy-scene submitted cadence
remained16–21FPS. GPU command processing, guest scene work and pipeline stalls need
separate profiling; no FPS uplift or gameplay-speed improvement is claimed.
No >60FPS/timestep change was made.

Separate185-file TRG candidate: ../ACV-Linux-TRG-WaitFix-20261025/start.sh.
Manifest/no symlinks/no Qt/ICU/dependency checks passed
79705c7e-528f-4cf5-b79a-c537f80a290a. Earlier packages and original saves retained.
This is a candidate, not a silent baseline replacement.

GDB callback exceptions are now saved in gdb-run.txt/observer-error.json and
return exit1 after detaching the game. Actual intentional-failure/lock regression
a7dfed32-ed42-49da-baa0-f32df2fd5649 passed. Frontend verification now waits for
deferred GPU/FFX dlopen before checking exact module paths: the earlier immediate
assertion raced startup, rather than proving a missing packaged plugin.

WaitFix package X11 frontend/probe smoke PASS: cwd /, selected local libraries, no Qt/ICU, game and writer lock survive GUI exit, lock releases after game exit; child0/fatal0. [Checkpoint](../logs/acv-waitfix-package-smoke-20261025-r2/checkpoint.json).


### FPS diagnostics gated at game age90s, 2026-10-25

User requested gameplay diagnostics only90s after game start. perf_gate.py reads
Linux process start ticks and CLOCK_BOOTTIME, checks PID/executable identity,
and delays output-directory creation and perf event enablement until the gate.
perf_ips.py and perf_sample.py now default to --after-start90. Host fixtures
explicitly use0 or2s. Actual process gate regression9debe404-f045-4401-ab62-a29f9331afd8
verifies no earlier output from either profiler. Ordinary startup/error runtime
logs remain separate; diagnostic gameplay sampling excludes the first90s.

Session20261025T172256652085Z, PID58307, runtimeca04c1c6/GPUfe3b0dc4,
normal300s launch: child0/fatal0. Both profilers started at age90.000408s.
120 CPU/GPU rows span ages90.0058..209.0048;6917 native user-IP samples,
zero lost/unavailable. [Provenance](../logs/acv-fps-after90-20261025/provenance.json),
[summary](../logs/acv-fps-after90-20261025/summary.json), native symbols in
logs/acv-fps-after90-20261025/ips/summary.json.
GPU utilization averaged65.37% (29..70); command thread38.66% CPU and frequent
do_sys_poll waits. The hot libc offsets are AVX memcpy loop instructions,
confirmed against the local libc executable, not inferred from an unknown symbol.

An opt-in Vulkan CPU wall-time observer now separates fence waits, Resolve
readback, shared uploads, draws and end-submission. Header gpu_stage_profile.h
must be copied to SDK src/graphics/vulkan/acv_gpu_stage_profile.h before applying
patches/vulkan-delayed-stage-profile.patch. It changes no Vulkan calls/results,
barriers, buffer ownership or publication semantics. Disabled unless
ACV_GPU_STAGE_PROFILE names a file; hard process-age90s gate,5s windows.
Nested stage durations are inclusive and must not be added together.
[Source manifest](../logs/acv-fps-after90-source-20261025/manifest.json).
Actual C++ pre90s no-file and aggregation regression
3aee9d7a-aee7-4ec7-9d53-3a850b9bd878 passed.

Diagnostic candidate tmp/acv-gpu-stage-profile/build.json:
GPU SHA1a22f7522f07607d6571551b002264f5ab0bebe300440b7ac8d0a24673c104d3,
ELF/runtime/FFX unchanged. It does not replace the standalone/accepted GPU.
Session20261025T172904539408Z, PID66532,300s child0/fatal0.
No stage file existed at process age38.08s; first record age95.0068s.
The first150s of recorded windows showed inclusive wall fractions:
Resolve readback78.11%, submission-fence checks63.80%, shared upload2.53%,
draw92.37% and end-submission4.12%. Fence time is nested in readback/draw.
About3400–4000 resolves per5s repeatedly force GPU completion in full mode.
This identifies synchronous Resolve readback as the dominant measured stall;
upload CPU time alone does not explain the loss.

An isolated fast-readback comparison uses a new copied profile store
tmp/acv-user-data-fps-fast-trial/, same UUID/XUID and explicit per-launch
--readback_resolve=fast. Source copy was taken only after acquiring its writer lock;
game exit precedes the normal probe releasing that lock, so waiting for /proc/exe
alone was insufficient and was corrected. Full remains the normal default.
[Trial command](../logs/acv-fps-after90-20261025/fast-trial.json).
Fast returns a previous generation and has not passed image/generation acceptance;
it is a diagnostic comparison, not an accepted FPS fix.

Next work: reduce the observed Resolve synchronization while preserving CPU-visible
generation correctness. SDK xmemory.h data providers are currently TODO; eager
full readback cannot safely be removed merely because it is slow. The current
working full-mode texture/readback fix and original profile must be retained.


Fast comparison completed: session20261025T173444622396Z, PID68655,240s,
child0/fatal0. In comparable post90s recorded windows, Resolve readback remained
76.80% inclusive wall time versus77.35% in full; fence checks60.86% versus63.34%.
No stable60FPS result. [Mode comparison](../logs/acv-fps-after90-20261025/readback-mode-comparison.json).
These are observed play windows, not deterministic identical-frame A/B.
The experiment does not justify changing the default or accepting delayed images.
[Resume checkpoint](../logs/acv-fps-after90-20261025/checkpoint.json).

The subsequent split measurement below resolves the slot/miss uncertainty.
Full remains the normal mode; delayed images have not passed ACV acceptance.


### Resolve wait attribution and fast-generation correction, 2026-10-26

Opt-in timers now separate allocation, write-slot reuse, missing/incomplete previous
results, full-mode waits, cache invalidation and CPU publication memcpy. The90s
process-age gate remains active. C++ host tests exercise every metric and absence
of an early output file. Full session20261025T212958686635Z and old-fast
session20261025T213304904190Z each completed180s with child0/fatal0 on X11,
2560x1080 output, bilinear, no CAS/FSR, limiter60, VSync off. Diagnostic GPU
b5e9ad41, runtimeca04c1c6; [candidate manifest](../tmp/acv-gpu-split-profile/build.json).

[Duration-weighted timer report](../logs/acv-fps-readback-split-20261026/baseline-summary.json):

| Inclusive wall metric | Full,90.063s windows | Old fast,85.119s windows |
|---|---:|---:|
| Current-result wait in full |57.37%|0%|
| Current-result fallback because previous is incomplete |0%|48.72%|
| Fallback because previous is missing |0%|0.07%|
| Pending write-slot reuse |0 calls|0 calls|
| CPU publication memcpy |12.65%|15.24%|
| Cache invalidate |0.017%|0.026%|

These are observed sessions, not identical-frame A/B. The old-fast run partly
overlapped the next incremental shader/plugin build, so its FPS is not a clean
performance comparison. Timers establish which branch executes, not the size of
a prospective speedup. Nested percentages must not be summed. First records occur
after95s; explicit file-absence checks at89.095s/full and89.165s/fast passed.

The fast path had9217 waits caused by an existing but unfinished previous result,
versus7 missing-result fallbacks. SDK command_processor.cpp
IssueCopy_ReadbackResolvePath treated both as cache misses and waited for the new
write slot. This serializes fast even when the previous generation is valid.
Fast now waits for that valid previous generation and publishes its bytes. Missing
or undersized previous data still falls back to the current generation. Full and
some retain their existing publication policy. Wait/invalidate failures still
reject publication; slot completion guards remain enabled.

[Reproducible fast patch](../tools/native-rexglue/patches/vulkan-fast-previous-generation-wait.patch)
is applied after the diagnostic patch. Actual SDK-tail ASan/UBSan host regression
0295a809-ecde-475c-98ea-c25f01cdc52c proves previous ticket9/previous bytes0x71
in fast, ticket10/current bytes0x42 in full, and no publication after failed waits.
Both patches reproduce their pinned source with zero fuzz. Candidate GPU45dbaab6
is isolated in tmp/acv-gpu-fast-generation-fix/; normal package/GPU, original save
and canonical config are unchanged. Runtime session20261025T213630477137Z ran180s,
child0/fatal0. Gate file absent at89.241s, first timer record after95s. Corrected
fast-generation branch executes; [measured windows](../logs/acv-fps-readback-split-20261026/fast-fixed-summary.json)
still show75.10% resolve wall time,60.44% waiting for the previous generation,
13.62% memcpy. Heavy-scene cadence remains approximately15–17FPS. No speedup or
image correctness is accepted from this observed session.

A separate next experiment submits each fast readback copy without an immediate
CPU wait, before testing the previous generation. Pending old copies can otherwise
belong to an open submission also containing new Resolve work, so choosing an
older ticket alone need not provide pipelining. This is a scheduling hypothesis,
not established as the dominant measured cause. The experiment retains full/some,
slot safety, invalidation and failed-submit rejection. Actual SDK-tail ASan/UBSan
bc52c766-20a3-4dab-b623-173c83ba2836 verifies both ring indices, old/new bytes,
submit failure and invalidate failure. Patch vulkan-fast-readback-submit-experiment.patch
is the third step after the diagnostic and generation-selection patches;
zero-fuzz chain reproduction passes. Build succeeded (bc07dd18-d802-446f-b36b-7a6d318b39b0). Isolated candidate
GPUd2404533, [manifest](../tmp/acv-gpu-fast-submit-experiment/build.json).
Session20261025T214132535324Z completed180s with child0/fatal0, termination by
test duration. Gate file absent at89.155s and all three mapped libraries matched
the isolated candidate hashes. Full/default package was not changed.

The user confirmed entering gameplay in this session; first followup observed
process age130.969s. Subsequent25 CPU/GPU samples cover147.152..171.151s;
1869 user-IP samples, lost0/unavailable0. GPU utilization88.4% (86..91),
GPU command CPU56.71% of one core, Draw27.33%, main22.83%. Recorded late submitted
cadence is about22–25FPS; this observed session is not an identical-input comparison
with the preceding15–17FPS run. In confirmed gameplay windows135..180s, inclusive
Resolve66.52%, previous-generation wait38.40%, memcpy21.14%, fast-submit5.63%.
[Confirmed gameplay evidence](../logs/acv-fps-readback-split-20261026/confirmed-gameplay-summary.json),
[user confirmation](../logs/acv-fps-readback-split-20261026/user-gameplay-confirmation.json),
[full timer report](../logs/acv-fps-readback-split-20261026/fast-submit-summary.json).
Image/generation correctness of delayed fast mode is still unaccepted; no stable60FPS
or accurate-full-mode speedup is claimed. The explicit visual feedback question is
pending; the candidate is not promoted.

CPU samples also identify the TimerQueue dispatcher:9.33% of one core,88 samples,
86.36% in TimerThreadMain and11.36% sched_yield. Source core/timer_queue.cpp uses
DisruptorPlus spin_wait_strategy even when idle. This is a separate CPU-efficiency
candidate, not evidence that removing it will reach60FPS. Evaluation must preserve
timer deadlines, concurrent producers, cancellation, callback self-disarm and shutdown.

Stable60FPS and full-mode performance remain OPEN. Correct full readback cannot
be replaced by a previous generation: ACV has an immediate CPU texture consumer.
SDK xmemory.h data providers and PhysicalHeap::EnableAccessCallbacks still mark
read providers TODO/assert_false. A general deferred implementation must cover
CPU first-read, all physical aliases, writes/free/reallocation, GPU-worker queue
ownership and release of the global memory lock; the private leased-range
prototype is not a global coherence solution.


### TimerQueue blocking wait and deadline validation, 2026-10-26

User-confirmed gameplay IP samples attributed the anonymous timer dispatcher to
TimerThreadMain/sched_yield,9.33% of one core. SDK src/core/timer_queue.cpp used
disruptorplus::spin_wait_strategy. Replaced only its wait strategy with the existing
blocking_wait_strategy: producer publication wakes the dispatcher and the earliest
timer deadline bounds the wait. Timer ordering, callback execution, recurrence,
Disarm state machine, ring capacity and shutdown kick are retained.

The previously unused third-party strategy had reversed argument order in both
condition_variable timed overloads. Corrected to wait_for(lock, duration, predicate)
and wait_until(lock, deadline, predicate); actual-header duration/deadline timeout,
publication wakeup and untimed wait pass (e620be34-7b4e-442a-84f4-f9e64ba60fc1).
[Reproducible SDK/dependency patch](../tools/native-rexglue/patches/timer-queue-blocking-wait.patch)
reproduces exact source with zero fuzz; dependency attribution is retained.

[Actual shared-library fixture](../tools/native-rexglue/timer_queue_cpu_test.cpp):

| Host metric | Before, runtimeca04c1c6 | After, runtime7b1f69a0 |
|---|---:|---:|
| Idle process CPU,700ms |1.538%|0.0016%|
| Recurring1ms timer,400 callbacks, process CPU |10.0262%|1.1863%|
|128 deadline callbacks, p95 lateness |0.9955ms|0.0690ms|
| Maximum deadline lateness |1.0595ms|0.2909ms|

Baseline d7f43b50-9ee2-474d-a8cb-4badb9dd38f0; fixed
cc2b2b4c-c4c1-445a-8878-8be6df7d28fe. Both execute2048 callbacks from4 concurrent
producers across the512-entry ring, earlier-deadline insertion, recurring self-disarm
and external cancellation waiting for a running callback. No early deadline callback.
These host measurements establish CPU efficiency and scheduler behavior, not game FPS.
Existing WaitMultiple CPU test remains≈1%, event/semaphore/WaitAll/alertable checks
pass;3 successful Thread waits still join exactly once (9312a47c-d159-4d3c-ae36-0889040b15b6).

[Isolated candidate manifest](../tmp/acv-timer-queue-fix/build.json):
runtime7b1f69a00408334ca3f50a6c2d50a5d8de9aa1f1ddcacfae80109fcbab7139dc,
GPUd2404533 retained for the controlled fast-mode comparison, ELF08045fae/FFX0933159a.
Live session20261025T215215281009Z, PID69477, probe --session without a3-minute game
limit (outer logger bound30min). The user confirmed gameplay at followup age83.179s;
no stage output file existed then. CPU/IP gates started at90.000363/90.000362s.
Actual X11 output settled to2560x1080 after initial resize; bilinear/no CAS/FSR,
limiter60/VSync off. All candidate mapped library paths and hashes match, canonical
config and original save unchanged. [Live verification](../logs/acv-timer-queue-fix-20261026/live-verification.json),
[user gameplay confirmation](../logs/acv-timer-queue-fix-20261026/user-gameplay-confirmation.json).
Post90s runtime CPU verification completed:90 rows at90.005..179.004s,
6393 user-IP samples, lost0/unavailable0. Timer dispatcher TID69481 is confirmed
by TimerThreadMain/kernel-clock callback symbols and pthread_cond_clockwait;
CPU9.33%→0.640% of one core. GPU utilization87.94% (83..91). Later cadence remains
about21–24FPS; timer CPU waste is fixed, stable60FPS is not. Observed windows are
not an identical-input FPS benchmark. Resolve65.48%, previous-generation wait38.29%,
memcpy20.23% remain dominant inclusive wall fractions in90..180s.
[Runtime comparison](../logs/acv-timer-queue-fix-20261026/runtime-comparison.json),
[immutable timer snapshot/report](../logs/acv-timer-queue-fix-20261026/stage-summary.json),
[checkpoint](../logs/acv-timer-queue-fix-20261026/checkpoint.json).
Game remains open; shutdown/normal
exit for this session is not yet checked. Fast image/generation acceptance and
stable60FPS remain open; no normal-package promotion.

### FBO retest: black gameplay confirmed, 2026-10-26

Session `20261025T220523182625Z`, timer runtime7b1f69a0/GPUd2404533,
explicit FBO + full readback, X11/2560x1080, VSync off, cap60. User reported
black gameplay; the owned test was stopped (child0/fatal0). FBO is rejected
for the working configuration. Normal defaults and accepted package were not changed.

The capture triggered at100s (`acv_frame70994.rdc`) contains only3 draws.
Replay event66 has exactly zero RGB in both the1280x720 source and2560x1080
output. This localizes the black image upstream of final scaling, but this
presentation-only capture cannot identify the failing scene draw/resolve.
[Checkpoint and capture checksum](../logs/acv-fbo-retest-20261026/checkpoint.json),
[numeric inspection](../logs/acv-fbo-retest-20261026/present-inspect.json).

Added [GPU replay timestamp tool](../tools/native-rexglue/replay_gpu_times.cpp),
built against existing RenderDoc1.46 headers/library. It validates counter
availability/type/unit, excludes parent actions and rejects invalid/duplicate
samples. Actual capture replay passed; leaf duration sum0.64144ms is only
this presentation capture, not a gameplay frame time or evidence of60FPS.
The next FBO investigation needs a capture containing scene rendering and
host-render-target-to-EDRAM resolve. Accurate deferred readback remains the
other priority. Restored FSI/full without RenderDoc, PID73719 at launch;
new gameplay/image acceptance is pending. Profiling retains the90s gate.

### Confirmed FSI/full gameplay sample, 2026-10-26

User confirmed black screen resolved and gameplay entered after restoring FSI/full.
Session `20261025T220743666736Z`, PID73719, same runtime7b1f69a0/GPUd2404533,
no RenderDoc. Read-only CPU/GPU sampling:60 rows at game age137.156–196.156s;
3103 CPU IP samples, zero lost/unavailable. GPU utilization mean68.12%
(range57–72); GPU Commands CPU37.36% of one core, DrawThread21.37%,
Main13.39%. Late pacing windows14.78–15.34FPS (~65–68ms/frame).
The largest CPU sample group on GPU Commands remains the previously identified
libc memcpy loop; this does not by itself measure GPU wait time. Full readback
serialization remains the leading optimization target from earlier stage timers.
The cap60 is not binding. No new FPS fix is claimed, and sampled scenes are not
identical-input A/B comparisons. Game left running.
[Summary](../logs/acv-fsi-full-confirmed-20261026/summary.json),
[symbols](../logs/acv-fsi-full-confirmed-20261026/ips/summary.json).

### General deferred readback scheduling core, 2026-10-26

Implemented [DeferredReadbackQueue](../tools/native-rexglue/deferred_readback_queue.h)
and [host regression](../tools/native-rexglue/deferred_readback_queue_test.cpp).
GPU-worker ownership; submitted monotonic tickets; retained staging callback;
bounded64 jobs/64MiB by default. Enqueue does not normally wait or copy.
CPU access flushes the prefix through the last overlapping job, waiting once
on its newest ticket. Prefix publication preserves order for partial overlaps.
Slot retirement MUST precede recording another GPU copy; Enqueue rejects a
still-owned slot. Capacity flushes preserve data. Failure poisons the queue;
no later CPU access may succeed until the owner handles the device failure.

Actual C++ ASan/UBSan regression passed: deferred bytes, unrelated CPU access,
partial overlap ordering, CPU write ordering, slot reuse, capacity, malformed
ranges, ticket regression, wrong worker, failed wait/publication, callback
exception and reentrancy. [Checkpoint](../logs/acv-deferred-readback-core-20261026/checkpoint.json).
This is a host scheduling core, not integrated runtime acceleration; no FPS gain claimed.

Integration requirements: SDK xmemory.cpp EnableAccessCallbacks currently rejects
providers; TriggerCallbacks asserts writes and rejects reads. Implement read
protection and alias-aware provider generations, release/reacquire the global
memory lock around waits, then revalidate generation/protection before unprotecting.
Resolve and staging ownership must be connected on the GPU worker; CPU consumers
must not deadlock when the GPU worker waits on guest work. Host physical accesses
bypassing faults need explicit synchronization. Before free/protection changes,
drain affected jobs before allocator reuse; teardown must drain normally or stop
all GPU/CPU users before cancellation. Full readback remains enabled until these
boundaries and current-generation CPU/GPU bytes are tested. Diagnostics retain90s gate.

### Readback worker dispatch and memory integration analysis, 2026-10-26

Implemented SDK patches [dispatch locking](../tools/native-rexglue/patches/gpu-worker-dispatch-lock.patch)
and [readback service during guest waits](../tools/native-rexglue/patches/gpu-readback-service-during-wait.patch).
The previous std::queue was accessed by multiple threads without synchronization.
All queue accesses now hold a mutex; callbacks execute outside it and enqueues
wake the worker. A separate restricted queue services already-recorded readback
completion during WAIT_REG_MEM, preventing a CPU-readback/guest-WAIT dependency
cycle. Ordinary host callbacks remain outside the guest wait. The private leased
experiment now uses this service; general memory read interception is still absent.

[Host test](../tools/native-rexglue/test_gpu_worker_dispatch.py) compiles actual SDK
methods and the actual WAIT_REG_MEM implementation with host dependencies:
32000 normal +32000 nested +32000 restricted requests, exactly-once owner-thread
execution, per-producer normal FIFO, callback recursion, guest-wait progress,
ordinary callback isolation and short-wait shutdown pass ASan/UBSan. Private
lease wait/publication guards and the general queue regression also pass.
Both patches reproduce final SDK sources with zero fuzz. New state is appended
at the end of CommandProcessor to preserve existing inline getter member offsets.
Final candidate `tmp/acv-readback-service-fix/`, GPU48994593, runtime7b1f69a0,
ELF08045fae, FFX0933159a; built, host-tested, final service not runtime-tested.
[Manifest/checkpoint](../logs/acv-readback-dispatch-20261026/checkpoint.json).

Earlier mutex-only GPU013217a0 ran session20261026T031256047692Z for180s,
child0/fatal0. It used an intermediate member layout and is superseded, never
accepted. SDK build overlapped the run, gameplay was not newly user-confirmed;
this is not a clean FPS comparison. Post90s inclusive timers: full wait60.79%,
copy13.70%, outer resolve75.19% (nested, do not add outer and inner values).
Final candidate and accepted package have not been substituted for one another.
No FPS improvement is claimed.

Source analysis identifies the remaining integration boundaries:

- MMIOHandler already forwards read faults under the global memory lock; physical
  page faults do not require MMIO instruction decoding. PhysicalHeap rejects read
  callbacks and data providers, so a queue alone cannot intercept CPU consumers.
- Host page reconciliation preserves only write watches. Provider NoAccess must
  survive allocation/protection changes, including changes to neighbouring pages.
- XFile writes file-read data through TranslatePhysical, then triggers callbacks.
  Synchronize/cancel pending GPU ownership before that write, otherwise a late GPU
  publication can overwrite newly read file bytes. Partial writes need exact range
  handling or prior publication; discarding a whole overlapping job loses other bytes.
- PhysicalHeap Release/Decommit mutate the parent before callbacks. Pending data
  must retire or cancel its old allocation generation before this mutation, with
  revalidation after any unlocked wait, to prevent publication into reused memory.
- Write-fault stale-protection recovery may restore ReadWrite. Provider failure
  must remain distinguishable from an ordinary unwatched fault and must not enter
  this recovery branch. Reads/writes via direct physical pointers need explicit
  synchronization; audio and file paths are examples, not a complete consumer audit.
- The private converter hook covers caller0x82C8391C only. It does not establish
  general CPU coherence. Protection of all A/C/E aliases, host access handling,
  staging lifetime and teardown still need implementation and regression tests.

[Source hashes and findings](../logs/acv-readback-dispatch-20261026/memory-analysis.json).
Implementation order: versioned ownership; alias read protection; wait outside the
lock and generation-checked publication under it; pre-write/free host paths; Vulkan
connection; exact current-generation bytes, then identical-scene measurements after90s.

### SDK physical read providers and allocation ownership, 2026-10-26

This supersedes the earlier statement that CPU read providers are entirely absent.
The compiled SDK now has an explicit provider registry and read/write fault path;
there is still no production Vulkan deferred producer. Full/FSI remains active.

- Registry waits outside the SDK global critical region, then publishes once under
  that lock. Concurrent CPU faults retain shared completion ownership and recheck
  providers registered while waiting. Completion/publication failure stops access
  instead of entering the old stale-protection recovery path.
- A/C/E alias page watches survive independently until each alias is accessed.
  Read-only guest mappings retain guest protection; private physical publication
  mapping is writable when a provider is registered. This fixes the actual SIGSEGV
  found when a deferred publication followed guest Protect(read-only).
- Physical alias allocation operations hold the global lock before heap locks.
  Free/decommit publish before mutating parent metadata; fixed commit and Protect
  retire old read watches before replacing host protection. Per-4KB epochs reject
  a suspended release when another thread freed and reused that physical address.
- Existing public TriggerCallbacks ABI is retained; allocator paths borrow the
  same lock through TriggerCallbacksLocked. New Memory members are appended.
- Actual compiled SDK regression exercised POSIX faults, all three aliases, an
  unlocked GPU completion dependency, partial writes, write invalidations,
  read-only/fixed-commit rearming, pre-decommit publication and deterministic reuse
  during an outstanding release. PASS operation 1c234271-f9b6-4348-b419-7ac9bed6c9aa.
  Fixed guest mappings conflict with ASan shadow, so this mapping test is native;
  registry/lease host tests and compiled explicit bridge run with ASan/UBSan.
  Latest bridge PASS: 7fd622de-d13d-49ba-abba-80b097659031.
- Reproducible patch: tools/native-rexglue/patches/physical-memory-readback-providers.patch;
  zero-fuzz application reproduces all three SDK files byte-for-byte. Build outputs
  and earlier read-watch-only checkpoint are retained with hashes in evidence.

Candidate tmp/acv-memory-provider-fix/: runtime a9125a31, GPU48994593,
ELF08045fae, FFX0933159a. Rebuilt SDK FFX is deliberately excluded from this
candidate. A75s X11/full/FSI startup smoke completed: session20261026T164337008101Z,
child0/fatal0/runtime_errors0. This is not gameplay or FPS acceptance; the90s
measurement gate was never reached. Live mappings of all four artifacts matched
its manifest. The process ended at the requested bound and is no longer running.

Remaining integration boundaries: direct host physical IO/audio, direct parent-heap
mutations, Memory reset/save/restore, Vulkan staging resources/queue integration and
teardown while the device is still valid. Registration and alias arming must occur
under the same global lock with allocator ownership held. No general mode may be
enabled until those consumers and lifetimes are covered. No60FPS claim.
[Evidence and hashes](../logs/acv-memory-provider-bridge-20261026/checkpoint.json).

### Physical host file IO reservation, 2026-10-26

Physical XFile::ReadInternal now synchronizes providers before writing and reserves
its range while disk IO runs without the global mutex. Overlapping registration
and allocator synchronization wait for the reservation. Release and post-write
invalidation are atomic under that mutex, including partial errors and exceptions.
No temporary texture buffer/copy was introduced; virtual file reads retain their path.

Actual ReadInternal source (class/completion names replaced in a host harness) was
compiled against SDK Memory and a deterministic fake disk. It passed old-GPU-data
publication before a4-byte partial write, blocked overlapping registration, unlocked
IO, preserved neighboring bytes and invalidation after partial failure/exception.
PASS73558f8d; real alias/allocator regression PASS217eda0d; registry ASan/UBSan
PASS8a5db4e2. New runtime built and preserved; not runtime-tested or promoted.

XMA needs atomic multi-range reservations before its raw input/output pointers are
used. The general Vulkan producer remains disabled. Direct GetPhysicalHeap() has
no callers in current SDK/native source search; reset/save/restore and lifetime
integration still need coverage.
[Sources, incremental patch and hashes](../logs/acv-host-physical-io-20261026/checkpoint.json).

### XMA direct physical access reservation, 2026-10-26

XmaContext::Work now pins both valid input buffers, output and physical context
metadata as one reservation. All GPU waits finish before any partial reservation
is retained. This prevents a GPU-worker/decoder dependency cycle. Actual decoding
runs with the global mutex released; the RAII exit notifies output writes across
A/C/E before releasing ownership. Owner-thread context page faults may synchronize
inside their own pin; allocator mutation and new GPU registration cannot do so.
Host access also validates committed parent pages and restores the private mapping
for hardware writes while preserving guest alias permissions.

Actual Work/RAII source, with codec/merge operations stubbed, passed against
compiled SDK Memory: both inputs/output/context pinned, an earlier-range provider
registered during a later-range wait, decoder without global lock, owner context
write fault and output ordering. Operation442a3909. This does not verify codec
quality. Actual file regression9bab55b1 and alias/allocator regressionda74b449
also passed. Atomic multi-range host regression passed ASan/UBSan.

Candidate tmp/acv-host-access-fix/ runtime5b126405/GPU48994593/FFX0933159a
passed75s X11/full/FSI session20261026T171406346150Z, child0/fatal0/runtime_errors0.
The bounded process ended; no after90s measurement or gameplay acceptance.

Consumer search found only XFile and XMA direct TranslatePhysical users in current
system/audio/kernel/input/filesystem/native shim sources. Graphics CPU consumers
still need their own integration. MapViews additionally mirrors physical0..16MiB
at7F000000; LookupHeap returns null there, so A/C/E tests do not prove coherence
of that mirror. The deferred GPU producer must exclude this range or implement
its protection. No current GetPhysicalHeap caller was found in SDK/native sources.
Reset/save/restore remain unverified; the general producer is still disabled.
[Patch/source/test/runtime hashes](../logs/acv-xma-physical-access-20261026/checkpoint.json).

### Graphics CPU physical coherence, 2026-10-26

Graphics CPU words and snapshots now synchronize physical providers before
reading or modifying memory: packets, constants/shaders, resolve vertices,
occlusion results and host publications. A packet is captured individually,
including ring wrap; scoped provenance retains the guest address for inline
shaders without changing base processor layout. GPU upload preflight runs before
submission-ticket/resource capture, then reopens submission after memory waits.
Preparation borrows one memory lock and waits only for host CPU reservations;
old GPU data is awaited only by actual CPU reads. Exact GPU publication avoids
invalidating already-current GPU caches. Published providers retire without a
GPU wait; bounded queue admission precedes recording into a staging slot.

Compiled SDK graphics/packet test4c514642 and XMA testbf5b9a75 PASS; provider
registry, retirement, queue and restricted WAIT dispatch pass host ASan/UBSan.
Candidate tmp/acv-graphics-cpu-fix/ runtime131b829f/GPUf2f3ee56 with accepted
FFX0933159a passed75s X11/full/FSI session20261026T181420117779Z,
child0/fatal0/runtime_errors0. It ended before the90s diagnostic gate, so it
provides startup evidence only, no gameplay image/FPS acceptance.

General Vulkan producer integration follows separately. The unprotected7F mirror
excludes physical0..16MiB; bulk state APIs remain unverified. No current source
caller of Memory Reset/Save/Restore was found; this differs from guest save-file
IO, which uses XFile. The accepted package is unchanged.
[Sources, patch, tests and runtime hashes](../logs/acv-graphics-cpu-coherence-20261026/checkpoint.json).

### General Vulkan readback producer, experimental integration, 2026-10-26

ACV_GENERAL_DEFERRED_READBACK=1 enables a separate producer for full/native
(unscaled) FSI resolves. Physical0..16MiB is excluded because7F000000 is not
covered by A/C/E watches. Other modes retain their previous path. The accepted
standalone package and canonical config are unchanged.

ResolveInfo is captured once. Pool admission/slot retirement precedes memory
locking and command recording. A prepared memory scope waits for host IO,
refreshes destination uploads before descriptors and registers/arms A/C/E
read/write watches before GPU dispatch. It holds allocation ownership through
recording, then unlocks before submission. Only successfully submitted tickets
enter the ordered bridge. Publication waits for one newest prefix fence,
invalidates mapped staging and copies exact current-generation bytes under the
memory mutex; it does not recursively use a CPU-write helper or invalidate
already-current GPU caches. Providers retain plain state only; the GPU worker
owns Vulkan allocations (64 slots/64MiB limit) and reuses them after publication.

Host IO reservations now cover host pages, merged across ranges, because faults
and invalidation callbacks are page granular. This blocks a neighboring provider
from creating an IO/GPU dependency cycle. Original requested ranges still own
committed-page validation, avoiding rejection of unrequested neighboring pages.

Normal shutdown drains publications, closes/wakes pending CPU requests, then
always completes all submissions before destroying staging. This includes a
recorded/submitted copy whose EndSubmission/Commit failed before enqueue. A
non-lost device that cannot complete stops with an error instead of freeing live
resources. Late callbacks retain no raw destroyed owner or Vulkan allocation.

Real Vulkan/compiled SDK oracle on GTX1660 SUPER passed11 copies, overlapping
generations,8 actual guest faults, repeated slot reuse and partial CPU writes
(dc47d06a). Actual teardown source passed ASan/UBSan8 drain/fence/device-loss
cases (0eb49083). Graphics/page reservation9cfea5e2, XMAefe6f313, file IO
5b15633c and alias/allocator1e97b0ba PASS. Extended stage metrics retain the
90s gate; actual C++ test a73e86e4 creates no diagnostic file before90s.

First candidate runtimeb0bf9b46/GPU0270e382/accepted FFX0933159a ran180s
X11/full/FSI, session20261026T183953558552Z: child0/fatal0/runtime_errors0,
ended. Eighteen complete diagnostic windows begin at/after90s:52709 general
resolves,52700 publications and3217 prefix fence waits; legacy full-wait count0.
CPU wall timers are inclusive: draw46.959% includes general_resolve32.469%;
within that, general_wait14.987%, general_publish12.634%, general_submit3.710%.
These percentages cannot be added. Cadence and this uncontrolled scene do not
prove higher gameplay FPS or image correctness. No such gain is claimed.

Latest source includes the failed-submit lifetime fix, built as isolated
tmp/acv-general-readback-teardown-fix/ runtimeb0bf9b46/GPU06400c82. A second
CPU/GPU profile starts after90s to select the next optimization. Current-generation
byte preservation is host/GPU-tested; image/game-time, representative mission
transitions, save/load and stable60FPS still require acceptance.
[Patch, source hashes, tests and runtime records](../logs/acv-general-vulkan-readback-20261026/checkpoint.json).

### Coherent CPU read fast path, 2026-10-26 UTC / 2026-10-27 local

The second general-producer run20261026T184646257218Z completed180s
child0/fatal0/runtime_errors0. Its35s CPU profile began at process age90.001s,
2608 IPs lost0. GPU Commands consumed81.21% of a core; explicit reservation
setup, provider synchronization and allocator functions were among its hottest
user instructions. A same-input host test confirmed6 allocations per normal
4-byte coherent read, median107.546ns across3x200000 reads.

ReadPhysicalCpu now checks for IO/provider hazards while holding the global
memory mutex. If none overlap, it validates committed parent pages/private
mapping and copies without releasing that lock; no long-lived token or generation
snapshot is needed. Any conflict retains the original pinned synchronization path.
Mapping validation is shared with BeginPhysicalCpuAccess. Owned IO pins remain
rejected; concurrent IO blocks reads; holes remain rejected without touching them.

Same-input host test now asserts zero allocations/read, identical checksum and
median22.275ns (4.83x for this boundary only). Actual SDK graphics test covers
IO pin/owner rejection, decommitted pages, readonly hardware mapping and wrapped
packet/inline-shader provenance. It and XMA, file IO, A/C/E allocator/ABA and
real Vulkan11-copy/8-fault byte regressions PASS. No KHRONOS validation layer
is installed locally; no system changes were made.

Isolated tmp/acv-physical-read-fastpath-fix/ runtimee12818a0/GPU06400c82
with accepted FFX0933159a passed180s X11/full/FSI20261026T185840508042Z
child0/fatal0/runtime_errors0, ended. A35s >=90s profile collected2468 IPs lost0.
Observed GPU-worker CPU66.47% versus prior81.21%, with different scenes;
this is not an identical-input FPS comparison or visual/game-time acceptance.
Most sampled new SDK read instructions iterate the pending-provider list
(runtime offsets4396ec/4396ef/4396fe). Next add a safe range rejection and
identify whether remaining fence waits arise from CPU demand or staging pressure.
[Patch, hashes, byte regressions and profiles](../logs/acv-physical-read-fastpath-20261026/checkpoint.json).

### Provider range rejection — source/host checkpoint, 2026-10-27 local

Sampled runtime instructions4396ec/4396ef/4396fe traverse pending-provider
list nodes for non-overlapping command reads. A conservative min/max envelope
now rejects ranges outside all providers before exact scans; ranges within it
still scan normally. Registration widens bounds, publication/collection and
cancellation recompute them under the same memory mutex. CPU IO reservations
and failed/publishing-state checks precede the rejection.

Independent interval oracle passed4000 register/collect/prefix-sync/pin
operations and >64000 queries including512MiB boundary under ASan/UBSan.
Existing registry concurrent wait/new-generation/failure, retirement/admission
and bridge late-callback/lifetime suites PASS. Bridge diagnostic owner flag
classifies CPU-demand waits separately from pool-pressure waits; timers still
create no output before90s (actual C++ gate test PASS).

Same-input baseline on e12818a0 with64 non-overlapping pending providers:
89.258/99.6524/87.221ns per word read, zero allocations, unchanged checksum.
This is a host benchmark, not gameplay FPS. New benchmark remains pending.
The private provider-registry tail grew; full matched SDK runtime/GPU rebuild
is required before use. Existing original Memory prefix fields were not edited;
accepted-executable inline/member-offset compatibility still needs checking.

Build completed (dbd63bf7); matched runtimeebf338a1/GPU14154063 was preserved
in tmp/acv-provider-envelope-fix/ with accepted FFX0933159a. SDK graphics,
IO/XMA/alias/ABA and real Vulkan11-copy/8-fault regressions PASS. Accepted ELF
and original Memory prefix declarations are unchanged; private-tail changes
require the rebuilt pair. Source/symbol review is supplementary and does not by
itself rule out every fully inlined access; the unchanged executable was tested
with the matched runtime/GPU. Protected original save/config hashes match.

Runtime20261026T191748089182Z completed180s X11/full/native FSI,
child0/fatal0/runtime_errors0; PID136294 ended. No live game or build remains.
After90s:18 diagnostic windows,68427 general resolves,68436 publication
observations (jobs may cross window boundaries),4019 bridge waits. All classified
waits are pool-related (31.602% inclusive wall), CPU-demand waits0. Publication
is15.980%; nested percentages are not additive.2389 user IP samples lost0.
No identical-scene gameplay FPS, image or game-time acceptance is established.

The next optimization is oldest-prefix admission: retire only enough oldest
pending jobs to admit the next copy instead of draining the entire queue. It is
not implemented yet. Retain exact current-generation bytes, slot/fence ownership,
capacity limits and fail-closed publication; validate with queue/admission/failure,
SDK and actual GPU regressions before another >=90s profile.
[Source hashes, patch replay and exact resume steps](../logs/acv-provider-range-rejection-20261026/checkpoint.json).

### Projection census and the UltraWide experiment extended, 2026-10-27

Census (`ACV_PROJECTION_CENSUS=1`, `scene_aspect_experiment.cpp`; read-only; per call site: count and argument ranges;
return address of the call, which the recompiled code sets before every call). Sessions
`20261026T193756597623Z` (110 s) and `20261026T194121365223Z` (150 s), scripted keys (`--acv_probe_keys`), throwaway
profile `tmp/acv-census-user`, heavy scene 15-19 FPS:

| Helper | Call site (LR) | Calls | Arguments |
|---|---|---|---|
| perspective `0x82BC8980` | `0x822B4510` | 3981 / 8781 | FOV 0.51-1.05 rad, aspect 16:9, near 1, far 13000 |
| perspective | `0x82300978` (generic builder `0x82300948`) | 29049 / 61366 | FOV 0.51-1.05, aspect 0.889-2.0 (16:9 among them), near 1-60, far 11-13000 |
| orthographic `0x82BC8CC0` | `0x822B5368` | 14478 | pixel space 0..1280 x -720..0: the HUD overlay |
| orthographic | `0x822DF238` | 25312 / 55632 | 0..640/1280 x 0..360/720, near 0.1, far 1000: full/half-size passes |
| orthographic | `0x82B1ABCC` | 2652 / 5839 | normalised +-0.1 x +-1: small 3D/UI element |

Sites `0x82437C60`, `0x82B21E70`, `0x822DF058`, `0x825173B8`, `0x82300780`, `0x822DF238`'s perspective callers and `0x822B52D8`
were not called in these scenes. Ghidra xrefs know only one caller per helper (fragmented function bodies), so the
caller list comes from the recompiled sources. `0x82BC8CC0` is orthographic (left, right, bottom, top, near, far in
`f1..f6`), `0x82BC8980` perspective.

Experiment (all default-off, `scene_aspect_experiment.cpp`):
- `ACV_EXPERIMENTAL_SCENE_ASPECT=<1..4>`: widens the aspect of every perspective call from `0x822B4510` or `0x82300978`
  whose original aspect is 16:9; other aspects (0.889, 2.0) and other sites are untouched.
- `ACV_EXPERIMENTAL_HUD_ASPECT=<1..4>`: only the exact HUD ortho (`0x822B5368`, 0..1280 x -720..0) gets left/right widened
  symmetrically by `aspect/(16/9)`, so the HUD keeps its proportions and sits in the centre 16:9 part of the stretched
  render target. Full-screen post passes (other sites) are unchanged.
- Used with `--present_letterbox=false` (render target stretched over 2560x1080, output rect x=0 y=0 2560x1080).

Run of `20261026T194121365223Z` (host `03cbc5454f3ae6c1`, build in `tmp/acv-ultrawide-test2/`): experiment active, scene
and HUD hooks logged, output 2560x1080 without bars. The user reviewed the picture and accepted it ("сойдёт") as an
experimental mode. Not verified: culling at the screen edges, the reticle position, cut-scenes/menus, and other missions.

### `wide_view` setting (experimental), 2026-10-27

- `acv_settings.py`: new graphics key `wide_view` (`off` default, `auto`), status experimental. `auto` derives the aspect from
  the chosen `resolution` (it needs an explicit WxH; `auto` resolution and outputs not wider than 16:9 are a no-op, above 4:1
  is rejected), adds `--present_letterbox=false` and, through the new `graphics_to_env()`, sets
  `ACV_EXPERIMENTAL_SCENE_ASPECT` and `ACV_EXPERIMENTAL_HUD_ASPECT` for the game process (`probe_linux.py` merges it into the
  environment and records `settings_env` in `launch.json`).
- Availability: `runtime_features()` reports `wide_view` when the executable in the build directory contains both variable
  names (memory-mapped search). The accepted ELF now contains the hooks (default-off, pass-through): hash
  `03cbc5454f3ae6c1...`, recorded in `RUNTIME_BASELINE.json` (`history` keeps the previous ELF hash); the earlier ELF is in
  `tmp/acv-backups/pre-wideview/`. Python tests added (mapping, 16:9 no-op, auto resolution, missing hooks, experimental flag).
- Check: `logs/acv-native-linux/20261026T194525066353Z` (scripted keys, throwaway profile, settings only, no manual
  environment): hooks logged, output rect 2560x1080 at x=0 y=0.
- The TRG frontend does not list the new key; set it in the settings JSON until a widget is added.

### `wide_view` for outputs narrower than 16:9 (16:10), 2026-10-27

- Geometry: the 1280x720 render target is stretched over the output, so only the projection aspect against the SCREEN
  matters. Wider than 16:9 the hook keeps the vertical FOV and widens x. Narrower (16:10, 4:3) it keeps the horizontal
  view and raises the vertical FOV: `tan(fov'/2) = tan(fov/2) * (16/9) / aspect`, `f2 = aspect`; the HUD ortho
  (`0x822B5368`) is extended top/bottom by `720*((16/9)/aspect - 1)/2` instead of left/right. `acv_settings._wide_aspect`
  accepts 1.2 to 4.0 (16:9 stays a no-op).
- Check `logs/acv-native-linux/20261026T195018601039Z` (scripted keys, settings only, windowed 1728x1080): hooks logged
  with `new=1.6`. Host ELF `6cd54abf0b011e5f...` (baseline updated). The user watched the run and reported that it works.
- Limit: in a window the client area is smaller than the requested size (swapchain 1728x1038, aspect 1.665 against the
  1.6 used by the hook), so geometry is exact only in fullscreen/borderless. Culling at the edges, the reticle and
  cut-scenes were not examined separately.

### Minimal staging admission prefix, 2026-10-27

PrepareForEnqueue/Enqueue now wait/publish only the smallest oldest prefix that
allows the new job under both byte and job-count limits. Slot IDs distinguish
jobs even when submission tickets are equal. CPU overlap synchronization,
explicit drain and pre-record slot retirement retain their prior ordered rules.
No budget increase; failed waits retain unpublished jobs/resources.

Host queue/retirement/bridge suites passed ASan/UBSan (81479610). Added tests
cover combined byte pressure, count pressure, equal-ticket copies and capacity
failure retention. Actual Vulkan/SDK test cee1fd4d passes12 copies including
partial capacity publication (later jobs stay pending),8 guest faults, current
overlap bytes, staging reuse, partial CPU writes and normal drain. Build814df842.
Zero-fuzz patch replay restores the exact SDK header.

The portable benchmark reconstructs the old baseline via the durable reverse
patch instead of depending on ignored logs. On GTX1660 SUPER, alternating3
trials of identical512x4MiB copies give median296.900→228.750ms, throughput
1.298x, median GPU-fence wait224.099→148.191ms. This is synthetic transfer
pressure, not game frames/FPS. Final complete buffers and per-copy samples are
validated; both policies use the same16-slot/64MiB pool.

Candidate tmp/acv-minimal-admission-fix/ runtimeebf338a1/GPUc558cb37/
accepted FFX0933159a ran180s X11/full/native FSI20261027T043019725818Z,
child0/fatal0/runtime_errors0, ended. Full log confirms GTX1660 SUPER selected;
llvmpipe was merely enumerated. The run stayed in menu (17 >=90s windows);
its60Hz cadence cannot validate mission FPS. Representative mission choice was
requested from the user.

Next independently retire an oldest fitting occupied slot when pool allocation
pressure would otherwise drain every pending copy. That policy is not yet
implemented. Maintain slot/fence ownership and fail-closed reuse.
[Patch, runtime records and reproducible benchmark](../logs/acv-readback-minimal-admission-20261026/checkpoint.json).

### Oldest fitting staging slot and complete progress save, 2026-10-27

Allocation pressure now retires the fitting occupied slot with the oldest known
submission before falling back to draining/reclaiming differently sized buffers.
The queue must publish it successfully and clear occupied; missing jobs, unknown
IDs/tickets, failed waits and exceptions do not authorize reuse. Slots retain
submission metadata; the callback cannot remove/destroy the pool allocation.
Full drain remains for genuine size growth. No budget increase or Memory ABI change.

Selector/teardown ASan/UBSan04e3f279, actual Vulkan/SDK12-copy/8-fault test
a2f09d07 and extended90s gate0fac27ff PASS. Build4cf700f5 and zero-fuzz patch
preserved. Mixed-size real-GPU benchmark uses the same minimal-prefix queue
and fixed16-slot40MiB pool for both policies:512 alternating1/4MiB requests,
median183.850→143.635ms, throughput1.280x, allocator full drains32→0.
This synthetic allocator result is separate from the earlier1.298x admission
benchmark; neither is a game FPS measurement and they must not be multiplied.

Latest isolated candidate tmp/acv-fitting-slot-fix/ uses runtimeebf338a1,
GPUf1ccdfb9, host08045fae and preserved FFX0933159a. It has not been launched
in the game yet. Two identical offline profile clones were prepared under
writer lock with a verified source backup; saved SDL input sequences were found
in the existing projection-census sessions. Control/candidate comparison remains
unstarted; sampling is planned after120s. No live-copy or protected-save/config
modification was performed. The separate accepted WideView package is preserved.
[Hashes, tests, benchmark](../logs/acv-readback-fitting-slot-20261027/checkpoint.json),
[complete Russian progress and remaining work](FPS_READBACK_PROGRESS_20261027.md).


## Focused readback runtime comparison, 2026-10-27

Runtime-сравнение завершено: кандидат20261027T052317764850Z и контроль с фокусом20261027T052659721550Z, по210с child0/fatal0. Ожидание пула976.869→33.794мс за поздние35с; cadence≈60 в обоих, прирост FPS миссии не доказан. Main XThread44.79→95.29% ядра; polling82BAF218/82BAE308 требует проверки. Фактический fullscreen1920×1080; миссия/темп/картинка не подтверждены. Все процессы завершены; пакет не заменён.

[Detailed results and remaining work](FPS_READBACK_PROGRESS_20261027.md).


- 2026-10-27: Параллельная копия принята пользователем; host/GPU/ASan и два210с запуска child0/fatal0. Worker16.79% ядра. Gameplay-окна различались: прирост FPS не доказан. Перенос в отдельный пакет открыт. [Детали](CPU_PARALLEL_READBACK_20261027.md).


## Selected integration and FOV/HUD refinement — 2026-10-27

Current separate package: ACV-Linux-TRG-FOVHUD-20261027 (`../../ACV-Linux-TRG-FOVHUD-20261027/start.sh`, исторический пакет; каталог сейчас отсутствует),
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

Current user scope: package + FOV/HUD, integrated CAS/FSR without tests, existing
colour capability limits. FPS benchmarking, save/mission regression,
controller/HiDPI work and other-resolution tests are skipped by user instruction.
Protected config/save hashes unchanged; all owned game/build/test runs ended.


### Launcher memory policy and archive preloading — 2026-10-27

Bounded Linux archive read-ahead and configurable texture retention are built,
actual-source ASan/UBSan tested and published as a separate candidate. Both210s
scripted runs exit child0/fatal0; no cache evictions under either policy. This does
not establish a fix for model/location unloads. Read-only retail82586050 confirms
conditional cleanup calls; ownership semantics unchanged.
[Technical result, hashes, limits and next step](RESOURCE_STREAMING_20261027.md).


### Local frustum and UltraWide sphere rejection — 2026-10-28

Retail82302520 builds local side planes before the hooked projection;82300CE0
consumes them. The camera-build override now follows the actual widened
projection through a scoped thread-local marker and updates only4normal fields.
Actual translated sphere tests/SDK context pass ASan/UBSan; build passes.
[Evidence, runtime verification and limits](ENGINE_OFFSCREEN_20261028.md).
