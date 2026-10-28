#!/usr/bin/env python3
"""Check both X11 title properties on an owned launcher, without screenshots/gameplay."""
import argparse, ctypes as C, json, locale, os, subprocess, tempfile, time
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument("--launcher",type=Path,required=True);ap.add_argument("--root",type=Path,required=True);ap.add_argument("--previous",type=Path);a=ap.parse_args()
locale.setlocale(locale.LC_CTYPE,"C.UTF-8")
x=C.CDLL("libX11.so.6")
class TextProperty(C.Structure):_fields_=[("value",C.POINTER(C.c_ubyte)),("encoding",C.c_ulong),("format",C.c_int),("nitems",C.c_ulong)]
x.XOpenDisplay.argtypes=[C.c_char_p];x.XOpenDisplay.restype=C.c_void_p
x.XInternAtom.argtypes=[C.c_void_p,C.c_char_p,C.c_int];x.XInternAtom.restype=C.c_ulong
x.XGetTextProperty.argtypes=[C.c_void_p,C.c_ulong,C.POINTER(TextProperty),C.c_ulong]
x.Xutf8TextPropertyToTextList.argtypes=[C.c_void_p,C.POINTER(TextProperty),C.POINTER(C.POINTER(C.c_char_p)),C.POINTER(C.c_int)]
x.XFreeStringList.argtypes=[C.POINTER(C.c_char_p)];x.XFree.argtypes=[C.c_void_p];x.XCloseDisplay.argtypes=[C.c_void_p]
display=x.XOpenDisplay(None);assert display,"X11 unavailable"
expected="Armored Core V — Лаунчер"
def check(binary):
 with tempfile.TemporaryDirectory(prefix="acv-title-") as temp:
  env=dict(os.environ,LC_ALL="C",LANG="C",ACV_ROOT=str(a.root.resolve()),ACV_PROFILES_ROOT=temp,SDL_VIDEODRIVER="x11")
  env.pop("TRG_LAUNCHER_AUTOPLAY",None)
  with tempfile.TemporaryFile(mode="w+") as log:
   p=subprocess.Popen([str(binary.resolve())],env=env,stdout=log,stderr=log)
   try:
    deadline=time.monotonic()+15;window=None
    while time.monotonic()<deadline:
     r=subprocess.run(["xdotool","search","--onlyvisible","--pid",str(p.pid)],capture_output=True,text=True)
     if r.returncode==0:window=int(r.stdout.splitlines()[0]);break
     assert p.poll() is None,"launcher exited before title check"
     time.sleep(.05)
    assert window,"launcher window missing"
    result={}
    for name in ["WM_NAME","_NET_WM_NAME"]:
     prop=TextProperty()
     if not x.XGetTextProperty(display,window,C.byref(prop),x.XInternAtom(display,name.encode(),False)):
      result[name]=None;continue
     texts=C.POINTER(C.c_char_p)();count=C.c_int()
     try:
      assert x.Xutf8TextPropertyToTextList(display,C.byref(prop),C.byref(texts),C.byref(count))==0,name
      assert count.value>0,name
      result[name]=texts[0].decode("utf-8")
     finally:
      if texts:x.XFreeStringList(texts)
      if prop.value:x.XFree(prop.value)
    return result
   finally:
    p.terminate()
    try:p.wait(timeout=5)
    except subprocess.TimeoutExpired:p.kill();p.wait(timeout=5)
try:
 result={}
 if a.previous:result["previous"]=check(a.previous)
 result["current"]=check(a.launcher)
 assert all(v==expected for v in result["current"].values()),result
 print(json.dumps(result,ensure_ascii=False,indent=2))
finally:x.XCloseDisplay(display)
