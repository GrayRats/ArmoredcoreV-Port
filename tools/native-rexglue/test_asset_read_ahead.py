#!/usr/bin/env python3
"""Test actual SDK PosixFileHandle and background archive advice without retail files."""
import subprocess,tempfile,hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
SDK=ROOT/"tools/rexglue-sdk/src/core/filesystem_posix.cpp"
def main():
    source=SDK.read_text()
    start=source.index("class PosixFileHandle : public FileHandle {")
    end=source.index("std::unique_ptr<FileHandle> FileHandle::OpenExisting",start)
    with tempfile.TemporaryDirectory(prefix="acv-readahead-") as temp:
        root=Path(temp);(root/"posix_filehandle_actual.inc").write_text(source[start:end])
        exe=root/"test"
        subprocess.run(["clang++","-std=c++23","-O2","-g","-pthread","-fsanitize=address,undefined",
          "-fno-omit-frame-pointer","-I",str(root),"-I",str(ROOT/"tools/native-rexglue"),
          str(ROOT/"tools/native-rexglue/asset_read_ahead_test.cpp"),"-o",str(exe)],check=True)
        subprocess.run([str(exe),str(root)],timeout=20,check=True)
    print(json.dumps({"sdk_source_sha256":hashlib.sha256(SDK.read_bytes()).hexdigest(),"retail_assets":"not accessed"}))
if __name__=="__main__":main()
