# Asset format notebook

The ACV image contains `/bind/dvdbnd.bdt` (3,279,025,311 bytes) with companion `/bind/dvdbnd5.bhd` (338,592 bytes), plus `/bind/script.bdt` (1,328,198 bytes) and `/bind/script.bhd` (18,308 bytes). The `dvdbnd5` index has been structurally parsed and validated; the script pair remains unidentified.

## `dvdbnd5.bhd` index (validated structure, filenames unresolved)

- BHD SHA-256: `816fa23bc3721fbe11d29a56e335b6a9f6945fc67fd561e730602ea2b5330bb2`.
- BDT SHA-256: `a66d5305c5a2977c2cf13aab20c8916c4b2924bdbac2de3c74a66a23d963b7ca`.
- Header magic is `BHD5`. Big-endian fields observed: byte 4 marker `0` (matching the big-endian marker in the public SoulsFormats BHD5 reader), version word `1`, declared header length `0x52aa0`, bucket count `0xb11`, and bucket-table offset `0x18`.
- Each bucket table entry is 8 bytes: a count and absolute offset. The matching per-file entry size is 16 bytes: 32-bit hash, signed 32-bit stored size, and 64-bit BDT offset. Iterating all 2,833 buckets yields 19,744 entries. Every data range lies within the BDT and the largest range ends exactly at the BDT length. 24-byte and 32-byte alternatives produce out-of-range records.
- This 16-byte layout matches the non-extended branch of the public [SoulsFormats BHD5 reader](https://github.com/JKAnderson/SoulsFormats/blob/master/SoulsFormats/Formats/BHD5.cs), but that cross-game reference alone does not establish every ACV-specific property. The all-entry bounds checks are local evidence for the layout. Path hashes have no recovered names yet.
- `assets/manifests/acv-dvdbnd.tsv` is the generated index. It records decimal path hashes plus bucket, item, offset, size, and the first up-to-eight payload bytes as hex. `tools/bhd5_index.py` validates the header and data bounds, creates that TSV, or extracts only explicit hashes into a workspace-local output directory.
- Signature scan found 14,971 entries beginning `DCX\0`, 933 beginning `TPF\0`, and 399 beginning `BND3`. All 14,971 DCX entries contain `DCP\0DFLT` and internally consistent stored/compressed size bounds. The wrapper is consistent with the DCX format, whose public parser explicitly lists ACV support ([SoulsFormatsNEXT DCX reader](https://github.com/soulsmods/SoulsFormatsNEXT/blob/master/SoulsFormats/Formats/DCX.cs)).
- A local implementation of the observed DCP/DFLT path reads the declared uncompressed and compressed sizes and zlib-decodes the stream. All 14,971 payloads decoded to their declared lengths with no trailing or unused zlib data; total decoded size is 4,065,172,862 bytes. Decoded leading signatures: TPF 6,391; FLVE 3,628; BND3 2,609; `10 00 01 17` 2,142; SMD4 201. The selective decoder is exposed as `--decompress-dcx`; it does not decode other DCX variants.
- One decoded TPF sample is `assets/original/acv-dvdbnd-samples/f29f543a.decoded.bin` (1,417,648 bytes). Its Xbox 360 header and records were parsed and its nine raw texture payloads were extracted; conversion for Unity is still pending.

## Xbox 360 TPF entries (metadata and raw payload validated)

- The TPF platform byte is `1` for the validated Xbox 360 variant; metadata is big-endian. The external [SoulsTemplates TPF template](https://github.com/JKAnderson/SoulsTemplates/blob/master/TPF.bt) explicitly maps platform 1 to X360 and describes this 28-byte record: data offset/size; format, cubemap, mip count, flags; width/height; zero reserved word; name offset; zero reserved word. This layout matches the files tested locally.
- Header `dataSize` equals the sum of entry payload sizes, not the whole TPF container size. Entry names with encoding byte `2` are decoded as Shift-JIS, matching the behavior in the [SoulsFormats TPF reader](https://github.com/JKAnderson/SoulsFormats/blob/master/SoulsFormats/Formats/TPF.cs).
- `tools/tpf_x360.py` checked all 932 direct platform-1 TPF containers: 1,473 records had valid names, data ranges, dimensions, non-overlapping payloads, and a matching aggregate byte count. There is one platform-2 TPF in the index; this Xbox 360-only parser rejects it.
- Raw texture data and metadata from a nine-entry TPF are in `assets/original/acv-tpf-samples/`. `.x360tex` files are not DDS and are not Unity-ready. The format byte mapping, Xenos tiling/byte order, and texture conversion remain unresolved.
- `./scripts/assets.sh extract-dvdbnd-tpf` parsed 932 direct platform-1 TPFs plus 6,390 decoded TPFs from DCX entries. One direct platform-2 TPF and one malformed DCX-wrapped TPF are listed in `assets/original/acv-dvdbnd-textures/unsupported-tpf.tsv`.
- The batch produced 22,605 raw payloads (3,070,066,688 bytes) across the validated TPFs. `textures.tsv` records source hash, name, format byte, dimensions, mip count, flags, size, SHA-256, and output filename. Format-byte counts are 0: 17,981; 5: 2,229; 23: 140; 24: 2,255. These numeric codes have not been mapped to GPU texture formats; no DDS/PNG conversion is claimed.

## BND3 archives (one confirmed variant)

- There are 399 direct BDT entries beginning `BND3`. The 010 Editor [BND3 reader](https://github.com/JKAnderson/SoulsFormats/blob/master/SoulsFormats/Binder/BND3/BND3.cs) and [BinderFileHeader reader](https://github.com/JKAnderson/SoulsFormats/blob/master/SoulsFormats/Binder/BinderFileHeader.cs) document the header/table conventions used for cross-checking.
- `tools/bnd3_x360.py` supports only the observed `format=0x26`, big-endian, bit-big-endian BND3 variant with 32-bit data offsets. It validated 242 containers containing 4,737 named entries; the other 157 format variants are listed in `assets/original/acv-bnd3-extracted/unsupported-containers.tsv` and were not parsed.
- In those validated containers, 4,696 entries carry the compressed flag. All 4,696 decoded as zlib and matched the BND3-declared uncompressed size. `./scripts/assets.sh extract-bnd3` stores the extracted entries and an aggregate hash/name/size manifest under `assets/original/acv-bnd3-extracted/`.
- The 242 parsed BND3s contain 2,764 `.ani`, 1,680 `.ffx`, 175 `.mqb`, 52 `.tpf`, and additional files. `./scripts/assets.sh extract-tpf` validated all 52 TPFs (173 textures) and wrote raw payloads to `assets/original/acv-bnd3-textures/`.
- Texture format-byte values in these 173 files are 0 (162), 5 (9), and 24 (2); their meanings are still unknown. The bytes remain raw Xenos texture payloads, not Unity-ready images.

## Script container (unresolved)

- `script.bhd` begins `BHF311G21X35`; its partner `script.bdt` begins `BDF311G21X35`. A zlib stream begins at BDT offset `0x10`; it independently decodes to 23,612 bytes and reaches a clean stream end, leaving 1,320,199 bytes after that member. The output begins `1b 4c 75 61` (not identified). This proves a stream exists, not the full archive layout or relation to BHD records. Hashes: BHD `c08a425434ae3c83e113c7b9b8b4928828b12015731263c3a0e5d0d2cfe782f5`; BDT `2ce4b715e34c552cb670de349d2799cec2d7d62cf99471653ade6d04fba9b5f6`.

For each future format, record: source file and hash, container offset and size, byte order, header fields, alignment, compression/codec, parser version, extracted output paths, and validation evidence. Preserve unknown fields as unknown. Never label a texture or geometry layout from visual guesses alone.

Known generic candidates (not yet confirmed in these files): BC1/DXT1, BC3/DXT5 and BC7. Do not apply a decoder until format identification supports it.

## Full dvdbnd extraction (2026-10-22)

`tools/bhd5_extract_all.py` writes every one of the 19 744 BHD5 entries to
`assets/original/acv-dvdbnd-all/<path-hash>.<ext>` (4.9 GB), decoding the validated
DCX/DCP DFLT form for 14 971 entries; manifest `assets/manifests/acv-dvdbnd-all.tsv`
(stored/decoded size, SHA-256). Extension comes from the decoded magic only; original
names are not recovered. Counts: 7 324 TPF, 4 811 unidentified `bin`, 3 628 FLVER
(`FLVE`/`FLVER\0` header), 3 008 BND3, 378 TAE, 255 `\0BRD`, 201 `SMD4`, 70 XML, 69 FEV1.
`tools/verify_dvdbnd_extraction.py` (independent): 0 hash collisions, 0 manifest hash
mismatches, raw entries byte-equal to their BDT slice, DCX raw sizes equal the headers,
and all 4 737 files of the earlier BND3 extraction are byte-identical.

`tools/bnd3_extract_dir.py` unpacks BND3 from that tree into `assets/original/acv-bnd3-all/`
(3 003 of 3 008 containers, 33 165 files, 1.4 GB) and, one level deeper, into
`assets/original/acv-bnd3-nested/` (347 of 352, 6 503 files). `bnd3_x360.py` now also
accepts format byte `0x2c`: 20-byte records without a file id (flags, stored size, data
offset, name offset, unpacked size), derived from the bytes of 152 containers and kept
under the same bounds/overlap/zlib checks (0 compressed entries observed in that variant;
the entries carry real paths such as `mission\mission_6100.xml`, and `.evd` files all
start with `EVD\0`). Still unsupported and not guessed: 4×format `0x67`, 1×`0x54`
(top level) and nested format `0x06` (4) and `0x0e` (1).

Not decoded yet: 2 142 DCX payloads starting `10 00 01 17`, 201 `SMD4`, FLVER
contents, TAE/FEV1/BRD contents and about 4 800 `bin` entries over 451 distinct leading
words (largest: `00 01 00 00` 607, `00 00 00 80` 304, `00 00 00 70` 202, `06 01 00 00`
147; `FSB4` 50). These are extracted byte-exact, not interpreted.

## FLVER header inventory (2026-10-22)

`tools/flver_inventory.py` reads the 7 111 `FLVER\0` files in the extracted trees
(`acv-dvdbnd-all`, `acv-bnd3-all`, `acv-bnd3-nested`) and writes
`assets/manifests/acv-flver-inventory.tsv`. Header fields follow the public SoulsFormats
FLVER2 reader as a hypothesis; every file was checked only against structural invariants
(data range inside the file, count ranges, finite and ordered bounding box). All 7 111
pass. 9 files are empty models with a ±FLT_MAX sentinel box and zero meshes, flagged as
`empty-model`. Big-endian marker on all files. Versions: `0x2000e` 4 456, `0x20009` 2 650,
`0x20007` 3, `0x12` 1, `0x14` 1 (the last two are older layouts; not parsed further).
Meshes 1–99, bones 1–384, materials 1–42 in non-empty files. 6 874 distinct contents
across 7 111 files (237 byte-identical duplicates). Vertex and face data are not decoded.

## FLVER read by the public SoulsFormats reader (2026-10-22)

Source: `JKAnderson/SoulsFormats` (git clone, `SoulsFormats/Formats/FLVER/FLVER2/FLVER2.cs`),
built as netstandard2.1 and called from a small .NET 8 tool (`/tmp/flverdump`, not in the
repo). The header offsets in `tools/flver_inventory.py` agree with this reader's `Read` order
(version 0x08, data offset 0x0C, counts 0x14–0x24, bbox 0x28, face counts 0x40).

Results over all 7 111 `FLVER\0` files (`assets/manifests/acv-flver-soulsformats.tsv`):
4 350 read (2 566 version `0x2000E`, 1 784 version `0x20009`): 13 391 meshes, 14 170 042
vertices, 26 273 face indices, 9 empty models. 2 761 fail in the reference reader itself:
2 095 with an unknown vertex layout type `0x14` (not in SoulsFormats' `LayoutType`),
649 with `Short2toFloat2` bone indices (SoulsFormats: "Read not implemented") and 17 with
face-set linkage errors. These are reference limits, not extraction errors; the type `0x14`
meaning is not inferred here.

## Vertex layout type 0x14 (2026-10-22)

The reference reader stops on layout member type `0x14`. Size was inferred from the data,
not from a name: each vertex buffer header stores its declared element size, and a layout
member's size must make the layout sum equal that value. A probe copy of SoulsFormats
(`tools/flver_probe/unk14-soulsformats.patch`: enum entry, configurable size via
`ACV_UNK14_SIZE`, and a skip in `Vertex.Read`) was run over the 2 096 affected files for
sizes 1, 2, 4, 8, 12, 16. Only size 4 reads without error: 1 854 files, all with a
`Tangent` semantic member of type `0x14`, and all vertex positions finite. The 4-byte
element encoding (per-byte scaling, component order) is not verified; the size is.
Remaining errors at size 4: 232 files with `Short2toFloat2` bone indices (reference
limitation), 8 face-set linkage errors, and 1 file whose version field is 0x14 (not a model).
Manifest: `assets/manifests/acv-flver-unk14-size4.tsv`; operation 20fb3de7.

### Tangent encoding of 0x14 (measured)

Per vertex, 4 bytes `[w, x, y, z]`: `w = (sbyte)b0 / 127` (b0 is always +127 or -127 in
the sample, i.e. a bitangent sign), `x, y, z = (sbyte)b1..b3 / 127`. Over 1 854 files and
7 783 594 tangents: 7 750 960 have unit xyz length within 0.02; the other 32 634 are exact
zero vectors (null tangents, all in 22 files, e.g. `o1406`, `o1409`). No other deviations.
Manifest `assets/manifests/acv-flver-unk14-tangent-check.tsv`; operation recorded above.

### Bone indices of type 0x12 (2026-10-22)

Layout type `0x12` (`Short2toFloat2`) with semantic `BoneIndices` has 4 bytes per vertex.
Hypothesis tested with the probe copy: the bytes are four indices into the mesh's own bone
palette (`Mesh.BoneIndices`), read like `Byte4B`. Result over 649 affected files (4 696 meshes):
0 meshes with any index >= palette size. Weights were not checked in this step.

### Final probe over all FLVER files

With the `0x14` tangent and `0x12` bone-index decodings in the probe copy, SoulsFormats reads
7 085 of 7 111 `FLVER\0` files. 26 fail: 21 with `Face set not found or already taken`
(reference linkage check, not investigated) and 5 whose version field is not a model version
(0x12, 0x14, 0x1161, 0x2D6A). Manifest `assets/manifests/acv-flver-probe-final.tsv`;
patch `tools/flver_probe/unk14-soulsformats.patch` (it contains both decodings and the probe
hooks; not a fix for upstream).

### Draw-to-model link (frame 73703, tail draws; 2026-10-22)

Per-face-set index counts for all readable FLVER files: `assets/manifests/acv-flver-faceset-counts.tsv`.
All 55 element counts of the last draws in frame 73703 occur among face-set counts, but a single count is
weak (≈3 % chance for a random integer from the same range). Ordered windows are stronger: six consecutive draws
(events 7176–7194; counts 79, 691, 9, 19, 79, 354) match contiguous face sets of `0001_o2092.flv`, and no other
file matches a window of six. Most longer windows (≥4) match nothing, so most of these draws are not whole
FLVER face-set sequences, and coverage of this frame is low. The match is a candidate, not a material or texture
confirmation; the bound textures of those draws have not been compared with the model's material list yet.

### Texture check for the o2092 candidate (2026-10-22)

Materials of `0001_o2092.flv` (`cafe436b`) reference `m0200_*` textures (`map\m0299\tex`), listed
in `assets/original/acv-dvdbnd-textures/textures.tsv` with sizes. The bound textures of draws
7176, 7180, 7184, 7188 and 7192 are exactly two per draw: 512×512 and 256×256. Several o2092
materials have the same pair (e.g. `m0200_006`, `m0200_004`, `m0200_005`, `m0200_012`,
`m0200_022` with `_s` masks). Size alone therefore does not identify the material; the
candidate is consistent with `o2092` but not confirmed. The replay texture payloads are decoded
block-compressed data and cannot be compared byte-for-byte with the TPF payload hashes here.

### Strict texture comparison: status (2026-10-22)

Not done. Blockers found:
- Xenos 2D tiling exists locally: `tools/rexglue-sdk/src/graphics/pipeline/texture/util.cpp`
  `GetTiledOffset2D` (cites UModel `UnTexture.cpp`). It is a usable reference, not yet validated here.
- The `format` column of `assets/original/acv-dvdbnd-textures/textures.tsv` (values 0, 5, 23, 24)
  is not mapped to the Xenos `GpuTextureFormat` enum in `xenos.h` (e.g. `k_8_8_8_8 = 6`, `k_DXT1 = 18`).
  The mapping must be established before any block decode.
- Replay-side texture bytes need a dump tool (none exists for raw `GetTextureData`).

### BC decode of Xbox 360 textures: blocked (2026-10-22)

Format codes are now mapped from the public SoulsFormats table (`Formats/TPF/Headerizer.cs`):
TPF codes 0 and 24 are DXT1, 5 and 23 are DXT5 (17 981 + 2 255 DXT1 and 2 229 + 140 DXT5 records
in `textures.tsv`).

Decoder: `tools/x360_bc_decode.py` (BC1/BC3 blocks, byte-order option, numeric statistics only).
Tested on `ae47bdf3_0000_m0200_009.x360tex` (512×512, DXT1, mip0 = 131 072 bytes):
- linear layout, no swap: roughness 49.7, mean RGB ≈ (102, 100, 98);
- linear layout with 16-bit swap: roughness 14.6, mean RGB ≈ (73, 68, 53);
- tiled layouts through the SDK `GetTiledOffset2D` (texel and block coordinates): offsets leave
  the mip0 range, so most blocks read zeros and the image is black. No shift of the SDK result
  makes the block→offset map a bijection onto the mip0 range.

Conclusion: the SDK 2D tiling function does not describe this data as used here; the Xbox 360 BC
tiling still has to be derived (fit against a bijection and smoothness criterion) or taken from a
validated source. No texture has been compared with the replay yet.

## User-supplied FromSoftware references (2026-10-22)

Pinned source snapshots, commit IDs and per-file hashes are in
[repositories.json](../logs/acv-user-references-20261022/repositories.json);
[assessment](../logs/acv-user-references-20261022/assessment.json) separates
checked applicability from future candidates. No original media was modified.

- [DSEventScriptTools](https://github.com/HotPocketRemix/DSEventScriptTools):
  its dcx_handler.py was executed on ACV DCX entry f29f543a. All1,417,648
  decoded bytes match our previous extraction, SHA-256
  babd68e0e454eb0a310f910475ca3ff9e70e914dfafe2d780e8424773cd9b235.
  [Comparison](../logs/acv-user-references-20261022/dcx-reference-comparison.json).
  EVD/ELD/EDF remain useful references; the ACV big-endian EVD variant and
  opcode definitions are not validated by this one DCX comparison.
- [DarkSoulsTextureExtractor](https://github.com/HotPocketRemix/DarkSoulsTextureExtractor):
  tpf_unpacker.py:53-66 uses a little-endian header and20-byte records for DS1 PC.
  Our sample is big-endian, platform1,28-byte records. It cannot directly replace
  tpf_x360.py or establish Xbox tiling/BC3 correctness.
- [BinderTool](https://github.com/Atvaark/BinderTool):
  TpfFile.cs:21-52 uses BinaryReader LE with Common/Bloodborne branches, and
  Bhd5BucketEntry.cs:18-22 always reads salt/AES offsets after the first16 bytes.
  ACV's checked BHD5 entries are16 bytes. Treat the source as comparison material;
  a direct invocation on these ACV variants is not established.
- [DarkSoulsIII.FileFormats](https://github.com/Atvaark/DarkSoulsIII.FileFormats):
  retain the extension/type/name references. DS3 names, layouts and keys do not
  establish ACV identities or compatibility.
- [Xenia a192b94](https://github.com/Atvaark/xenia/commit/a192b94df4266bf94cf9a04b4289e0534227186a)
  fixes UTF-8 BOM config reading; it changes no GPU/texture code. Equivalent BOM
  handling was applied to our Python launcher and verified. It is not evidence
  for the dark-scene cause.
