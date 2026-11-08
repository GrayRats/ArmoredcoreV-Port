#!/usr/bin/env python3
"""Read-only verification of the standalone manifest and dependency resolution."""
import argparse,hashlib,json,os,re,shutil,subprocess
from pathlib import Path
# Windows packages (launcher/acv-launcher.exe): PE imports are read with objdump, not ldd.
SYSTEM_DLLS={"kernel32.dll","user32.dll","gdi32.dll","advapi32.dll","ole32.dll","oleaut32.dll","shell32.dll",
             "setupapi.dll","imm32.dll","winmm.dll","ws2_32.dll","version.dll","dxgi.dll","bcrypt.dll","ntdll.dll",
             "msvcrt.dll","shlwapi.dll","winhttp.dll","dbghelp.dll","comdlg32.dll","comctl32.dll"}
DRIVER_DLLS={"vulkan-1.dll"}  # installed by the GPU driver; not shipped in the package
def pe_imports(path):
  objdump=shutil.which("x86_64-w64-mingw32-objdump") or shutil.which("objdump")
  if not objdump: raise SystemExit("objdump is required to read PE imports")
  out=subprocess.run([objdump,"-p",str(path)],text=True,capture_output=True,check=True).stdout
  return re.findall(r"^\s*DLL Name: (\S+)",out,re.M)
def verify_windows(p):
  launcher=pe_imports(p/"launcher/acv-launcher.exe")
  assert any(d.lower()=="vulkan-1.dll" for d in launcher),("launcher does not import vulkan-1.dll",launcher)
  for name in ("launcher/acv-launcher.exe","bin/game/acv_windows_probe.exe","bin/game/librexruntime.dll","bin/game/rexgpu-xenos.dll"):
    for dll in pe_imports(p/name):
      low=dll.lower()
      assert low!="opengl32.dll",(name,"imports OpenGL: the Windows build is Vulkan-only")
      if low.startswith("api-ms-win-") or low in SYSTEM_DLLS or low in DRIVER_DLLS: continue
      assert any((p/d/dll).is_file() for d in ("launcher","bin/game")),("unresolved DLL",name,dll)
  assert not re.search(r"wine[\\/]|start-wine|wine cmd",(p/"start.cmd").read_text(encoding="ascii",errors="ignore"),re.I),"start.cmd refers to Wine"
ap=argparse.ArgumentParser();ap.add_argument("package",type=Path);a=ap.parse_args();p=a.package.resolve()
development_root=Path(__file__).resolve().parents[2]
m=json.loads((p/"manifest.json").read_text())
assert m["frontend"]=="trg"
for name,record in m["files"].items():
 f=p/name
 assert not f.is_symlink(),name
 with f.open("rb") as stream:assert hashlib.file_digest(stream,"sha256").hexdigest()==record["sha256"],name
assert not any(f.is_symlink() for f in p.rglob("*"))
assert not any("libQt" in name or "libicu" in name for name in m["files"])
if (p/"launcher/acv-launcher.exe").is_file():
  verify_windows(p)
  print(json.dumps({"package":str(p),"files_checked":len(m["files"]),"frontend":"trg","platform":"windows-x64",
                    "vulkan_only":True,"libraries_resolve":True},indent=2))
  raise SystemExit(0)
env=dict(os.environ,LD_LIBRARY_PATH=str(p/"lib")+":"+str(p/"bin/game"))
resolution={}
for name in ["bin/acv-launcher",*['bin/game/'+n for n in m['runtime_hashes']]]:
 r=subprocess.run(["ldd",str(p/name)],env=env,text=True,capture_output=True,check=True)
 assert "not found" not in r.stdout,r.stdout
 for lib,location in re.findall(r"^\s*(\S+) => (/\S+)",r.stdout,re.M):
  resolved=Path(location).resolve()
  assert resolved.is_relative_to(p) or not resolved.is_relative_to(development_root),('Development dependency outside package',lib,location)
  assert not lib.startswith(("libQt","libicu")),lib
 resolution[name]=r.stdout
assert "QT_" not in (p/"start.sh").read_text()
print(json.dumps({"package":str(p),"files_checked":len(m["files"]),"frontend":"trg","no_qt_icu":True,"runtime_status":json.loads((p/"acv-package.json").read_text())["runtime_status"],"libraries_resolve":True},indent=2))
