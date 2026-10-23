#!/usr/bin/env python3
"""Check real probe command/config precedence without starting a game process.

Requires the existing pinned Linux build and extracted XEX, as probe_linux.py does.
Popen is intercepted at the launch boundary; guest assets are only read.
"""
import contextlib
import io
from pathlib import Path
import runpy
import sys
import tempfile
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
class LaunchIntercepted(Exception):
    pass

def assembled(extra, config):
    captured = []
    def intercept(command, **kwargs):
        captured.append(command)
        raise LaunchIntercepted
    argv = [str(ROOT / "tools/native-rexglue/probe_linux.py"), "--config", str(config), *extra]
    with patch.object(sys, "argv", argv), patch("subprocess.Popen", intercept):
        try:
            runpy.run_path(argv[0], run_name="__main__")
        except LaunchIntercepted:
            pass
    assert len(captured) == 1, "Probe did not reach its launch boundary"
    return captured[0]

def selected(command):
    values = [arg.split("=", 1)[1] for arg in command if arg.startswith("--readback_resolve=")]
    return values[-1] if values else None

def main():
    config = ROOT / "tools/native-rexglue/acv.config"
    original = config.read_bytes()
    assert selected(assembled([], config)) == "full"
    assert selected(assembled(["--graphics-profile", "sdk"], config)) is None
    assert selected(assembled(["--extra=--readback_resolve=none"], config)) == "none"
    with tempfile.TemporaryDirectory(prefix="acv-readback-config-", dir=ROOT / "tmp") as temp:
        override = Path(temp) / "override.config"
        override.write_bytes(original.replace(b"[sdk]", b"[sdk]\nreadback_resolve = fast", 1))
        assert selected(assembled([], override)) == "fast"
        override.write_bytes(b"\xef\xbb\xbf" + original)
        assert selected(assembled([], override)) == "full"
        with patch.object(sys, "argv", [str(ROOT / "tools/native-rexglue/probe_linux.py"),
                                       "--config", str(Path(temp)/"missing.config")]), \
             patch("subprocess.Popen") as launch, contextlib.redirect_stderr(io.StringIO()):
            try:
                runpy.run_path(sys.argv[0], run_name="__main__")
            except SystemExit as error:
                assert error.code == 2
            else:
                raise AssertionError("Missing config did not fail")
            launch.assert_not_called()
    assert config.read_bytes() == original
    print("Passed: ACV full default, SDK unchanged, CLI/config overrides, BOM, missing-file guard; no game launched")
if __name__ == "__main__":
    main()
