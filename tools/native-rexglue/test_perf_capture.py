#!/usr/bin/env python3
"""Exercise capture against a real host process; never launches ACV."""
import json
import contextlib
import io
import subprocess
import sys
import tempfile
import threading
from pathlib import Path

from perf_capture import capture, find_process, artifact_records

HERE = Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix="acv-capture-host-") as directory:
    root = Path(directory)
    runtime = root / "runtime.log"
    runtime.write_text("host fixture only; no gameplay measured\n")
    (root / "launch.json").write_text('{"scope": "host fixture only"}')
    # Synthetic filesystem device alias must remain explicit; inode mismatch rejects.
    library = root / "librex-fixture.so"
    library.write_bytes(b"host-only identity fixture")
    inode = library.stat().st_ino
    records = artifact_records(Path(sys.executable), f"1000-2000 r-xp 0000 ff:ff {inode} {library}")
    record = next(r for r in records if r["path"] == str(library))
    assert record["device_matches"] is False and record["mapped_devices"] == ["ff:ff"]
    try:
        artifact_records(Path(sys.executable), f"1000-2000 r-xp 0000 ff:ff {inode+1} {library}")
    except RuntimeError:
        pass
    else:
        raise AssertionError("Mismatched inode accepted")
    child = subprocess.Popen([sys.executable, "-c", "while True: pass",
                              "--log_file=" + str(runtime)])
    try:
        executable = Path("/proc") / str(child.pid) / "exe"
        executable = executable.resolve(strict=True)
        assert find_process(executable, child.pid) == child.pid
        try:
            find_process(Path("/bin/true"), child.pid)
        except RuntimeError:
            pass
        else:
            raise AssertionError("Wrong executable accepted")
        output = root / "capture"
        code = capture(child.pid, executable, output, "HOST FIXTURE ONLY", 3, 0)
        session = json.loads((output / "session.json").read_text())
        rows = [json.loads(line) for line in (output / "cpu_gpu/samples.jsonl").read_text().splitlines()]
        assert len(rows) >= 2 and rows[-1]["elapsed"] >= 1, rows
        assert session["pid"] == child.pid and session["scene"] == "HOST FIXTURE ONLY"
        assert session["artifacts_before"] == session["artifacts_after"]
        assert all(len(a["sha256"]) == 64 for a in session["artifacts_before"])
        assert (output / "runtime.log").read_bytes() == runtime.read_bytes()
        assert json.loads((output / "launch.json").read_text())["scope"] == "host fixture only"
        cpu_gate = json.loads((output / "cpu_gpu/gate.json").read_text())
        ips_gate = json.loads((output / "ips/gate.json").read_text())
        assert cpu_gate["process_start_ticks"] == ips_gate["process_start_ticks"] == session["gate"]["process_start_ticks"]
        assert abs(cpu_gate["sampling_started_age_s"] - ips_gate["sampling_started_age_s"]) < 1
        assert (code == 0) == session["instrumentation_complete"]
        ips = json.loads((output / "ips/samples.json").read_text())
        if ips["unavailable"]:
            assert code != 0 and any("unavailable" in p for p in session["problems"])
        assert child.poll() is None, "Capture stopped the observed process"
        try:
            capture(child.pid, executable, output, "HOST FIXTURE ONLY", 3, 0)
        except FileExistsError:
            pass
        else:
            raise AssertionError("Existing evidence overwritten")
        assert json.loads((output / "session.json").read_text()) == session
        rejected = subprocess.run([sys.executable, str(HERE / "perf_capture.py"),
                                   "--after-start", "0", "--scene", "host",
                                   "--output", str(root / "rejected")], capture_output=True, text=True)
        assert rejected.returncode == 2 and not (root / "rejected").exists()
        print("PASS real concurrent profilers, same PID/start, hashes/log snapshot, explicit availability, no game signals, evidence overwrite and <90s CLI rejected; host-only")
    finally:
        child.terminate()
        child.wait()

    short = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(30)"])
    timer = threading.Timer(.5, short.terminate)
    try:
        executable = (Path("/proc") / str(short.pid) / "exe").resolve(strict=True)
        timer.start()
        output = root / "ended-process"
        diagnostics = io.StringIO()
        with contextlib.redirect_stdout(diagnostics), contextlib.redirect_stderr(diagnostics):
            code = capture(short.pid, executable, output, "HOST EARLY EXIT", 3, 0)
        assert code == 1, diagnostics.getvalue()
        assert "instrumentation_complete" in diagnostics.getvalue() or "Capture failed" in diagnostics.getvalue()
        session = json.loads((output / "session.json").read_text())
        assert not session["instrumentation_complete"] and any("identity changed" in p or "Capture failed" in p for p in session["problems"])
        assert (output / "cpu_gpu.log").exists() and (output / "ips.log").exists()
        print("PASS observed host exit is a failed capture with preserved evidence")
    finally:
        timer.cancel()
        if short.poll() is None:
            short.terminate()
        short.wait()
