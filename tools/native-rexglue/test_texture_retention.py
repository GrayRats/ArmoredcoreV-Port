#!/usr/bin/env python3
"""Actual SDK eviction body with deterministic texture lifetimes; not a VRAM/FPS benchmark."""
import hashlib,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def main():
    source=ROOT/"tools/rexglue-sdk/src/graphics/pipeline/texture/cache.cpp"
    text=source.read_text();begin=text.index("void TextureCache::CompletedSubmissionUpdated(");end=text.index("void TextureCache::BeginSubmission(",begin)
    with tempfile.TemporaryDirectory(prefix="acv-retention-") as temp:
        root=Path(temp);(root/"actual_texture_eviction.inc").write_text(text[begin:end])
        exe=root/"test"
        subprocess.run(["clang++","-std=c++23","-O2","-g","-fsanitize=address,undefined","-fno-omit-frame-pointer",
          "-I",str(root),str(ROOT/"tools/native-rexglue/texture_retention_test.cpp"),"-o",str(exe)],check=True)
        subprocess.run([str(exe)],check=True,timeout=15)
    print("SDK eviction source SHA256",hashlib.sha256(source.read_bytes()).hexdigest())
if __name__=="__main__":main()
