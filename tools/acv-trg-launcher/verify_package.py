#!/usr/bin/env python3
"""Read-only verification of the standalone manifest and dependency resolution."""
import argparse,hashlib,json,os,re,subprocess
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument("package",type=Path);a=ap.parse_args();p=a.package.resolve()
m=json.loads((p/"manifest.json").read_text())
assert m["frontend"]=="trg"
for name,record in m["files"].items():
 f=p/name
 assert not f.is_symlink(),name
 with f.open("rb") as stream:assert hashlib.file_digest(stream,"sha256").hexdigest()==record["sha256"],name
assert not any(f.is_symlink() for f in p.rglob("*"))
assert not any("libQt" in name or "libicu" in name for name in m["files"])
env=dict(os.environ,LD_LIBRARY_PATH=str(p/"lib")+":"+str(p/"bin/game"))
resolution={}
for name in ["bin/acv-launcher",*['bin/game/'+n for n in m['runtime_hashes']]]:
 r=subprocess.run(["ldd",str(p/name)],env=env,text=True,capture_output=True,check=True)
 assert "not found" not in r.stdout,r.stdout
 for lib,location in re.findall(r"^\s*(\S+) => (/\S+)",r.stdout,re.M):
  assert "/DEArmoredcore/" not in location,(lib,location)
  assert not lib.startswith(("libQt","libicu")),lib
 resolution[name]=r.stdout
assert "QT_" not in (p/"start.sh").read_text()
print(json.dumps({"package":str(p),"files_checked":len(m["files"]),"frontend":"trg","no_qt_icu":True,"runtime_status":json.loads((p/"acv-package.json").read_text())["runtime_status"],"libraries_resolve":True},indent=2))
