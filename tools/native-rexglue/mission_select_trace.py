"""GDB read-only trace of scene creation; source through hang_watch --diagnostics."""
import gdb, json, os, time
from pathlib import Path
cfg = json.loads(os.environ["ACV_HANG_CFG"])
exec(compile(Path(cfg["diagnostics"]).with_name("gdb_return_probe.py").read_text(), "gdb-return-probe", "exec"))
output = Path(cfg["out"]) / "scene-create.jsonl"
active = {}
rows = 0
limit = 6000

def emit(data):
    global rows
    if rows >= limit:
        return
    rows += 1
    data["time"] = time.time()
    with output.open("a") as stream:
        stream.write(json.dumps(data) + "\n")

def reg(ctx, offset):
    return int.from_bytes(gdb.selected_inferior().read_memory(ctx + offset, 8).tobytes(), "little") & 0xffffffff

def fields(base, ptr, offsets):
    if not ptr:
        return {}
    result = {}
    for offset in offsets:
        try:
            result[hex(offset)] = hex(int.from_bytes(gdb.selected_inferior().read_memory(base + ptr + offset, 4).tobytes(), "big"))
        except gdb.MemoryError:
            result[hex(offset)] = "unreadable"
    return result

class Returned(ReturnProbe):
    def __init__(self, name, ctx, base, tid, obj, outer):
        super().__init__(internal=True)
        self.name, self.ctx, self.base, self.tid, self.obj, self.outer = name, ctx, base, tid, obj, outer
    def stop(self):
        result = reg(self.ctx, 0)
        data = {"event": "return", "function": self.name, "thread": self.tid, "result": hex(result)}
        if self.name in ("828DB268", "828E71A8", "8290B6E8"):
            data["object"] = hex(self.obj)
            data["fields"] = fields(self.base, self.obj, [108,112,120,124,128,484])
        emit(data)
        if self.outer:
            active.pop(self.tid, None)
        return False

class Entered(gdb.Breakpoint):
    def __init__(self, name):
        super().__init__("*__imp__sub_" + name, internal=True)
        self.name = name
    def stop(self):
        tid = gdb.selected_thread().global_num
        outer = self.name == "82753C48"
        if not outer and tid not in active:
            return False
        if rows >= limit:
            return False
        ctx, base = int(gdb.parse_and_eval("$rdi")), int(gdb.parse_and_eval("$rsi"))
        if outer:
            active[tid] = True
        obj, arg = reg(ctx,0), reg(ctx,32)
        data = {"event": "enter", "function": self.name, "thread":tid, "r3":hex(obj), "r4":hex(arg), "lr":hex(reg(ctx,256))}
        if self.name == "828E8220":
            try:
                inf = gdb.selected_inferior()
                descriptor = int.from_bytes(inf.read_memory(base + obj + 4, 4).tobytes(), "big")
                pointer = int.from_bytes(inf.read_memory(base + descriptor + 596, 4).tobytes(), "big")
                data["resource_name"] = inf.read_memory(base + pointer, 128).tobytes().split(b"\0")[0].decode(errors="replace") if pointer else ""
            except gdb.MemoryError:
                data["resource_name"] = "unreadable"
        emit(data)
        Returned(self.name, ctx, base, tid, obj, outer)
        return False

for function in ("82753C48", "82753448", "82732868", "82751AD8", "828DB650", "828DB268", "8290B6E8", "828E71A8", "828E8220", "828E82B8", "8294B860", "8294B6C0", "82C33090", "825E34E0", "82A20498", "82A205B0", "824E2550", "82776540"):
    Entered(function)
emit({"event":"trace_ready", "scope":"read-only scene factory arguments and returns", "limit":limit})
