#!/usr/bin/env python3
"""Sampling profile of the game's start-up under gdb (no perf/strace on this host).

Runs the pinned executable as a child of gdb (ptrace_scope=1 forbids attaching to unrelated
processes), interrupts it every --interval seconds, records the top frames of every thread and
writes a JSON report. Guest functions appear as sub_82XXXXXX. Output: logs/acv-startup-profile/<id>/
Usage: python3 profile_startup.py --seconds 120   (outer driver)   |   gdb -batch -x <this> (inner)"""
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
    Path(cfg["out"]).write_text(json.dumps({"samples": samples}))
    try: gdb.execute("kill")
    except gdb.error: pass
    gdb.execute("quit")

if "gdb" in sys.modules or __name__ == "__main__" and os.environ.get("ACV_PROFILE_INNER"):
    inner()
elif __name__ == "__main__":
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--seconds", type=int, default=120)
    ap.add_argument("--interval", type=float, default=1.0)
    ap.add_argument("--depth", type=int, default=10)
    ap.add_argument("--executable", type=Path, default=ROOT / "tmp/acv-rexglue-linux-indexed/out/build/linux-release/acv_linux_probe")
    args = ap.parse_args()
    out = ROOT / "logs/acv-startup-profile" / time.strftime("%Y%m%dT%H%M%S"); out.mkdir(parents=True)
    user = ROOT / "tmp/acv-profile-user"; user.mkdir(exist_ok=True)
    cmd = [str(args.executable), f"--game_data_root={ROOT/'tmp/iso_root'}", f"--user_data_root={user}",
           f"--cache_root={user/'cache'}", "--gpu_plugin=xenos", f"--log_file={out/'runtime.log'}",
           "--protect_zero=false", "--render_target_path_vulkan=fsi", "--readback_resolve=full", "--headless=true"]
    env = dict(os.environ, ACV_PROFILE_INNER="1", ACV_PILOT_NAME="ARTEM", ACV_GAMERTAG="ARTEM",
               ACV_PROFILE_CFG=json.dumps({"seconds": args.seconds, "interval": args.interval,
                                           "depth": args.depth, "out": str(out / "samples.json")}))
    for name, var in (("acv-dlc-sound", "ACV_DLC_SOUND_ROOT"), ("acv-dlc-image", "ACV_DLC_IMAGE_ROOT")):
        if (ROOT / "tmp" / name).is_dir(): env[var] = str(ROOT / "tmp" / name)
    r = subprocess.run(["gdb", "-batch", "-nx", "-x", str(Path(__file__).resolve()), "--args", *cmd],
                       cwd=args.executable.parent, env=env, capture_output=True, text=True)
    (out / "gdb.txt").write_text(r.stdout + r.stderr)
    print(out)
