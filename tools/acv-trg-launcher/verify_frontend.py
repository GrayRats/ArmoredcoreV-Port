#!/usr/bin/env python3
"""Exercise the real frontend->bridge->probe boundary with an isolated profile.
The game session has no probe timeout; this test terminates only its own supervisor.
"""
import argparse,ctypes,hashlib,json,os,signal,subprocess,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
ap=argparse.ArgumentParser();ap.add_argument('--root',type=Path,default=ROOT);ap.add_argument('--launcher',type=Path,required=True);ap.add_argument('--runtime',type=Path);ap.add_argument('--evidence',type=Path,required=True);ap.add_argument('--seconds',type=int,default=75);a=ap.parse_args()
root=a.root.resolve();out=a.evidence.resolve();out.mkdir(parents=True,exist_ok=False)
profiles=out/'profiles';state=out/'state'
env=dict(os.environ,ACV_ROOT=str(root),ACV_PROFILES_ROOT=str(profiles),XDG_STATE_HOME=str(state),SDL_VIDEODRIVER='x11',TRG_LAUNCHER_AUTOPLAY='240')
for k in ['LD_PRELOAD','VK_INSTANCE_LAYERS','ENABLE_VULKAN_RENDERDOC_CAPTURE']:env.pop(k,None)
cli=root/'tools/native-rexglue/acv_profile_cli.py'
def call(args):return json.loads(subprocess.check_output([sys.executable,'-I',str(cli),'--profiles-root',str(profiles),*args],env=env,cwd='/',text=True))
profile=call(['create','TRG integration','--gamertag','ARTEM','--pilot','ARTEM'])
exe=(a.runtime.resolve() if a.runtime else root/'bin/game')/'acv_linux_probe'
settings={'schema_version':1,'graphics':{'resolution':'2560x1080','vsync':False,'dither':False},'launcher':{'build':str(exe),'profile':profile['uuid'],'video_driver':'x11','allow_experimental':False}}
(profiles/'settings.json').write_text(json.dumps(settings))
logpath=out/'frontend.log';supervisor=None;gamepid=None
start=time.monotonic()
with logpath.open('w') as log:
 frontend=subprocess.Popen([str(a.launcher.resolve()),'--profiles-root',str(profiles)],stdout=log,stderr=log,env=env,cwd='/')
 try:
  while time.monotonic()-start<25:
   text=logpath.read_text()
   if 'game_started pid=' in text:
    supervisor=int(text.split('game_started pid=')[-1].split()[0]);break
   assert frontend.poll() is None,text
   time.sleep(.1)
  assert supervisor,'PLAY did not launch game: '+logpath.read_text()
  children=Path(f'/proc/{supervisor}/task/{supervisor}/children')
  for _ in range(100):
   ids=children.read_text().split() if children.exists() else []
   if ids:gamepid=int(ids[0]);break
   time.sleep(.1)
  assert gamepid,'probe did not start game'
  assert Path(f'/proc/{gamepid}/exe').resolve()==exe.resolve()
  args=Path(f'/proc/{supervisor}/cmdline').read_bytes().split(b'\0')
  assert b'--session' in args and b'--seconds' not in args,args
  # Try to acquire the exact writer lock while the first game holds it.
  sys.path.insert(0,str(root/'tools/native-rexglue'));import acv_profiles
  store=acv_profiles.ProfileStore(profiles)
  try:
   with store.lock(profile['uuid']):raise AssertionError('second writer acquired running profile')
  except acv_profiles.ProfileBusy:pass
  # The child exists before runtime initialization dlopens the GPU and FFX.
  # Wait for those exact mappings rather than racing their first load.
  required=[str(exe.parent/name) for name in ('librexruntime.so','librexgpu-xenos.so','libamd_fidelityfx_vk.so')]
  map_deadline=time.monotonic()+15
  while True:
   assert Path(f'/proc/{gamepid}/exe').exists(),'game exited before plugin initialization'
   game_maps=Path(f'/proc/{gamepid}/maps').read_text()
   missing=[name for name in required if name not in game_maps]
   if not missing:break
   assert time.monotonic()<map_deadline,('selected runtime mappings missing',missing)
   time.sleep(.05)
  maps=Path(f'/proc/{frontend.pid}/maps').read_text()
  assert 'libQt' not in maps and 'libicu' not in maps,'Qt/ICU mapped in TRG'
  window=subprocess.check_output(['xdotool','search','--onlyvisible','--pid',str(frontend.pid)],text=True).splitlines()[0]
  # Send the standard WM close protocol to our exact window. Keyboard
  # injection depends on focus and may be discarded when another window opens.
  class Data(ctypes.Union):_fields_=[('b',ctypes.c_char*20),('s',ctypes.c_short*10),('l',ctypes.c_long*5)]
  class Client(ctypes.Structure):_fields_=[('type',ctypes.c_int),('serial',ctypes.c_ulong),('send_event',ctypes.c_int),('display',ctypes.c_void_p),('window',ctypes.c_ulong),('message_type',ctypes.c_ulong),('format',ctypes.c_int),('data',Data)]
  class Event(ctypes.Union):_fields_=[('client',Client),('pad',ctypes.c_long*24)]
  x=ctypes.CDLL('libX11.so.6');x.XOpenDisplay.argtypes=[ctypes.c_char_p];x.XOpenDisplay.restype=ctypes.c_void_p
  x.XInternAtom.argtypes=[ctypes.c_void_p,ctypes.c_char_p,ctypes.c_int];x.XInternAtom.restype=ctypes.c_ulong
  x.XSendEvent.argtypes=[ctypes.c_void_p,ctypes.c_ulong,ctypes.c_int,ctypes.c_long,ctypes.POINTER(Event)]
  x.XFlush.argtypes=[ctypes.c_void_p];x.XCloseDisplay.argtypes=[ctypes.c_void_p]
  display=x.XOpenDisplay(None);assert display,'Cannot open X11 display'
  try:
   event=Event();event.client.type=33;event.client.send_event=1;event.client.display=display;event.client.window=int(window);event.client.format=32
   event.client.message_type=x.XInternAtom(display,b'WM_PROTOCOLS',False)
   event.client.data.l[0]=x.XInternAtom(display,b'WM_DELETE_WINDOW',False)
   assert x.XSendEvent(display,int(window),False,0,ctypes.byref(event))
   x.XFlush(display)
  finally:x.XCloseDisplay(display)
  frontend.wait(timeout=10)
  assert frontend.returncode==0,logpath.read_text()
  assert Path(f'/proc/{gamepid}/exe').resolve()==exe.resolve(),'game ended with launcher'
  try:
   with store.lock(profile['uuid']):raise AssertionError('launcher exit released live game lock')
  except acv_profiles.ProfileBusy:pass
  # Allow runtime startup, without controlling gameplay or changing names/saves.
  while time.monotonic()-start<a.seconds:
   if not Path(f'/proc/{gamepid}/exe').exists():break
   time.sleep(.2)
  if Path(f'/proc/{supervisor}/cmdline').exists():
   current=Path(f'/proc/{supervisor}/cmdline').read_bytes()
   assert str(root/'tools/native-rexglue/probe_linux.py').encode() in current
   os.kill(supervisor,signal.SIGTERM)
  deadline=time.monotonic()+20
  while Path(f'/proc/{gamepid}/exe').exists() and time.monotonic()<deadline:time.sleep(.1)
  assert not Path(f'/proc/{gamepid}/exe').exists(),'game did not terminate'
  # The supervisor drains logs and records diagnostics after child exit.
  deadline=time.monotonic()+20
  while True:
   try:
    with store.lock(profile['uuid']):pass
    break
   except acv_profiles.ProfileBusy:
    assert time.monotonic()<deadline,'supervisor retained lock after shutdown'
    time.sleep(.1)
  deadline=time.monotonic()+10
  while time.monotonic()<deadline:
   latest=call(['last-run'])
   if latest.get('completed'):break
   time.sleep(.1)
  result={'launcher_exit':frontend.returncode,'supervisor':supervisor,'game_pid':gamepid,'window':window,'session_unbounded':True,'second_writer_rejected':True,'game_survived_launcher':True,'lock_survived_launcher':True,'lock_released_after_game':True,'no_qt_icu':True,'last_run':latest,'scope':'frontend integration/startup only; no gameplay or visual acceptance'}
  (out/'verification.json').write_text(json.dumps(result,indent=2)+'\n')
  print(json.dumps(result,indent=2))
 finally:
  if frontend.poll() is None:frontend.terminate();frontend.wait(timeout=10)
  if supervisor and Path(f'/proc/{supervisor}/cmdline').exists():
   cmd=Path(f'/proc/{supervisor}/cmdline').read_bytes()
   if str(root/'tools/native-rexglue/probe_linux.py').encode() in cmd:
    try:os.kill(supervisor,signal.SIGTERM)
    except ProcessLookupError:pass
