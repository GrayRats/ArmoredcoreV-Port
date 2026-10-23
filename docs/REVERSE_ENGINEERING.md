# Reverse engineering log

## Scope and evidence rules

The only currently identified source is the Armored Core V Xbox 360 image at `../Original/Armoredcore/iso.iso`. Original input is read-only. No function names, layouts, addresses, or behavior claims are accepted without a reproducible artifact and evidence citation (file, offset/address, and method).

## Verified observations

- `dvd.dvd` says `LayerBreak=1913760` and `Armored core V.iso`.
- ISO 9660 volume ID is `13599`. The ISO 9660 filesystem view contains Xbox dashboard files under `/_SYSTEMU/` and DVD video files. This view is only a small part of the image and does not expose the game filesystem.
- XISO-aware listing reports 26 files totaling 3,547,390,725 bytes. The largest files are `/bind/dvdbnd.bdt` (3,279,025,311 bytes), `/movie/jp/acv_opening.wmv` (173,369,732 bytes), and `/default.xex` (23,281,664 bytes).
- Full XISO extraction completed under `tmp/iso_root/`; the executable is also staged at `games/acv/extracted/default.xex`. Its first four bytes are `XEX2`.
- Ghidra 12.1.2 with XEXLoaderWV 13.0.0 successfully imported the retail XEX in a workspace-local headless project. The loader reported encryption type 1 and compression type 1, then loaded sections into a PPC program. This is loader-managed in-memory processing; no standalone decrypted ELF was emitted.
- Loader-selected language: `PowerPC:BE:64:A2ALT-32addr:default`. Loader-reported image base: `0x82000000`; entry point: `0x82B91F88`.
- Loaded sections: `.rdata` at `0x82000400`, `.pdata` at `0x82216A00`, `.text` at `0x822B0000`, `.data` at `0x83540000`, `.XBMOVIE` at `0x83990E00`, `.tls` at `0x83991000`, `.idata` at `0x839A0000`, `.XBLD` at `0x839B0000`, and `.reloc` at `0x839B0200`.
- Loader reported 73,307 function definitions and 295 import references / 282 thunks. A raw address/name listing is saved at `decompilation/functions/acv-ghidra-functions.tsv`. These are loader-created entries, not evidence of semantic function names.
- A full auto-analysis attempt generated two invalid-PNG analyzer errors and p-code constructor warnings while processing this large function set; the run was stopped. The saved project remains a `-noanalysis` import. One targeted function was decompiled separately below; no whole-program analysis conclusions are recorded.
- A separate general-purpose `xex_info.py` helper reported inconsistent optional-header values (for example, entry point `0x600`). Those values are not used here; the loader log is the recorded source.
- XEX SHA-256: `4fac87348f827e84633013375012284cd8c769a3c155a675b2989757e666a12b` (staged retail image, `games/acv/extracted/default.xex`).
- Targeted Ghidra decompilation of loader entry `0x82B91F88` completed without global auto-analysis. The export is `decompilation/pseudocode/acv-82B91F88.c`; the listing identifies the entry as loader-labeled `Function_82B91F88`, not a recovered semantic name. It is 440 bytes per the Ghidra function body. The decompiler output shows initialization calls, parsing a command-line string into at most 16 token slots with quote handling, then dispatch/cleanup calls. These are observations of the pseudocode; the called routines and their intent remain unidentified.
- Added `./scripts/decomp.sh decompile [address]` to request bounded decompilation of one loader-defined function without rerunning slow whole-program analysis. Ghidra's project and output stay under the workspace.

## Verified asset-container index

- `dvdbnd5.bhd` SHA-256: `816fa23bc3721fbe11d29a56e335b6a9f6945fc67fd561e730602ea2b5330bb2`; `dvdbnd.bdt` SHA-256: `a66d5305c5a2977c2cf13aab20c8916c4b2924bdbac2de3c74a66a23d963b7ca` (3.28 GB).
- BHD starts with `BHD5`; the big-endian interpretation has marker 0, version word 1, declared size 338,592, and 2,833 bucket records beginning at offset `0x18`.
- A 16-byte bucket entry layout (`u32 path hash`, `i32 size`, `u64 BDT offset`) was checked against all buckets: 19,744 entries parse, every range stays within the BDT, and the maximum range end equals the 3,279,025,311-byte BDT length. Tested 24- and 32-byte layouts fail bounds checks. This identifies a structurally consistent layout; exact game-specific semantics and any omitted transforms remain open.
- Generated `assets/manifests/acv-dvdbnd.tsv` with bucket, entry index, 32-bit hash, offset and size. Hashes are not decoded filenames; the index contains no extracted game content.
- Signature scan: 14,971 entries start with `DCX\0`, 933 with `TPF\0`, and 399 with `BND3`. These are byte-signature tallies, not a complete classification of all asset types.
- All 14,971 DCX entries have `DCP\0DFLT` at offset `0x24`; all decompressed successfully as zlib and exactly matched declared sizes, with no trailing/unused stream bytes. Decoded leading signatures: TPF 6,391; FLVE 3,628; BND3 2,609; `10 00 01 17` 2,142; SMD4 201. Total decoded bytes: 4,065,172,862.
- Parsed the DCX inventory's 2,609 BND3 payloads; all use observed format `0x26` and parse with the currently supported layout. Across BND3 and TPF children, record-level parsing reports 41,825 entries. This does not cover the 157 other direct BND3 variants or imply semantic identification of child formats.
- Batch-extracted 22,605 raw texture payloads from 932 direct X360 TPFs and 6,390 valid DCX-wrapped TPFs (3,070,066,688 bytes). One direct platform-2 TPF and one malformed DCX-wrapped TPF are recorded in the unsupported report. Texture GPU format and Xenos tiling remain unresolved.
- Added `tools/bhd5_index.py` for bounded validation, TSV indexing, explicit hash extraction, and selective decoding of the validated DCX/DCP DFLT form. It rejects writes outside the workspace. Bulk extraction is intentionally not the default.
- A decoded sample was saved at `assets/original/acv-dvdbnd-samples/f29f543a.decoded.bin` (1,417,648 bytes; starts `TPF\0`). The Xbox 360 platform marker and big-endian metadata match the Xbox 360 branch in the public [SoulsTemplates TPF structure](https://github.com/JKAnderson/SoulsTemplates/blob/master/TPF.bt).
- Added `tools/tpf_x360.py`. It validates the 28-byte Xbox 360 table entry, Shift-JIS names (encoding marker 2 in the sample; consistent with the public [SoulsFormats TPF reader](https://github.com/JKAnderson/SoulsFormats/blob/master/SoulsFormats/Formats/TPF.cs)), dimensions, offsets, non-overlapping payloads and aggregate payload size, then exports raw `.x360tex` payloads plus a TSV manifest. It explicitly rejects other platform variants.
- Ran that parser over all 932 direct platform-1 TPF entries in the BDT: 1,473 texture records validated. One platform-2 TPF was observed and intentionally skipped. Nine texture payloads from one decoded DCX sample were exported with names/dimensions in `assets/original/acv-tpf-samples/`.
- Payloads are raw Xbox 360 texture bytes. The TPF `format` byte interpretation, Xenos tiling/byte order, DDS/PNG conversion, and Unity import have not been validated.
- `script.bhd` begins with `BHF311G21X35`; its paired `script.bdt` begins with `BDF311G21X35` and a zlib-looking stream marker at offset `0x10`. This is an initial byte observation only; parser/schema and decompression validation are pending.

## Live Ghidra RPC session (2026-10-19)

- Latest continuation: managed RPC was restored after process exit. The reopened database reports 67,314 function definitions. Bounded snapshots, current section map and truncation warnings are in `decompilation/evidence/acv/timing-index.json`; `docs/GHIDRA_CAPTURE.md` documents reproduction. The `analysis_complete` flag only combines the program's analyzed marker and analyzer-idle state.
- Instruction evidence corrects `0x822F4908` to repeated reset/start timing, paired with elapsed storage in `0x822F4ED0`. The latter has a complete 220-byte listing but an 8-byte database body. The 224 bytes including padding were unchanged by bounded disassembly. See `docs/ACV_TIMING.md` for offsets, pointer-selection conditions, and unresolved indirect dispatch.
- Started a managed GUI session on `tmp/ghidra/isolated-projects/ACV_RETRY.gpr`; the user's separate `Armoredcore.gpr` process is not the RPC-controlled session. Confirmed `/default.xex` in the expected project, PowerPC, 32-bit pointers, big-endian, XEXLoaderWV format, and 73,307 function definitions. `analysis_complete` is false.
- `scripts/ghidra-rpc.sh` keeps RPC endpoints, state and Java/Python runtime files inside the workspace. GUI navigation to `0x82AEEF58` succeeded. While the session owns this project, use RPC rather than concurrent headless `decomp.sh` operations.
- Exported `decompilation/pseudocode/acv-82AEEF58.c`, raw RPC JSON, outgoing xrefs, and a listing beginning at `0x82AEF0A0`. The entrypoint calls this function at `0x82B92114`.
- Observed a repeated path calling `0x82AF1BF0` and `0x82AEF208`; instructions at `0x82AEF0C4` and `0x82AEF0D0` multiply floating-point values. The decompiler resolves constants near `1/60`, `0.01`, and `600`. This is a candidate for main-loop/time-step analysis; units, subsystem meanings, and gameplay tick rate are unverified. No function renames or invented structures were applied.
- Integration failures and their workarounds are recorded in `logs/ghidra-rpc-issues-20261019.md`.

## Pending

- Timing follow-up: `docs/ACV_TIMING.md` records normalizer/pacing instruction evidence, kernel-call chains, local Ghidra function repairs and the Unity reference formula. The timer and pacing functions now have validated instruction bodies; inferred ABI and subsystem meanings remain unverified.

- Continue targeted decompilation and map call targets/imports before interpreting gameplay code. Whole-program analysis is still incomplete.
- Investigate analyzer errors/warnings before treating affected code as understood.
- Add loader-backed exports only; keep function names and data layouts unverified unless supported by code evidence.

## Read-only Ghidra RPC and rendering checkpoint — 2026-10-22

RPC started headless against the existing ACV_RETRY project; `load --no-analyze`
reopened its existing `/default.xex` (existing-program reuse checked in
`server/context.py:257-299`), without requesting analysis or structural edits.
Metadata: PowerPC, 32-bit pointers, big-endian, 67,314 function entries.
Operation `f4252aa5-297a-4520-a898-df5572afacbc`.
Listing operation `93865684-0deb-4d6a-8454-e136f9215801` confirms the call
`0x82BB6C20 -> 0x83537294` and stack output pointers r3=r1+0x54,
r4=r1+0x50. The listing warns it skipped undefined instructions at 0x82BB6C00;
this is incomplete database coverage, not a verified function entry.
Xrefs report one unconditional call to that import, from 0x82BB6C20;
operation `482e349e-a6de-48f4-bc2d-6601fc143c61`.
The existing function object at 0x82BB6BF8 has an 8-byte body, ending
0x82BB6BFF; no function-body repair was made. Its pseudocode cannot represent
the complete routine. Use raw instructions and independent extent evidence.
Ghidra MCP is not exposed in this session; RPC works.

The invalid-fetch experiment `20261021T211711835573Z` ended with child exit 0
before the 150-second limit (no claim about why the window closed). Its only
80-second capture has one non-indexed 4-vertex draw and no indexed draws;
this is insufficient to compare gameplay shading. Data:
`tmp/acv-invalid-fetch-171434.xml`; summary operation
`e3c81922-bb7b-49fb-8988-655fa5402299`.
New `tools/native-rexglue/summarize_capture.py` summarizes XML commands without
images. The screenshot subprocess has been removed from probe_linux.py;
nonzero --screenshot-every is rejected. Existing unique evidence is preserved.
FBO run `20261021T212231576790Z` is testing late captures (110/140 seconds)
with protect_zero=false and headless text dialogs. Scene equivalence remains
unverified; one-draw loading frames cannot determine rendering correctness.
