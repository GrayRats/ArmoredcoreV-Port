"""Bounded read-only storage API observer; no per-frame/input breakpoints."""
import gdb, json, os, time
from pathlib import Path
cfg=json.loads(os.environ["ACV_HANG_CFG"])
exec(compile(Path(cfg["diagnostics"]).with_name("gdb_return_probe.py").read_text(), "gdb-return-probe", "exec"))
out=Path(cfg["out"])/"storage.jsonl"
def emit(row):
    row["time"]=time.time()
    with out.open("a") as f:f.write(json.dumps(row)+"\n")
def mem(base,p,n):
    return gdb.selected_inferior().read_memory(base+p,n).tobytes()
def u32(base,p):return int.from_bytes(mem(base,p,4),"big")
def reg(ctx,n):
    offset=0 if n==3 else 32+(n-4)*8
    return int(gdb.parse_and_eval(f"*(unsigned long long*){ctx+offset}"))&0xffffffff
def cstr(base,p):
    return mem(base,p,256).split(b"\0")[0].decode(errors="replace") if p else ""
def buffers(name,a):
    if name=="XamContentGetDeviceData":return [(a[1],80)]
    if name=="NtQueryVolumeInformationFile":return [(a[1],8),(a[2],min(a[3],80))]
    if name=="XamShowDeviceSelectorUI":return [(a[4],4),(a[5],32)]
    if name=="NtCreateFile":return [(a[0],4),(a[3],8)]
    if name.startswith("XamContentCreate"):return [(a[4],4),(a[5],4),(a[6],32)] if name=="XamContentCreate" else []
    return []
class Returned(ReturnProbe):
    def __init__(self,ctx,base,row,bufs):
        super().__init__(internal=True)
        self.ctx,self.base,self.row,self.bufs=ctx,base,row,bufs
    def stop(self):
        row=dict(self.row,event="return",status=hex(reg(self.ctx,3)))
        row["buffers"]={}
        for p,n in self.bufs:
            if p:
                try:row["buffers"][hex(p)]=mem(self.base,p,n).hex()
                except gdb.MemoryError:row["buffers"][hex(p)]="unreadable"
        emit(row)
        return False
class Call(gdb.Breakpoint):
    def __init__(self,name):
        self.name=name;self.hits=0
        super().__init__("*__imp__"+name,internal=True)
    def stop(self):
        ctx=int(gdb.parse_and_eval("$rdi"));base=int(gdb.parse_and_eval("$rsi"))
        a=[reg(ctx,n) for n in range(3,11)]
        row={"api":self.name,"args":[hex(x) for x in a],
             "lr":hex(int(gdb.parse_and_eval(f"*(unsigned long long*){ctx+256}"))&0xffffffff)}
        try:
            if self.name=="NtCreateFile":
                name=u32(base,a[2]+4)
                path=cstr(base,u32(base,name+4))
                if not path.lower().startswith(("con:","save:","hdd:")):return False
                row["path"]=path
            elif self.name.startswith("XamContentCreate") and self.name!="XamContentCreateDeviceEnumerator":
                row["root"]=cstr(base,a[1])
                row["content"]=mem(base,a[2],min(0x134,0x134)).hex()
            emit(dict(row,event="call"))
            Returned(ctx,base,row,buffers(self.name,a))
        except gdb.MemoryError:emit(dict(row,event="unreadable"))
        self.hits+=1
        if self.hits>=100:self.enabled=False
        return False
class Ready(gdb.Breakpoint):
    def __init__(self):super().__init__("*main",internal=True,temporary=True)
    def stop(self):
        for name in ("XamContentGetDeviceData","XamContentGetDeviceState",
                     "XamShowDeviceSelectorUI","XamContentCreate","XamContentCreateEx",
                     "XamContentClose","XamContentGetCreator","NtQueryVolumeInformationFile",
                     "NtCreateFile"):Call(name)
        emit({"event":"ready"})
        return False
Ready()
