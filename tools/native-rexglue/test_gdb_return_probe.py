#!/usr/bin/env python3
"""Actual GDB return observations across concurrent calls and exiting workers."""
import json, os, subprocess, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix="acv-gdb-returns-") as directory:
    d=Path(directory)
    (d/"child.cpp").write_text(r'''
#include <thread>
#include <vector>
#include <chrono>
#include <pthread.h>
extern "C" __attribute__((noinline)) int observed(int n) {
 if(n<0) pthread_exit(nullptr);
 std::this_thread::yield();
 return n*3+1;
}
int main(){
 for(int batch=0;batch<20;++batch){
  std::vector<std::thread> threads;
  for(int i=0;i<8;++i) threads.emplace_back([i]{
   for(int j=0;j<12;++j) observed(i*100+j);
   observed(-1);
  });
  for(auto& t:threads)t.join();
 }
 return 0;
}
''')
    subprocess.run(["c++","-g","-O0","-pthread",str(d/"child.cpp"),"-o",str(d/"child")],check=True)
    script=r'''
import gdb,json
exec(compile(open(@HELPER@).read(),"return-helper","exec"))
count=0
class Returned(ReturnProbe):
 def __init__(self,arg):
  super().__init__();self.arg=arg
 def stop(self):
  global count
  assert int(gdb.selected_frame().read_register("eax"))==self.arg*3+1
  count+=1
class Entry(gdb.Breakpoint):
 def stop(self):
  arg=int(gdb.selected_frame().read_register("edi"))
  if arg >= 2**31:arg-=2**32
  # Duplicate observers at the same native entry must both complete.
  Returned(arg);Returned(arg)
  return False
gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("set debuginfod enabled off")
Entry("*observed",internal=True)
captured=gdb.execute("run",to_string=True)
assert "Python Exception" not in captured,captured
assert not gdb.selected_inferior().pid,captured
assert count==20*8*12*2,count
assert sum(len(s.outstanding) for s in _sites.values())==0
open(@RESULT@,"w").write(json.dumps({"returns":count,"pending":0,"exit":int(gdb.parse_and_eval("$_exitcode"))}))
'''
    script=script.replace("@HELPER@",repr(str(ROOT/"tools/native-rexglue/gdb_return_probe.py"))).replace("@RESULT@",repr(str(d/"result.json")))
    (d/"test.py").write_text(script)
    env=dict(os.environ);env["DEBUGINFOD_URLS"]=""
    result=subprocess.run(["gdb","-batch","-nx","-x",str(d/"test.py"),"--args",str(d/"child")],env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=60)
    assert result.returncode==0,result.stdout[-5000:]
    data=json.loads((d/"result.json").read_text())
    assert data["exit"]==0,data
    print("PASS actual GDB: 160 exiting threads, 3840 correlated return callbacks, abandoned pthread_exit calls removed, duplicate observers supported")
