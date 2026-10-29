#!/usr/bin/env python3
"""Capture only a newly created documentation launcher; never starts the game."""
import json
import os
from pathlib import Path
import signal
import subprocess
import time

root = Path(__file__).resolve().parents[2]
out = root / "tmp/install-docs"
out.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, ACV_ROOT=str(root), SDL_VIDEODRIVER="x11",
           LIBGL_ALWAYS_SOFTWARE="1", ACV_PROFILES_ROOT=str(out / "profiles"),
           XDG_DATA_HOME=str(out / "xdg-data"), XDG_STATE_HOME=str(out / "xdg-state"))
with (out / "launcher.log").open("w") as log:
    app = subprocess.Popen([str(root / "tmp/acv-trg-build/acv-launcher"),
                            "--profiles-root", str(out / "profiles")],
                           cwd=root, env=env, stdout=log, stderr=subprocess.STDOUT,
                           start_new_session=True)
    try:
        deadline = time.monotonic() + 25
        window = None
        while time.monotonic() < deadline:
            if app.poll() is not None:
                raise RuntimeError("Documentation launcher exited before capture")
            found = subprocess.run(["xdotool", "search", "--onlyvisible", "--pid", str(app.pid)],
                                   capture_output=True, text=True)
            if found.returncode == 0 and found.stdout.strip():
                window = found.stdout.splitlines()[0]
                break
            time.sleep(.2)
        if window is None:
            raise RuntimeError("No window for documentation launcher")
        time.sleep(3)
        subprocess.run(["import", "-window", window, str(out / "install-launcher-linux.jpg")], check=True)
        (out / "capture.json").write_text(json.dumps({
            "scope": "Actual Linux launcher built from source; no game/runtime installed",
            "pid": app.pid, "window": window, "software_rendering": True,
            "game_launched": False}, indent=2) + "\n")
    finally:
        try:
            os.killpg(app.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
        app.wait(timeout=10)
print((out / "launcher.log").read_text())
print("Documentation JPEG captured from owned launcher only")
