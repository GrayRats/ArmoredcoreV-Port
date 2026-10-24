# ACV timing — current findings and evidence

**[I]** instructions/bytes; **[T]** Xenia trace;
**[M]** guest memory; **[H]** limited host test; **[G]** hypothesis.
This is not a specification of Xenon and not proof of any effect on combat or physics.
Full earlier text: [archive](../logs/docs-context-20261022/originals.tar.gz),
entry `docs/ACV_TIMING.md`; [manifest and link index](../logs/docs-context-20261022/manifest.json).

[Русская версия](ACV_TIMING.RU.MD)

## Formula and expectation

**[I]** `0x82AEF208` (148 bytes) computes the elapsed value D from the observed uint32 F;
the caller clamps only from above, then scales:

```text
N = floor(F / 60)               // integer division
P = float(float(D) * 100) / float(N)
P = min(P, 600)
step = float(float(P * 0.01) * float(1 / 60))
```

The practical model is `min(D/F, 0.1)` seconds. This is an approximation, not an exact
identity: N is an integer and float rounding is preserved. Negative D is not clamped from
below. The guard F<60 in the C# utility is protection for the tool, not established retail
behaviour. NaN/overflow/rounding parity has not been accepted separately.
**[H]** 4,000,000 comparisons of C# arithmetic: max difference ≤1.34e-7, none above 1e-6;
this is not execution of PPC and not proof of a fixed FPS.

**[I]** Call path: entry `0x82B92114 → 0x82AEEF58`; repeated
`0x82AEF0AC → 0x82AF1BF0`; `0x82AEF0B4 → 0x82AEF208`.
`0x82B8F9C0` reads TBLr/mftb, stores a 64-bit value and returns 1.
`0x822F4E88 → 0x82B8FA70 → KeQueryPerformanceFrequency` initialises the
low32 frequency global `0x83886944` if it is zero.
`0x82AF1BF0` (328 bytes) calls preliminary routines, then waits while the normalised elapsed
value is <90; the wait calculation uses 95.
`0x82C27BE8` discards nonpositive input and passes `(input+999)/1000`;
`0x830F3288 → 0x82B93668` builds a negative relative interval from input*10000
and calls KeDelayExecutionThread. Sleep precision and the retail frequency are not established.

**[T]** `0x82AEF26C`: r7=r8=0x000CB735 (=N), so F in this Xenia run
is in [50,276,460, 50,276,519]. This is the frequency the game sees inside the emulator,
not a hardware Xenon specification. The D/f1 logger does not print r3–r8.
The rate and the step write were observed roughly once per frame; the physics rate is not proven.

Evidence: [timing-index](../decompilation/evidence/acv/timing-index.json),
[follow-up index](../decompilation/evidence/acv/timing-followup-index.json),
`logs/acv-stdout.log`, `logs/acv-consumers-stdout.log`, `tmp/acv-timing-check`.

## Inter-thread handoff: two P objects, not three

**[I,T]** The singleton getter `0x82AEEF40` is linked to the global `0x8372BF80`,
object `0x8372BF88`. `0x82AEFC58` sets selector +0x38=0;
`0x82AF1D38` fills +0x30/+0x34 with two results of `0x82AF5F50`.
`0x82AF12F0` XORs the selector with 1, selects a pointer at +0x30+4*selector,
stores it to +0x28, writes float +0x974, word +0x980=1 and +0x984=0.
With the known initialisation, +0x34 is selected first. Other later writers of the selector
are not fully excluded. The stack temporary `0x82AF2F70` / `0x82AF1B60` is a different object;
its selector does not describe the singleton.

**[T]** `Main XThread`, guest 01000028, tid6: `0x82AF1BF0` /
`0x82AF12F0`. `DrawThread`, guest F80013FC, tid0x18: executor
`0x822D0E40`, callback return LR=0x822D0F28. The passed P values alternate
`0x40586F00` / `0x405850D0` and match the callback contexts.
This confirms the handoff through the same two objects; the earlier "three buffers"
hypothesis is replaced. The possible ring `0x82AF5410` is not the same as this handoff.

**[M]** P+8=1280, P+12=720. The first sample of P+0x974 ≈ 0.0333 s;
later, 12 samples over 0.563 s: first P 0.016492827–0.016702265,
second 0.016626088–0.017455928 s. These are different windows, not a fixed 30/60 FPS.
The reads are asynchronous and not atomic. Historical Xenia mapping:
host address = 0x100000000 + guest VA; check against the maps of the specific process.
Do not confuse it with the virtual base of ReXGlue or with physical mappings.

**[M,I]** The slot `0x8202411C` was read 12 times: bytes 82 5d 41 00 →
`0x825D4100`. It takes the target from record+8, r3 from record+4,
moves r5→r4 and does mtctr/bctr. The slot itself is confirmed; a separate new
trace of the indirect call through it was not captured. The callback `0x82AEF6C8` is
`mr r3,r4; b 0x822B51F0`, not a jump into the middle of 0x822B5190;
`0x82AEF6D0` calls `0x822E5DE8`, `0x822E52D0`, `0x822B5B90`
with the saved input r4. No direct reads of +0x974 by them were found.

Evidence: [guest samples](../logs/timing-followup-20261019/guest-samples.json),
[summary](../logs/timing-followup-20261019/summary.json),
[maps](../logs/timing-followup-20261019/xenia-maps.txt),
[slot instructions](../logs/timing-followup-20261019/slot0.asm).

## Step reader and S modes

**[I]** `0x824609E0` calls `0x82460848` at `0x82460A54`.
If byte(S+0)==0, the consumer exits — the earlier inverted condition was an error.
`0x824608C8 → 0x82436FE0`: P=*(context+0x10), f1=*(float*)(P+0x974).
Then r3=S and `0x824608D0 → 0x8245FA20`; S and P play different roles.
The VMX128 preconditions must not be decoded from generic PWR7 mnemonics.

**[I]** `0x8245FA20` — 48 instructions / 192 bytes,
[0x8245FA20,0x8245FAE0), without a .pdata owner; the live bytes matched .text.

| Field | Observed operation | Limit of meaning |
|---|---|---|
| byte +0 | Zero → return; otherwise update | "Activity" is a descriptive name |
| byte +1 | Mode selection | Not a recovered enum |
| u16 +2 | Modes 1/3 increment, saturating at 65535; the mode-4 handler also writes it | Not a universal "period counter" |
| float +4 | Comparison / one-time subtraction | Duration/period — hypothesis |
| float +8 | +=f1; then mode-dependent changes | Time accumulator — hypothesis |

Modes 2/4: when +4 is reached, +0 is reset. Modes 1/3: +4 is subtracted once
and +2 is incremented; there is no repeated subtraction loop.
Other modes keep the addition; NaN does not pass the usual less-than branch.
In the consumer, mode 100 zeroes +8; branches 101/102 restore +0.
Mode-4 handler `0x82460788`, store `0x82460814`: floor((+8)/(+4)*N),
clamp 0..N, low16→+2; when N is reached it resets +0 at 0x82460824.
N, NaN and a zero period have not been checked dynamically.

**[T]** The consumer/getter/update path ran on DrawThread F80013FC.
In the completed run: LR 0x82460A58 — 15,635 entries; 0x82460850 and
0x824608D4 — 223 each; 222 pairs matched by context/frame marker.
The logger does not print the PC: LR is tied to the original call instructions and the save helper
`0x830D4E9C`, and is not taken as the address of the current instruction.

**[M]** 308 samples: S=0x40F738C4, mode 4, +4=0.5;
+8 grew 0.033505775→0.515492499, +0 changed 1→0.
S=0x40F73964/0x40F73B84: 0.033333134→0.502060354 with the same reset;
14 different consecutive states. Asynchronous reads and the last trace marker
are not an exact per-frame memory trace. Late first addresses may have been reused;
modes 1/3 are not confirmed in this memory run.

**[I,G]** N is the result of the wcslen-like `0x830D5550` on the string from virtual slot+108,
which `0x824609E0` copies into a buffer of 1023 characters. `0x824603A8` /
`0x82460470` select a character by S+2 via the 16-bit copy helper `0x830D60C0`;
`0x8245FB38` compares 10, 32, 0x3000, 48..57. This supports a timed
wide-string / UI interpretation. The source and destination of the string and any effect on combat
are not established; the earlier "state table" is not confirmed.

Evidence: [consumer trace report](../logs/timing-followup-20261019/consumer-early-report.json),
[samples](../logs/timing-followup-20261019/consumer-early-samples.json),
[mode-4 instructions](../logs/timing-followup-20261019/mode4-handler.asm),
[getter](../logs/timing-followup-20261019/getter.asm),
[update](../logs/timing-followup-20261019/8245FA20.asm).
A repeated report reads the saved data: `tools/acv_consumer_report.py TRACE SAMPLES`.

## Start/end timer, scopes and ownership

**[I]** `0x82AF1BF0` passes singleton+0x74 as r3 to
`0x822F4908` and `0x822F4ED0`. `0x822F4908 → 0x822F47E8`
first resets byte+8, qword+0x18/+0xA0, word+0x48/+0xB0;
then it writes a fresh counter to +0x10 and sets active+8=1. This is a repeated start,
not a lazy init. The end `0x822F4ED0`, when active, writes now-start to +0x18
and resets +8, after the drain loop. 220 code bytes / 224 with padding
were verified unchanged; the Ghidra body remained 8 bytes.

**[I]** Drain `0x822F4950`: count=word(obj+0x48),
S=obj+0x20+(count-1)*0x28, id=S+0; R=obj+id*0x70.
If R+0x50!=0 or id=0, it closes the active S timer, adds elapsed
to R+0xA0, increments R+0xB0; the signed max R+0xA8 is computed from the running total,
not from a single elapsed value. Then count is decremented. Callers: 0x822F4A48,
0x822F4BF0, 0x822F4ED0; `0x822F5308` returns base+idx*0x28.

Push `0x822F4678 → 0x822F52C8` copies 5 qwords to +0x20+count*0x28,
then increments count. Slot 1 begins at +0x48, on top of count; record 0 crosses
the header. This limits confidence in the layout/depth; it does not prove runtime nesting.
Scope `0x822F4AB8` and the closes 0x822F4A48/0x822F4BF0 were not found through direct
branch, address-construction or data-pointer patterns, except in .pdata. Do not declare them
unused: other addressing or indirect paths are not excluded. 0x822F4BF0 updates the
maximum per-id mean, capped at F*3/60 (about 50 ms only if F is ticks/s).
The purpose of profiling/frame pacing is not fully established; push/pop must not be carried over.

`0x822F4A48` gate: singleton+0x58 is compared with the result of 0x82B8F9E0
(`*(*(r13+0x100)+0x14C)`); thread identity is a hypothesis.
`0x822F45A0` returns *0x83722018. `0x822F4CF0` creates a
328-byte timer with an inline slot+0x20 and count+0x48. `0x82AF2F70` builds a
stack temporary inside 0x82AF1D38 and is not the paced singleton.
The post-wait `0x82303DF0` walks the table 0x83722168 / count 0x837221A8;
`0x82B49088` calls virtual slot+8 on input+0x84 with r4=-1.
The destination and meaning of -1 and the writers of +0x84 are not established.

.pdata bounds: 0x822F4950=248 bytes; 0x822F4ED0=220;
0x822F4BF0=256; 0x822F4A48=108; 0x82AEF208=148;
0x82AF1BF0=328; 0x82AF1B60=140; 0x82AF2F70=132;
0x82AF12F0=240; 0x826BC7A0=128. Do not confuse unwind records with call tables.
Evidence: [follow-up index](../decompilation/evidence/acv/timing-followup-index.json),
[integrity 4950](../logs/acv-822F4950-integrity.json),
[integrity 4ED0](../logs/acv-822F4ED0-listing-integrity.json).

## Rectangles and FileManager.BindRead: a separate chain

**[I]** `0x82AF5F50` creates a 3008-byte object, ctor `0x82AF7160`,
rectangle differences/ratio → `0x822B5A18`. Startup 1280/720 via
`0x82AF60C0` writes `0x83885E80+0x910/+0x914`; these become x1/y1 of
the two rectangles. The lookup result does not give this pointer: r27 is overwritten with
0x83885E80 at `0x82AF23A0`. View/surface/camera semantic labels were not established.
`0x822B5A18` stores the incoming f1 in outer+0x4C0 without a flag gate;
r6 pointer is saved to +0x370 and conditionally reads +0x77C → +0x374.

**[I]** The key FileManager.BindRead: literal `0x820E96BC`, helper `0x822B2320`
builds a temporary wide string. `0x830D5550` counts 16-bit units up to NUL.
`0x82AF1E88 → 0x82C25350(default=0)` returns bool/default:
search `0x82C25210` / `0x82C24F38`, text expansion of ${...} via `0x82C24EB8`,
`0x82C27148` / `0x82C26270`; true only for exact true/TRUE/yes/YES.
Incomplete markers, missing keys and the recursion limit were not checked.
Node links +0/+8, key +0x0C, marker +0x4D do not prove a named tree type.
Container path `0x82C237B0 → 0x82C24398 → global 0x838BB018`;
vtable `0x8210FE70` slot+0x10 → `0x82C22D20`; an aligned embedded region
near object+0x104, sentinel init `0x82C26458 → 0x82C24FB8`.

The startup setter `0x82AFA5A4` registers true; 14 direct setters were reviewed.
The dynamic site `0x823E07DC` builds Assembly.%s (literal `0x8201EC0C`), so it does not
write the exact FileManager.BindRead. The reader `0x825A6800` (+0xE4),
unique pointer `0x82232848` and writer `0x825A6BB0` via `0x82586130` remain
without a proven alias/dispatch. In the BDT there are two #FileManager.BindRead=false
lines and two BindReadCache prefixes; the relation to # semantics / the asset parser is unknown.
Other indirect setters are not excluded. Evidence:
[lookup summary](../logs/acv-lookup-20261020/summary.md),
[latest follow-up](../logs/acv-lookup-20261020/followup-20261021.md),
[ISO hits](../logs/acv-lookup-20261020/iso-key-hits-20261021.json).

## Checks, corrected errors and limits

- [H] Unity Editor: 9 synthetic cases. Then a separate Unity 6000.6.6f1 batch
  EditMode AcvTimingTests: **13 passed, 0 failed, 0 skipped**, exit 0:
  [XML](../logs/timing-followup-20261019/unity-tests.xml),
  [log](../logs/timing-followup-20261019/unity-tests.log).
  This batch had no compile or ScriptableSingleton errors; earlier GUI
  errors are not closed by it. The utility is not connected to gameplay; Unity is postponed.
- [H] 0x8245FA20 reconstruction: 69,152 state checks + 10 transitions;
  scalar 0x830D68F0 / 0x830D69D0: 80,784 finite split comparisons,
  fused differs 3825/3772; this is not a Xenon oracle. [Archive](fine/README.md).
- Ghidra save-helper noreturn/ABI guesses may truncate bodies. Earlier
  bounded repairs of 0x82AEF208 / 0x82AF1BF0 and helpers are kept in history,
  bytes verified; do not repeat changes to the live database. Metadata base 0
  does not replace the guest image 0x82000000 / .text 0x822B0000. analysis_complete
  does not mean a trustworthy pseudocode. Generic PWR7 VMX names are not an oracle.
- The old disassembler skipped unknown words; the corrected ppc_dis.py
  prints .long for every 4-byte word. Do not use the old shifted listings.
- Later pointer sampling, reordered zero-active conditions, the slot=50%
  hypothesis, three instead of two P objects, and the lazy-init interpretation are replaced above.
- ptrace_scope after the permitted sampling was restored to 1, and the Xenia config
  was restored byte-for-byte: [cleanup](../logs/timing-followup-20261019/cleanup-and-hashes.json).
  A later installation observed 0: [observation](../logs/xenonrecomp-install-20261020/ptrace-observed.txt).
  These are different dates; the current sysctl value is not asserted here. No new changes.
- REA 6.7.0 doctor/providers: healthy=false, the Ghidra provider is present;
  the standard PowerPC profile targets BE ELF32 ET_EXEC, XEX/Xenon support
  is not proven. No REA setup, engine install or ACV recovery was run.

Still open: the gameplay link, VMX conditions, indirect scope callers, the post-wait target,
checks of the other modes and runtime edge cases, and a trusted rounding oracle.
The work queue is in [PLAN](PLAN.md); new evidence replaces the corresponding line or section
here rather than adding another chronological retelling.
