#!/usr/bin/env python3
"""Run the game under gdb and capture every thread's stack when the runtime log goes quiet.

Why: a freeze on a mission load left no stack. ptrace_scope=1 forbids attaching gdb to a game started by
the launcher, so the only way to see the stacks is to start the game as gdb's child. The probe is asked
(ACV_PROBE_DRYRUN) for the exact command and environment; gdb then runs the game itself.

A watcher thread sends SIGINT when runtime.log has not changed for --idle seconds. The stop is handled by
the gdb main thread, which writes `thread apply all bt` to the session folder and then detaches: the game keeps
running either way, so a slow but healthy load is not interrupted.
Normal play is not interrupted: the signal is sent only after the log has been silent.

Usage: python3 hang_watch.py --profile UUID --settings tmp/acv-user-data/settings.json [--idle 90]
Then play the game normally. The stack file appears in logs/acv-hang-watch/<stamp>/bt.txt if a freeze happens.
"""
import argparse, json, os, subprocess, sys, tempfile, time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PROBE = ROOT / "tools/native-rexglue/probe_linux.py"

INNER = r'''
import gdb, json, os, signal, threading, time
from pathlib import Path
cfg = json.loads(os.environ["ACV_HANG_CFG"])
gdb.execute("set pagination off"); gdb.execute("set confirm off"); gdb.execute("set debuginfod enabled off")
for sig in ("SIGSEGV", "SIGBUS", "SIGUSR1", "SIGUSR2", "SIG34", "SIG35", "SIG36", "SIGALRM", "SIGPIPE"):
    gdb.execute(f"handle {sig} nostop noprint pass")
state = {"fired": False, "pid": None}
log = Path(cfg["log"]); out = Path(cfg["out"])
def watcher():
    last = time.monotonic(); size = -1
    while not state["fired"]:
        time.sleep(2)
        try: now_size = log.stat().st_size
        except OSError: continue
        if now_size != size:
            size = now_size; last = time.monotonic(); continue
        # Silence OR a manual request: a game that keeps logging (pipeline churn, repeated warnings) but
        # does not progress is not caught by silence alone, so `touch <session>/dump_now` also triggers it.
        manual = (out / "dump_now").exists()
        if state["pid"] and (manual or time.monotonic() - last >= cfg["idle"]):
            state["fired"] = True
            state["reason"] = "manual" if manual else "silence"
            os.kill(state["pid"], signal.SIGINT)
threading.Thread(target=watcher, daemon=True).start()
gdb.execute("starti")
state["pid"] = gdb.selected_inferior().pid
gdb.execute("continue", to_string=True)
if state["fired"]:
    text = gdb.execute("thread apply all bt 40", to_string=True)
    (out / "bt.txt").write_text(text)
    (out / "hang.json").write_text(json.dumps({"reason": state["reason"], "log_size": log.stat().st_size, "idle_s": cfg["idle"],
                                               "time": time.strftime("%Y-%m-%dT%H:%M:%S")}))
# Detach, do not kill: a long but healthy load must keep running after the stacks were captured.
gdb.execute("detach"); gdb.execute("quit")
'''

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--profile", required=True)
    ap.add_argument("--settings", type=Path, required=True)
    ap.add_argument("--build", type=Path, default=ROOT / "tmp/acv-ffx-test/acv_linux_probe")
    ap.add_argument("--idle", type=int, default=90)
    args = ap.parse_args()
    stamp = time.strftime("%Y%m%dT%H%M%S")
    out = ROOT / "logs/acv-hang-watch" / stamp; out.mkdir(parents=True)
    dry = out / "command.json"
    env = dict(os.environ, ACV_PROBE_DRYRUN=str(dry))
    r = subprocess.run([sys.executable, "-I", str(PROBE), "--session", "--profile", args.profile,
                        "--profiles-root", str(ROOT / "tmp/acv-user-data"), "--settings", str(args.settings),
                        "--allow-experimental", "--executable", str(args.build), "--video-driver", "x11"],
                       env=env, capture_output=True, text=True)
    if r.returncode or not dry.is_file():
        sys.exit(f"probe dry run failed: {r.stderr.strip()[-400:]}")
    spec = json.loads(dry.read_text())
    log_file = next(c.split("=", 1)[1] for c in spec["command"] if c.startswith("--log_file="))
    cfg = {"log": log_file, "out": str(out), "idle": args.idle}
    inner = out / "inner.py"; inner.write_text(INNER)
    run_env = dict(spec["env"], ACV_HANG_CFG=json.dumps(cfg))
    print(f"session folder: {out}\nlog: {log_file}\nfreeze threshold: {args.idle} s without log output", flush=True)
    rc = subprocess.run(["gdb", "-batch", "-nx", "-x", str(inner), "--args", *spec["command"]],
                        cwd=spec["cwd"], env=run_env).returncode
    print(f"gdb exit {rc}; stacks (if any): {out / 'bt.txt'}")

if __name__ == "__main__":
    main()
