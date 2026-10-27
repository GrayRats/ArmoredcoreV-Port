"""Gate diagnostics on Linux process age, preserving PID/executable identity."""
import os,time
from pathlib import Path

def process_identity(pid):
    root=Path("/proc")/str(pid)
    fields=(root/"stat").read_text().rsplit(")",1)[1].split()
    return int(fields[19])

def process_age(start_ticks):
    return time.clock_gettime(time.CLOCK_BOOTTIME)-start_ticks/os.sysconf("SC_CLK_TCK")

def wait_for_process_age(pid, executable, minimum_age):
    expected=executable.resolve(strict=True)
    root=Path("/proc")/str(pid)
    start=process_identity(pid)
    while True:
        if (root/"exe").resolve()!=expected or process_identity(pid)!=start:
            raise RuntimeError("Profiled process identity changed before sampling")
        age=process_age(start)
        if age>=minimum_age:
            return {"process_start_ticks":start,"sampling_started_age_s":age,
                    "minimum_process_age_s":minimum_age}
        time.sleep(min(.25,minimum_age-age))
