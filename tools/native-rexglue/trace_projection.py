"""Read-only guest argument tracing, loaded inside the startup GDB driver.

Requires an executable-pinned layout compiled from its SDK PPCContext header.
Linux x86-64 System V entry ABI: rdi=context, rsi=guest memory base.
The trace is diagnostic evidence, not a camera hook or frame-rate measurement.
"""
import json
import struct
import time
from collections import Counter


def install(layout, output):
    import gdb
    offsets = layout["offsets"]
    required = {"r3", "r4", "r8", "r9", "lr", "f1", "f2", "f3", "f4"}
    if set(offsets) != required or any(type(v) is not int or not 0 <= v < 4096 for v in offsets.values()):
        raise ValueError("Invalid PPCContext trace layout")
    rows, errors, counts = [], [], Counter()
    started = time.monotonic()
    epoch = [0]
    breakpoints = []

    def save():
        output.write_text(json.dumps({
            "scope": "entry arguments and sampled return blocks; guest LR may be stale when codegen keeps it local; no gameplay/HUD attribution",
            "layout": layout, "counts": dict(counts), "samples": rows,
            "sampling_policy": {"samples_per_entry_per_window": 8, "window_seconds": 8, "maximum_windows": 8},
            "errors": errors, "elapsed_seconds": time.monotonic() - started,
        }, indent=2, allow_nan=False) + "\n")

    class Returned(gdb.FinishBreakpoint):
        def __init__(self, row, base, dest):
            super().__init__(gdb.newest_frame(), internal=True)
            self.row, self.base, self.dest = row, base, dest
        def stop(self):
            try:
                raw = bytes(gdb.selected_inferior().read_memory(self.base + self.dest, 64))
                self.row["return_block_be_hex"] = raw.hex()
            except gdb.error as exc:
                errors.append(str(exc))
            save()
            return False

    class Entry(gdb.Breakpoint):
        def __init__(self, address):
            self.address = address
            self.window_samples = 0
            # Observe the effective arguments inside a possible handwritten wrapper.
            prefix = "*__imp__sub_" if address == "82BC8980" else "*sub_"
            super().__init__(prefix + address, internal=True)
        def stop(self):
            counts[self.address] += 1
            # Rearmed by the outer GDB sampling stop, avoiding per-frame overhead.
            self.window_samples += 1
            if self.window_samples > 8:
                self.enabled = False
                return False
            try:
                ctx = int(gdb.parse_and_eval("$rdi"))
                base = int(gdb.parse_and_eval("$rsi"))
                mem = gdb.selected_inferior()
                raw = lambda k: bytes(mem.read_memory(ctx + offsets[k], 8))
                # Float hex preserves NaNs/infinities and exact host register representation.
                row = {"entry": self.address, "seconds": time.monotonic() - started,
                       "gpr": {k: struct.unpack("<Q", raw(k))[0] for k in required if not k.startswith("f")},
                       "fpr_le_hex": {k: raw(k).hex() for k in ("f1", "f2", "f3", "f4")}}
                frame = gdb.newest_frame().older()
                row["host_caller"] = frame.name() if frame else None
                row["window"] = epoch[0]
                stack = []
                while frame is not None and len(stack) < 16:
                    stack.append({"name": frame.name(), "pc": hex(frame.pc())})
                    frame = frame.older()
                row["host_stack"] = stack
                rows.append(row)
                if self.address == "82BC8980":
                    Returned(row, base, row["gpr"]["r3"] & 0xffffffff)
                save()
            except (gdb.error, ValueError) as exc:
                errors.append(str(exc)); self.enabled = False; save()
            return False

    for address in ("82302520", "82BC8980", "82303138", "82A0CA28"):
        breakpoints.append(Entry(address))

    def tick():
        window = min(8, int((time.monotonic() - started) / 8))
        if window != epoch[0]:
            epoch[0] = window
            for bp in breakpoints:
                bp.window_samples = 0
                bp.enabled = window < 8

    save.tick = tick
    save()
    return save
