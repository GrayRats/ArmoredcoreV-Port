#!/usr/bin/env python3
# Actual GDB follow/exec/detach, including an inherited profile flock.
import importlib.util,json,os,signal,subprocess,sys,tempfile,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(Path(__file__).parent))
import acv_profiles
spec=importlib.util.spec_from_file_location('hang_watch',Path(__file__).with_name('hang_watch.py'))
watch=importlib.util.module_from_spec(spec);spec.loader.exec_module(watch)
with tempfile.TemporaryDirectory(prefix='acv-watch-supervised-') as directory:
 d=Path(directory);source=d/'child.cpp';exe=d/'child';log=d/'runtime.log'
 source.write_text('#include <signal.h>\n#include <unistd.h>\n#include <fstream>\nvolatile sig_atomic_t live=1;void stop(int){live=0;}\nint main(int n,char** v){signal(SIGTERM,stop);std::ofstream(v[1])<<"ready\\n";while(live)usleep(10000);return 0;}\n')
 subprocess.run(['c++',str(source),'-o',str(exe)],check=True)
 store=acv_profiles.ProfileStore(d/'profiles');profile=store.create('Observer test')
 fake=d/'probe.py'
 fake.write_text('''import os,sys,json,subprocess
from pathlib import Path
sys.path.insert(0,sys.argv[1])
import acv_profiles
store=acv_profiles.ProfileStore(Path(sys.argv[2]));lock=store.lock(sys.argv[3]).acquire()
child=subprocess.Popen([sys.argv[4],sys.argv[5]],pass_fds=(lock.fd,))
Path(sys.argv[6]).write_text(json.dumps({"supervisor":os.getpid(),"child":child.pid}))
child.wait();lock.release()
''')
 out=d/'watch';out.mkdir();(out/'dump_now').touch();inner=d/'inner.py';inner.write_text(watch.INNER)
 cfg={'out':str(out),'log':str(log),'idle':90,'executable':str(exe)}
 env=dict(os.environ,ACV_HANG_CFG=json.dumps(cfg));env.pop('ACV_PROBE_DRYRUN',None)
 command=['gdb','-batch','-nx','-x',str(inner),'--args',sys.executable,'-I',str(fake),str(ROOT/'tools/native-rexglue'),str(store.root),profile.uuid,str(exe),str(log),str(d/'owned.json')]
 with (d/'gdb.log').open('w') as captured:
  result=subprocess.run(command,env=env,stdout=captured,stderr=subprocess.STDOUT,text=True,timeout=30)
 result.stdout=(d/'gdb.log').read_text()
 owned=json.loads((d/'owned.json').read_text()) if (d/'owned.json').exists() else {}
 try:
  assert result.returncode==0,result.stdout[-8000:]
  child=owned['child'];parent=owned['supervisor']
  assert (out/'bt.txt').is_file(),result.stdout
  assert json.loads((out/'observed-child.json').read_text())['pid']==child
  assert Path(f'/proc/{child}/exe').resolve()==exe
  links=[]
  for fd in Path(f'/proc/{child}/fd').iterdir():
   try:links.append(str(fd.resolve()))
   except OSError:pass
  assert str(store.lock(profile.uuid).path) in links,links
  os.kill(parent,signal.SIGKILL)
  try:
   with store.lock(profile.uuid):raise AssertionError('Child did not retain writer lock after supervisor death')
  except acv_profiles.ProfileBusy:pass
  os.kill(child,signal.SIGTERM)
  deadline=time.monotonic()+5
  while Path(f'/proc/{child}/exe').exists() and time.monotonic()<deadline:time.sleep(.05)
  with store.lock(profile.uuid):pass
  print('PASS actual GDB follows supervisor exec, captures stacks, detaches live child; inherited flock survives supervisor death and releases after child exit')
  # A callback failure must not become an apparently successful diagnostic run.
  out=d/'watch-callback-failure';out.mkdir()
  diag=d/'bad-callback.py'
  diag.write_text('import gdb\nclass Broken(gdb.Breakpoint):\n def stop(self):raise RuntimeError("intentional test callback failure")\nBroken("*main",internal=True)\n')
  cfg.update(out=str(out),diagnostics=str(diag))
  env['ACV_HANG_CFG']=json.dumps(cfg)
  with (d/'gdb-failure.log').open('w') as captured:
   failed=subprocess.run(command,env=env,stdout=captured,stderr=subprocess.STDOUT,text=True,timeout=30)
  owned=json.loads((d/'owned.json').read_text())
  assert failed.returncode==1,(d/'gdb-failure.log').read_text()
  assert (out/'observer-error.json').is_file()
  assert 'intentional test callback failure' in (out/'gdb-run.txt').read_text()
  child=owned['child']
  assert Path(f'/proc/{child}/exe').resolve()==exe
  try:
   with store.lock(profile.uuid):raise AssertionError('Failed observer released live writer lock')
  except acv_profiles.ProfileBusy:pass
  os.kill(child,signal.SIGTERM)
  deadline=time.monotonic()+5
  while Path(f'/proc/{child}/exe').exists() and time.monotonic()<deadline:time.sleep(.05)
  with store.lock(profile.uuid):pass
  print('PASS diagnostic callback failure returns 1 with evidence, detaches live child and preserves its profile lock')

 finally:
  for key in ['supervisor','child']:
   pid=owned.get(key)
   if not pid:continue
   try:
    args=Path(f'/proc/{pid}/cmdline').read_bytes()
    if str(d).encode() in args:os.kill(pid,signal.SIGTERM)
   except (OSError,ProcessLookupError):pass
