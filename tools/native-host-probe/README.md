# Bounded native host probe

This x86-64 C++ prototype executes two existing hedge-dev XenonRecomp leaves
against a small committed region in a reserved 4 GiB guest address range. It is a
partial L3 implementation, with no external runtime dependency or runtime mixing.

A separate focused probe also extracts the generated leaf at `0x82B8F9C0`, checks
its seven raw words, and executes its emitted conditional reread/store path with a
deterministic `__rdtsc` stub. It validates two-read and one-read branches, the
64-bit guest store and return/register preservation. Run it with:

```sh
node tools/decomp-logging/cli.mjs run python3 tools/native-host-probe/run_timebase.py
```

The stub tests the generated leaf's structure only. It does not establish that
host `RDTSC` has Xenon time-base frequency, wrap behavior, or epoch.

From `/home/art/DEV/DECOMP/DEArmoredcore`:

```sh
node tools/decomp-logging/cli.mjs run python3 tools/native-host-probe/run.py
node tools/decomp-logging/cli.mjs run node tools/native-host-probe/record-evidence.mjs
```

Prerequisites: the current `tmp/xenon-out/ppc` generated output,
`tmp/scan/text.bin`, Clang, Python and the local SIMDe headers in
`tools/XenonRecomp/thirdparty/simde`. Output stays in `tmp/native-host-probe/`.
The runner verifies the exact five PPC words and extracts function definitions
verbatim; it does not modify generated shards or upstream tool sources. The
report records SHA-256 hashes, source line numbers, commands and exit codes.

The header uses existing `PPC_LOAD_U*`, `PPC_STORE_U*` and
`PPC_CALL_INDIRECT_FUNC` override points in the generated `ppc_context.h`.
This preserves the actual `PPCContext&, uint8_t* base` ABI while replacing unchecked
memory accesses with page validation and bytewise big-endian transfers. The
checked dispatcher deliberately does not use the default embedded pointer table.
Only the selected two leaf symbols are registered; missing targets fail explicitly.

The 41 checks cover generated stores/clear, big-endian byte representation,
unchanged argument and link registers, 8/16/32/64-bit host memory helpers,
unaligned helper transfer, unmapped and crossing-page accesses, guest address
overflow, wrong base, unknown/misaligned/overflowing dispatch and a generated
store to an unmapped page, invalid commit ranges, the final guest page,
recommit preservation and transfers across two committed pages. Linux builds
enable UBSan and abort on a detected violation.

Windows cross-build and Wine compatibility check (Clang plus installed MinGW):

```sh
node tools/decomp-logging/cli.mjs run --stage host_portability_wine -- python3 tools/native-host-probe/run.py --target windows --wine --output tmp/native-host-portability/windows-wine
```

Without `--wine`, `--target windows` only builds the PE executable. The Windows
backend uses `VirtualAlloc` reserve/commit and `VirtualFree` release. Wine uses
an isolated prefix inside the output directory; the user's default prefix is
not selected. Windows has no UBSan in this configuration. The current Clang 23 /
MinGW GCC 16 static link emits duplicate RTTI section-size diagnostics, although
all 41 checks pass under Wine 11.20. This narrow result does not establish
general C++ exception ABI compatibility or execution on an actual Windows host.
Detailed results: [NATIVE_HOST_L3.md](../../docs/NATIVE_HOST_L3.md).

These are synthetic expectations checked against the five raw instructions.
They are **not** Xenia comparisons or L4 parity. `0x83885000` is a synthetic test
page, not an observed live ACV object. The 4 GiB reservation has inaccessible pages
except the test page; it is not a complete Xbox memory layout or guest allocator.
No executable image is loaded, and the game entry point is not called.

Absent: XEX loading/import binding, kernel/xam services, guest exception behavior,
threads/TLS/synchronization, clocks, MMIO semantics, VFS, input, graphics and audio.
The header's MMIO fallbacks remain ordinary checked memory transfers and have no
device behavior. VMX direct accesses, atomics and unselected generated instructions
have not been audited. Checked macros cannot guard arbitrary direct `base + VA`
pointer operations or atomics emitted elsewhere (for example `lwarx`). This probe
is not a general sandbox. Single-threaded scope objects do not supply Xbox threading.
This prototype must not be linked with full shards as though it were a game runtime.

The concrete next step is a bounded import/clock host with verified signatures and
a small complete generated call closure, followed by same-input Xenia evidence.
Untranslated instructions and unresolved control flow remain blockers to full
image execution; this isolated leaf probe does not waive the L1 quality gate.

The complete generated `0x822C8AD8 -> 0x82B8F9C0` clock-return closure has a
separate runner, `run_clock_closure.py`. It checks the wrapper's `.pdata` extent,
all 17 raw words, call dependencies and 4,096 synthetic sample pairs (32,768
assertions). Both native Linux/UBSan and `--target windows --wine` pass. The
injected clock does not establish Xbox frequency or runtime parity. Details and
saved evidence: [NATIVE_HOST_L3.md](../../docs/NATIVE_HOST_L3.md).

`run_clock_closure.py --frequency-import` selects the generated `0x82B8FA70`
wrapper and one explicitly configured frequency import adapter; 44 assertions
pass on Linux/UBSan. No default clock frequency is assumed. See
[NATIVE_XEX_RUNTIME.md](../../docs/NATIVE_XEX_RUNTIME.md).

Current workflow builds and runs Linux only. The Windows switches above document
earlier checks and are used only on a separate explicit user request.
