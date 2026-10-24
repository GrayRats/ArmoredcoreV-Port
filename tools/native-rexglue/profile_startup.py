#!/usr/bin/env python3
"""Sample a canonical probe launch with GDB (X11, clean /dev/shm required).

The outer driver delegates all configuration to probe_linux.py. GDB writes samples.json
next to launch.json/runtime.log. Sampling adds overhead and is not an FPS benchmark.
Usage: python3 profile_startup.py --seconds 120 [probe options]
"""
import json, os, subprocess, sys, time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

def inner():
    import gdb, signal, threading
    cfg = json.loads(os.environ["ACV_PROFILE_CFG"])
    gdb.execute("set pagination off"); gdb.execute("set confirm off")
    gdb.execute("set print thread-events off")
    gdb.execute("set debuginfod enabled off")
    for sig in ("SIGSEGV", "SIGBUS", "SIGUSR1", "SIGUSR2", "SIG34", "SIG35", "SIG36", "SIGALRM", "SIGPIPE"):
        gdb.execute(f"handle {sig} nostop noprint pass")
    samples = []          # (t, {thread_name: [frames]})
    gdb.execute("starti")  # stops at the first instruction; we only need the pid
    pid = gdb.selected_inferior().pid
    t0 = time.monotonic()
    projection = None
    if os.environ.get("ACV_PROJECTION_LAYOUT"):
        import runpy
        projection = runpy.run_path(str(Path(__file__).with_name("trace_projection.py")))["install"](
            json.loads(os.environ["ACV_PROJECTION_LAYOUT"]), Path(cfg["out"]).with_name("projection.json"))
    def interrupt():
        try: os.kill(pid, signal.SIGINT)
        except ProcessLookupError: pass
    while time.monotonic() - t0 < cfg["seconds"]:
        timer = threading.Timer(cfg["interval"], interrupt); timer.start()
        try:
            gdb.execute("continue", to_string=True)
        except gdb.error:
            break
        timer.cancel()
        if not gdb.selected_inferior().threads():
            break
        snap = {}
        for th in gdb.selected_inferior().threads():
            th.switch()
            frames, f = [], gdb.newest_frame()
            while f is not None and len(frames) < cfg["depth"]:
                frames.append(f.name() or hex(f.pc())); f = f.older()
            snap[f"{th.num}:{th.name}"] = frames
        samples.append((round(time.monotonic() - t0, 2), snap))
        if projection is not None:
            projection.tick()
    import collections
    tops = collections.Counter(frames[0] for _, threads in samples
                               for frames in threads.values() if frames)
    Path(cfg["out"]).write_text(json.dumps({"samples": samples,
        "sample_count": len(samples), "top_frames": tops.most_common(),
        "elapsed_seconds": time.monotonic() - t0}, indent=2))
    if projection is not None:
        projection()
    try: gdb.execute("kill")
    except gdb.error: pass
    gdb.execute("quit")

if "gdb" in sys.modules or __name__ == "__main__" and os.environ.get("ACV_PROFILE_INNER"):
    inner()
elif __name__ == "__main__":
    # Reuse the canonical config/profile/input/compatibility path instead of a divergent launch.
    sys.exit(subprocess.call([sys.executable, "-I", str(Path(__file__).with_name("probe_linux.py")),
                              "--startup-profile", "--video-driver", "x11", *sys.argv[1:]]))
