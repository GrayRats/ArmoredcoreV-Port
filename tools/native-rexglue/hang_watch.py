#!/usr/bin/env python3
"""Run the game under gdb and capture every thread's stack when the runtime log goes quiet.

Why: a freeze on a mission load left no stack. ptrace_scope=1 forbids attaching gdb to a game started by
the launcher, so the only way to see the stacks is to start the game as gdb's child. GDB starts the normal probe and follows its game child at exec. The probe owns the profile lock
and passes its descriptor to the game, including after the debugger detaches.

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
for sig in ("SIGSEGV", "SIGBUS", "SIGUSR1", "SIGUSR2", "SIG34", "SIG35", "SIG36", "SIGALRM", "SIGPIPE", "SIGTERM"):
    gdb.execute(f"handle {sig} nostop noprint pass")
state = {"fired": False, "pid": None, "stop_signal": None}
def record_stop(event):
    state["stop_signal"] = getattr(event, "stop_signal", None)
gdb.events.stop.connect(record_stop)
log = Path(cfg["log"]) if cfg.get("log") else None; out = Path(cfg["out"])
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
# Follow the normal probe's exec: it acquires/inherits the writer lock itself.
gdb.execute("set startup-with-shell off")
gdb.execute("set follow-fork-mode child")
gdb.execute("set detach-on-fork on")
gdb.execute("set follow-exec-mode same")
gdb.execute("starti")
gdb.execute("catch exec", to_string=True)
gdb.execute("continue", to_string=True)
state["pid"] = gdb.selected_inferior().pid
if not state["pid"]:
    raise RuntimeError("Probe exited before game exec")
actual = Path(f"/proc/{state['pid']}/exe").resolve()
if actual != Path(cfg["executable"]).resolve():
    raise RuntimeError(f"Unexpected exec target: {actual}")
argv = Path(f"/proc/{state['pid']}/cmdline").read_bytes().split(b"\0")
if log is None:
    log = Path(next(a.split(b"=",1)[1].decode() for a in argv if a.startswith(b"--log_file=")))
(out / "observed-child.json").write_text(json.dumps({"pid":state["pid"],"executable":str(actual),"log":str(log),"argv":[a.decode(errors="replace") for a in argv if a]}))
# Other execs are not expected in the game; discard the setup catchpoint.
gdb.execute("delete breakpoints", to_string=True)
threading.Thread(target=watcher, daemon=True).start()
if cfg.get("diagnostics"):
    gdb.execute("source " + cfg["diagnostics"])
continued = gdb.execute("continue", to_string=True)
(out / "gdb-run.txt").write_text(continued)
diagnostic_failed = "Python Exception" in continued or "Error occurred in Python" in continued
if diagnostic_failed:
    (out / "observer-error.json").write_text(json.dumps({"error":"GDB diagnostic callback failed","detail":continued[-8000:]}))
    gdb.write("GDB diagnostic callback failed; see observer-error.json\n", gdb.STDERR)
if gdb.selected_inferior().pid and (state["fired"] or state["stop_signal"]):
    state["reason"] = state.get("reason", "signal:" + str(state["stop_signal"]))
    text = gdb.execute("thread apply all bt 40", to_string=True)
    (out / "bt.txt").write_text(text)
    (out / "hang.json").write_text(json.dumps({"reason": state["reason"], "log_size": log.stat().st_size, "idle_s": cfg["idle"],
                                               "time": time.strftime("%Y-%m-%dT%H:%M:%S")}))
# Detach, do not kill: a long but healthy load must keep running after the stacks were captured.
if gdb.selected_inferior().pid:
    gdb.execute("detach")
gdb.execute("quit 1" if diagnostic_failed else "quit")
'''

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--profile", required=True)
    ap.add_argument("--settings", type=Path, required=True)
    ap.add_argument("--build", type=Path, default=ROOT / "tmp/acv-ffx-test/acv_linux_probe")
    ap.add_argument("--profiles-root", type=Path, default=ROOT / "tmp/acv-user-data")
    ap.add_argument("--idle", type=int, default=90)
    ap.add_argument("--diagnostics", type=Path, help="Read-only GDB diagnostic script loaded before gameplay")
    ap.add_argument("--seconds",type=int,choices=range(1,301),metavar="1..300",help="Optional bounded normal probe; default is an unbounded play session")
    args = ap.parse_args()
    stamp = time.strftime("%Y%m%dT%H%M%S")
    out = ROOT / "logs/acv-hang-watch" / stamp; out.mkdir(parents=True)
    command = [sys.executable, "-I", str(PROBE), "--profile", args.profile,
               "--profiles-root", str(args.profiles_root.resolve()), "--settings", str(args.settings.resolve()),
               "--allow-experimental", "--executable", str(args.build.resolve()), "--video-driver", "x11"]
    command += ["--seconds", str(args.seconds)] if args.seconds else ["--session"]
    cfg = {"out":str(out), "idle":args.idle, "executable":str(args.build.resolve())}
    if args.diagnostics:
        cfg["diagnostics"] = str(args.diagnostics.resolve(strict=True))
    inner = out / "inner.py"; inner.write_text(INNER)
    (out / "command.json").write_text(json.dumps({"mode":"follow-normal-probe", "command":command},indent=2))
    run_env = dict(os.environ, ACV_HANG_CFG=json.dumps(cfg))
    run_env.pop("ACV_PROBE_DRYRUN",None)
    print(f"session folder: {out}\nnormal probe owns profile lock; freeze threshold: {args.idle} s",flush=True)
    rc = subprocess.run(["gdb", "-batch", "-nx", "-x", str(inner), "--args", *command],
                        cwd=ROOT,env=run_env).returncode
    print(f"gdb exit {rc}; stacks (if any): {out / 'bt.txt'}")
    return rc

if __name__ == "__main__":
    sys.exit(main())
