# Bounded ACV evidence capture

With the managed Ghidra session already open:

```bash
./scripts/decomp.sh capture 0x822F4ED0 0xDC
```

Arguments are an aligned guest address and an explicit byte count (4..4000, multiple of 4). The range is an investigation window, not an asserted function boundary. The tool checks the pinned staged ACV XEX hash, expected RPC project, loaded program and initialized executable section. It performs read-only queries and writes a new timestamped snapshot under `decompilation/evidence/acv/`; logs stay in `logs/` and the wrapper's caches remain in `tmp/`. It never replaces previous exports. Original media is not accessed.

Each snapshot records raw memory and its SHA-256, the Ghidra memory map and metadata, function body bounds, xrefs to the entry, a listing restricted to the requested range, and available pseudocode. `report.json` explicitly lists missing instructions and short function bodies. The listing's bytes are compared to the captured memory bytes. Snapshot completion means evidence collection finished, not that the decompilation is complete. A complete listing also does not prove that every byte is executable code.

The source hash is for the staged file; it does not independently authenticate the open Ghidra database. The program-property image base may differ from the loader's image base; use the saved memory map for guest address validation. A Ghidra `analysis_complete` flag must not be treated as proof of correct boundaries, types, or ABI.

`GHIDRA_RPC_NO_AUTOSTART=1` is set for every capture query. The local RPC client raises on disconnection instead of starting another GUI, including a disconnection between preflight and later requests. Failure responses are retained in the snapshot where available; `report.json` records failures. Capturing a range never defines missing instructions or repairs functions.

Regression check:

```bash
python3 -B -m unittest discover -s tools/tests -p test_ghidra_capture.py -v
```

The checks cover sparse listings crossing into another function, complete bounded listings, and disconnection without automatic daemon startup. Live captures are indexed in `decompilation/evidence/acv/timing-index.json`.
