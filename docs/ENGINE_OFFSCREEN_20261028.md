# Engine off-screen visibility — 2026-10-28

Request: reduce abrupt disappearance/unloading of textures/models outside the
screen at the game-engine layer. Keep CAS/FSR effects untested and use only the
2560×1080 runtime target. Release1.0 and personal profiles/settings are preserved.

## Confirmed defect and change

Retail82302520 constructs a perspective camera. It writes normalized local
left/right plane normals at camera+0x60/+0x70 from the original FOV and1280/720
aspect **before** calling82300948→82BC8980. The existing UltraWide hook changes
that projection to2560/1080. Retail82300CE0 later tests spheres against the
local plane array, so it can reject points that are inside the widened image.
The separately derived world planes use the projection; changing those alone
would leave the local rejection in place.

[Raw builder capture](../decompilation/evidence/acv/82302520-20261028T010652.093791Z/report.json),
[raw sphere-test capture](../decompilation/evidence/acv/82300CE0-20261028T010747.112364Z/report.json).
Ghidra has VMX128 gaps/truncated functions: raw bytes and the corresponding
translated instruction sequence were inspected. No live DB repair or auto-analysis.

scene_aspect_experiment.cpp now overrides82302520 and calls the original exactly
once. A thread-local RAII scope records whether the existing, selected82300978
projection was actually widened during that camera build. Only then, and only
for native16:9 input, wider output and validated symmetric normalized planes,
four horizontal normal coefficients are rescaled/renormalized. No other object
bytes change. Near/far/vertical planes, viewport dimensions, FOV, LOD settings,
matrices, return registers, saves and resource ownership remain original.
This applies an existing expanded view consistently; it does not draw every
object behind the camera or suppress resource destructors.

The first implementation limited outer callers to two Ghidra references.
The180s trial exposed the rendering path returning to82303164, missing from
that incomplete xref list. That implementation was not published. The final
implementation follows the widened projection itself and restores nested scope
on normal return/exception; [direct callers](../logs/acv-engine-offscreen-20261028/direct-callers.json)
show13 translated call sites. ACV_NATIVE_FRUSTUM_SYNC=0 provides a diagnostic
opt-out; the fix is otherwise active with an eligible wide_view camera.
ACV_FRUSTUM_TRACE=1 emits bounded counters only after90s, every10s per thread.

## Verification

Actual-source ASan/UBSan test_native_frustum.py compiles the new hook with the
real SDK PPCContext and the translated retail sphere test82300CE0/helper823009C8.
Read-only captured float/vector constants are used. The original camera-builder
side is a shaped fixture, so the host test is not full retail execution of that
builder. It checks points in both newly visible side bands, ordinary visible and
outside points, sphere radius, near/far/vertical rejection, normalized planes,
unchanged bytes/register outputs, foreign projection paths and exceptions.
Target aspect is2560/1080; no other game resolution or CAS/FSR effect is tested.
The existing FOV/HUD ASan/UBSan regression also passes.

Incremental host build passes; codegen writes0files. Candidate keeps the tested
streaming runtime544bdaae, GPUfe4550d5 and FFX0933159a. Final host ELFefa4756c.
[Full hashes, operations and current runtime status](../logs/acv-engine-offscreen-20261028/checkpoint.json).

First180s exploratory run20261028T011815872120Z exits child0/fatal0. It confirms
actual local-plane modification in one camera path and exposed the missed
rendering caller above. Its memory samples and trace describe that earlier
implementation only. Final160s run20261028T012252054423Z uses a fresh isolated copy of the same
profile, X11/fullscreen2560×1080, bilinear/noFXAA/no dither, readbackFSI/full,
wide_view=auto, and scripted camera keys after110s. It exits child0/fatal0/runtime_errors0. Late counters confirm eligible=patched
on both main/rendering paths (49467 and18449 at the last report); counters
start after90s. The user confirmed: «Да, помогло». No FPS improvement or visual acceptance is inferred
from a successful runtime/host test.

## Remaining scope

This is a visibility-frustum fix. Actual cache lifetime, decompression and
model/texture unload scheduling are separate. Earlier bounded GPU retention and
archive page-cache preloading remain integrated, but their tests did not reproduce
premature texture eviction. Turning the camera away and back may still expose a
separate engine streaming issue; no blanket refcount/destructor suppression was
implemented. The symptom question (edge disappearance versus delayed return)
remains unanswered. Full mission/image/long-run memory acceptance is open.

[Source/document snapshots](../logs/acv-engine-offscreen-20261028/docs-before/sha256.json)
retain all previous active document bytes and checksums.


## Accepted release update

The user-confirmed ELFefa4756c is now installed in
[ACV-Linux-release-1.0](../../ACV-Linux-release-1.0/start.sh).
Version/folder remain1.0; patch_level2026-10-28-native-frustum identifies the
change. All186 manifest hashes/dependencies pass, and RUNTIME_BASELINE records
these exact artifacts and the limited acceptance scope. Previous executable and
metadata preserved in [verified backup](../logs/acv-engine-offscreen-20261028/release-before/sha256.json).
Source repair is handwritten; generated PPC was not edited. Existing graphics
settings and resource caches remain in the package. This acceptance addresses
visible edge disappearance; separate engine cache/unload semantics and stable
FPS remain outside the established result.

Updated release35s frontend check passes from cwd/: local libraries/environment,
GUI/game/lock lifetime and orderly shutdown; child0/fatal0. This startup check
requests windowed2560×1080 (work area2560×1044), bilinear, no effect tests.
