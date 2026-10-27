"""Bounded, read-only launch telemetry shared by both launcher frontends."""
import json, time
from pathlib import Path
import acv_display

def latest(logs):
    runs=sorted((Path(logs)/"acv-native-linux").glob("2*"))
    runs=[r for r in runs if r.is_dir()]
    if not runs:return {"available":False,"reason":"Пока нет запусков игры"}
    run=runs[-1];report=run/"launch.json"
    launch=json.loads(report.read_text()) if report.is_file() else {}
    runtime=run/"runtime.log"
    display=launch.get("display",{})
    activity=None
    if runtime.is_file():
        with runtime.open("rb") as f:
            header=f.read(128*1024)
            size=f.seek(0,2)
            f.seek(max(len(header),size-1024*1024))
            tail=f.read(1024*1024)
        if not display:display=acv_display.summarize((header+b"\n"+tail).decode("utf-8",errors="replace"))
        activity=max(0,time.time()-runtime.stat().st_mtime)
    pacing=display.get("pacing",[])
    return {"available":True,"evidence":str(run),"completed":report.is_file(),
            "child_returncode":launch.get("child_returncode"),
            "fatal_error_count":launch.get("fatal_error_count",0),
            "output":display.get("output"),"applied":display.get("applied"),
            "fallbacks":display.get("fallbacks",[]),
            "monitors":display.get("monitors",[]),"display_modes":display.get("display_modes",[]),
            "pacing_last":pacing[-1] if pacing else None,
            "idle_seconds":activity,"limit_reached":launch.get("limit_reached")}
