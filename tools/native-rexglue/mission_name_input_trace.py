"""Read-only model-name registration, factory and observed controller packets."""
import gdb,json,os,time
from pathlib import Path
# Reuse the accepted bounded scene-creation observer without changing guest data.
exec(compile((Path(json.loads(os.environ["ACV_HANG_CFG"])["diagnostics"]).with_name("mission_select_trace.py")).read_text(),"scene-observer","exec"))
name_log=Path(cfg["out"])/"names-input.jsonl"
name_rows=0
input_last=None
input_time=0.0
input_polls=0

def record(data):
    global name_rows
    if name_rows>=1600:return
    data["time"]=time.time();name_rows+=1
    with name_log.open("a") as stream:stream.write(json.dumps(data)+"\n")
def memory(base,pointer,length):
    return gdb.selected_inferior().read_memory(base+pointer,length).tobytes()
def word(base,pointer):return int.from_bytes(memory(base,pointer,4),"big")
def cstring(base,pointer):
    try:return memory(base,pointer,128).split(b"\0")[0].decode(errors="replace") if pointer else ""
    except gdb.MemoryError:return "unreadable"

class NameReturn(ReturnProbe):
    def __init__(self,ctx,base,out,args):
        super().__init__(internal=True)
        self.ctx,self.base,self.out,self.args=ctx,base,out,args
    def stop(self):
        pointer=word(self.base,self.out+4)
        record(dict(self.args,event="name_return",name=cstring(self.base,pointer),result=hex(reg(self.ctx,0))))
        return False
class NameEntry(gdb.Breakpoint):
    def __init__(self):super().__init__("*__imp__sub_825E34E0",internal=True)
    def stop(self):
        ctx,base=int(gdb.parse_and_eval("$rdi")),int(gdb.parse_and_eval("$rsi"))
        out,source=reg(ctx,0),reg(ctx,32)
        try:
            pointer=word(base,source+26544)
            data={"source":hex(source),"type":word(base,source+4),"index":word(base,source+8),"wide":memory(base,pointer,128).hex() if pointer else "","lr":hex(reg(ctx,256))}
            record(dict(data,event="name_enter"));NameReturn(ctx,base,out,data)
        except gdb.MemoryError:record({"event":"name_unreadable"})
        return False
class RegisterName(gdb.Breakpoint):
    def __init__(self,func):
        self.func=func;super().__init__("*__imp__sub_"+func,internal=True)
    def stop(self):
        ctx,base=int(gdb.parse_and_eval("$rdi")),int(gdb.parse_and_eval("$rsi"))
        try:
            arg=reg(ctx,32)
            # Model maps use the bounded-string wrapper, whose c_str pointer is +4.
            record({"event":"model_map","function":self.func,"name":cstring(base,arg) if self.func=="82A20A48" else cstring(base,word(base,arg+4)),"arg":hex(arg),"r5":hex(reg(ctx,40)),"lr":hex(reg(ctx,256))})
        except gdb.MemoryError:record({"event":"map_unreadable","function":self.func})
        return False
class InputReturn(ReturnProbe):
    def __init__(self,ctx,base,pointer):
        super().__init__(internal=True);self.ctx,self.base,self.pointer=ctx,base,pointer
    def stop(self):
        global input_last,input_time,input_polls
        input_polls+=1;status=reg(self.ctx,0)
        data=memory(self.base,self.pointer,16) if self.pointer and status==0 else b""
        key=(status,data[4:])
        now=time.monotonic()
        if key!=input_last or now-input_time>5:
            record({"event":"input","status":status,"state":data.hex(),"buttons":int.from_bytes(data[4:6],"big") if data else None,"polls":input_polls})
            input_last=key;input_time=now
        return False
class InputEntry(gdb.Breakpoint):
    def __init__(self):
        self.polls=0;super().__init__("*__imp__XamInputGetState",internal=True)
    def stop(self):
        ctx,base=int(gdb.parse_and_eval("$rdi")),int(gdb.parse_and_eval("$rsi"))
        if reg(ctx,0)==0 or (reg(ctx,0)&255)==255:InputReturn(ctx,base,reg(ctx,40))
        self.polls+=1
        if self.polls>=600:self.enabled=False
        return False
NameEntry()
for function in ("82A21318","82A20E40","82A20A48"):RegisterName(function)
def wide(base,pointer):
    if not pointer:return ""
    try:
        raw=memory(base,pointer,2048)
        units=[]
        for i in range(0,len(raw),2):
            unit=raw[i:i+2]
            if unit==b"\0\0":break
            units.append(unit)
        return b"".join(units).decode("utf-16-be",errors="replace")
    except gdb.MemoryError:return "unreadable"
class Dialog(gdb.Breakpoint):
    def __init__(self,name):
        self.name=name;super().__init__("*__imp__"+name,internal=True)
    def stop(self):
        ctx,base=int(gdb.parse_and_eval("$rdi")),int(gdb.parse_and_eval("$rsi"))
        if self.name=="XamShowMessageBoxUI":
            record({"event":"message","title":wide(base,reg(ctx,32)),"text":wide(base,reg(ctx,40))})
        else:
            record({"event":"keyboard","default":wide(base,reg(ctx,40)),"title":wide(base,reg(ctx,48)),"description":wide(base,reg(ctx,56))})
        return False
class RuntimeReady(gdb.Breakpoint):
    def __init__(self):super().__init__("*main",internal=True,temporary=True)
    def stop(self):

        if os.environ.get("ACV_GDB_TRACE_INPUT")=="1":InputEntry()
        Dialog("XamShowMessageBoxUI");Dialog("XamShowKeyboardUI")
        record({"event":"runtime_observers_ready"})
        return False
RuntimeReady()
record({"event":"name_input_trace_ready","scope":"read-only; no synthetic packets or guest writes"})
