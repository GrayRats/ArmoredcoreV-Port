#!/usr/bin/env python3
"""Run the actual build wrapper against simulated old SDK GPU/runtime staging."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
source=Path(__file__).with_name("build_linux.py")
with tempfile.TemporaryDirectory(prefix="acv-gpu-retain-") as temporary:
    root=Path(temporary);tools=root/"tools/native-rexglue";tools.mkdir(parents=True)
    shutil.copy2(source,tools/"build_linux.py")
    (tools/"CMakeLists.linux-probe.txt").write_text("fixture")
    (tools/"acv.config").write_text("fixture")
    project=root/"tmp/acv-rexglue-linux-indexed";project.mkdir(parents=True)
    (project/"CMakeLists.txt").write_text("fixture")
    build=project/"out/build/linux-release";build.mkdir(parents=True)
    (build/"acv_build_info.h").write_text("fixture")
    for name in ("acv_linux_probe","librexruntime.so"):(build/name).write_bytes(b"\x7fELFfixture")
    gpu=root/"tmp/accepted.so";gpu.write_bytes(b"\x7fELFaccepted-texture-fix")
    runtime=root/"tmp/accepted-runtime.so";runtime.write_bytes(b"\x7fELFaccepted-keyboard-fix")
    bin_dir=root/"bin";bin_dir.mkdir()
    cmake=bin_dir/"cmake"
    cmake.write_text("#!"+sys.executable+"\n"+r'''import os,sys
from pathlib import Path
build=Path(sys.argv[sys.argv.index("--build")+1])
(build/"cmake-ran").write_text("yes")
(build/"librexgpu-xenos.so").write_bytes(b"\x7fELFold-installed-SDK")
if os.environ.get("ACV_TEST_MUTATE"):
 Path(os.environ["ACV_TEST_MUTATE"]).write_bytes(b"\x7fELFchanged-during-build")
''')
    cmake.chmod(0o755)
    env=dict(os.environ,PATH=str(bin_dir)+os.pathsep+os.environ["PATH"])
    def run(*args,ok=True,extra=None):
        result=subprocess.run([sys.executable,str(tools/"build_linux.py"),"--jobs","1",*args],env=dict(env,**(extra or {})),capture_output=True,text=True)
        assert (result.returncode==0)==ok,result.stdout+result.stderr
        return result
    def manifest():return json.loads((build/"acv-test-build.json").read_text())
    def correct():assert (build/"librexgpu-xenos.so").read_bytes()==gpu.read_bytes()
    def correct_runtime():assert (build/"librexruntime.so").read_bytes()==runtime.read_bytes()
    run("--gpu-plugin-artifact",str(gpu),"--runtime-artifact",str(runtime));correct();correct_runtime()
    assert manifest()["gpu_plugin_selection"]=="explicit"
    assert manifest()["runtime_selection"]=="explicit"
    run();correct();correct_runtime();assert manifest()["gpu_plugin_selection"]=="retained_manifest"
    assert manifest()["runtime_selection"]=="retained_manifest"
    (build/"cmake-ran").unlink();gpu.write_bytes(b"\x7fELFnew-unverified-plugin")
    result=run(ok=False)
    assert "Recorded GPU artifact changed" in result.stderr and not (build/"cmake-ran").exists()
    run("--gpu-plugin-artifact",str(gpu));correct();correct_runtime()
    assert manifest()["gpu_plugin_override"]["sha256"]==hashlib.sha256(gpu.read_bytes()).hexdigest()
    result=run(ok=False,extra={"ACV_TEST_MUTATE":str(gpu)})
    assert "changed during host build" in result.stderr
    run("--gpu-plugin-artifact",str(gpu));correct();correct_runtime()
    runtime.write_bytes(b"\x7fELFnew-unverified-runtime")
    result=run(ok=False)
    assert "Recorded runtime artifact changed" in result.stderr
    run("--gpu-plugin-artifact",str(gpu),"--runtime-artifact",str(runtime));correct_runtime()
    result=run(ok=False,extra={"ACV_TEST_MUTATE":str(runtime)})
    assert "Selected runtime artifact changed during host build" in result.stderr
    # Mutating the origin after copying must not change the identity recorded
    # for the staged bytes, or the next build could trust an unverified plugin.
    wrapper=root/"mutate_after_copy.py"
    wrapper.write_text(r'''import runpy,shutil,sys
from pathlib import Path
old=shutil.copy2
def copy(src,dst,*a,**k):
 result=old(src,dst,*a,**k)
 if Path(src).name=="accepted.so":Path(src).write_bytes(b"\x7fELFmutated-after-copy")
 if Path(src).name=="accepted-runtime.so":Path(src).write_bytes(b"\x7fELFmutated-after-copy")
 return result
shutil.copy2=copy
script=sys.argv[1];sys.argv=sys.argv[1:];runpy.run_path(script,run_name="__main__")
''')
    accepted=gpu.read_bytes();accepted_runtime=runtime.read_bytes()
    result=subprocess.run([sys.executable,str(wrapper),str(tools/"build_linux.py"),"--jobs","1","--gpu-plugin-artifact",str(gpu),"--runtime-artifact",str(runtime)],env=env,capture_output=True,text=True)
    assert result.returncode==0,result.stdout+result.stderr
    assert (build/"librexgpu-xenos.so").read_bytes()==accepted
    assert (build/"librexruntime.so").read_bytes()==accepted_runtime
    assert manifest()["gpu_plugin_override"]["sha256"]==hashlib.sha256(accepted).hexdigest()
    assert manifest()["runtime_override"]["sha256"]==hashlib.sha256(accepted_runtime).hexdigest()
    assert gpu.read_bytes()!=accepted
    assert runtime.read_bytes()!=accepted_runtime
    result=run(ok=False)
    assert "Recorded GPU artifact changed" in result.stderr
    runtime.write_bytes(accepted_runtime)
    gpu.write_bytes(accepted)
    print("PASS: explicit/default retention for GPU and runtime, reject changed/mid-build artifacts, explicit reselection, and preserve staged hashes after origin mutation")
