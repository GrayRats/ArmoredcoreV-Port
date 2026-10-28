#!/usr/bin/env python3
"""Bounded Linux CPU/GPU samples for an explicitly identified owned game."""
import argparse, json, os, subprocess, time
from pathlib import Path
from perf_gate import wait_for_process_age,process_age

def thread_sample(pid):
    result={}
    for entry in (Path("/proc")/str(pid)/"task").iterdir():
        try:
            stat=(entry/"stat").read_text()
            name,tail=stat[stat.index("(")+1:stat.rindex(")")],stat[stat.rindex(")")+2:].split()
            result[entry.name]={"name":name,"state":tail[0],"ticks":int(tail[11])+int(tail[12]),
                                "wchan":(entry/"wchan").read_text().strip(),
                                "last_cpu":int(tail[36]),
                                "allowed_cpus":next((line.split(":",1)[1].strip() for line in (entry/"status").read_text().splitlines() if line.startswith("Cpus_allowed_list:")),"")}
        except (OSError,ValueError):continue
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid",type=int,required=True)
    parser.add_argument("--executable",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True)
    parser.add_argument("--seconds",type=int,choices=range(1,301),required=True)
    parser.add_argument("--after-start",type=float,default=90,help="Minimum game-process age before recording (default90s)")
    args=parser.parse_args()
    if not 0<=args.after_start<=300:parser.error("--after-start must be0..300 seconds")
    expected=args.executable.resolve(strict=True)
    process=Path("/proc")/str(args.pid)
    if (process/"exe").resolve()!=expected:raise RuntimeError("Unexpected game executable")
    identity=(process/"stat").read_text().rsplit(")",1)[1].split()[19]
    gate=wait_for_process_age(args.pid,expected,args.after_start)
    args.output.mkdir(parents=True,exist_ok=False)
    (args.output/"gate.json").write_text(json.dumps(gate,indent=2))
    (args.output/"command.json").write_text(json.dumps({"pid":args.pid,"executable":str(expected),
         "start_ticks":identity,"seconds":args.seconds,"clock_ticks":os.sysconf("SC_CLK_TCK"),
         "scope":"read-only /proc and nvidia-smi; no debugger, no process injection"},indent=2))
    clock=os.sysconf("SC_CLK_TCK");previous={};previous_time=None
    with (args.output/"samples.jsonl").open("w") as output:
        start=time.monotonic()
        while time.monotonic()-start<args.seconds:
            try:
                if (process/"exe").resolve()!=expected:break
                if (process/"stat").read_text().rsplit(")",1)[1].split()[19]!=identity:break
                now=time.monotonic();current=thread_sample(args.pid)
                row={"time":time.time(),"game_age_s":process_age(int(identity)),"elapsed":now-start,"threads":current}
                if previous_time is not None:
                    for tid,data in current.items():
                        if tid in previous:
                            data["cpu_percent"]=100*(data["ticks"]-previous[tid]["ticks"])/clock/(now-previous_time)
                row["status"]=[s for s in (process/"status").read_text().splitlines() if s.startswith(("VmRSS:","VmSize:","Threads:"))]
                try:
                    row["io"]={key:int(value) for key,value in (line.split(":",1) for line in (process/"io").read_text().splitlines())}
                except OSError:
                    row["io"]=None
                try:
                    gpu=subprocess.run(["nvidia-smi","--query-gpu=index,uuid,utilization.gpu,utilization.memory,memory.used,clocks.current.graphics,power.draw,pstate",
                                        "--format=csv,noheader,nounits"],capture_output=True,text=True,timeout=3)
                    row["gpu_csv"]=gpu.stdout.strip();row["gpu_returncode"]=gpu.returncode
                    if gpu.returncode:row["gpu_stderr"]=gpu.stderr.strip()
                except (OSError,subprocess.TimeoutExpired) as error:row["gpu_unavailable"]=str(error)
                output.write(json.dumps(row)+"\n");output.flush()
                previous,previous_time=current,now
            except (FileNotFoundError,ProcessLookupError):break
            time.sleep(max(0,start+int(time.monotonic()-start)+1-time.monotonic()))
    print(json.dumps({"samples":sum(1 for _ in (args.output/"samples.jsonl").open()),"evidence":str(args.output)}))
if __name__=="__main__":main()
