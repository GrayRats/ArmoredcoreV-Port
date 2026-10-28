#!/usr/bin/env python3
"""Summarize per-thread CPU consumption and observed placement after the sampling gate."""
import argparse, collections, json, math
from pathlib import Path

def summarize(path):
    rows=[json.loads(line) for line in path.read_text().splitlines() if line.strip()]
    if not rows: raise ValueError("No CPU samples")
    threads={}
    previous=None
    for row in rows:
        if row["game_age_s"]<90: raise ValueError("Premature gameplay sampling")
        if previous is not None and row["time"]<=previous: raise ValueError("Non-monotonic sample times")
        previous=row["time"]
        for tid,value in row["threads"].items():
            data=threads.setdefault(tid,{"tid":int(tid),"name":value["name"],"cpu":[],"placements":collections.Counter(),"allowed_cpus":set()})
            if "cpu_percent" in value:
                cpu=value["cpu_percent"]
                if not math.isfinite(cpu) or cpu<0:raise ValueError("Invalid CPU value")
                data["cpu"].append(cpu)
            if "last_cpu" in value:data["placements"][str(value["last_cpu"])]+=1
            if "allowed_cpus" in value:data["allowed_cpus"].add(value["allowed_cpus"])
    result=[]
    for value in threads.values():
        values=value.pop("cpu")
        value["mean_core_percent"]=sum(values)/len(values) if values else None
        value["cpu_samples"]=len(values)
        value["placements"]=dict(value["placements"])
        value["allowed_cpus"]=sorted(value["allowed_cpus"])
        result.append(value)
    result.sort(key=lambda r:r["mean_core_percent"] or 0,reverse=True)
    return {"source":str(path),"samples":len(rows),"first_age_s":rows[0]["game_age_s"],
            "last_age_s":rows[-1]["game_age_s"],
            "scope":"CPU percent per thread, one core=100%; sampled last CPU is placement, not simultaneous execution or useful-work proof. Truncated equal names remain separate by TID.",
            "threads":result}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("samples",type=Path)
    parser.add_argument("--out",type=Path,required=True)
    args=parser.parse_args()
    result=summarize(args.samples)
    args.out.write_text(json.dumps(result,indent=2)+"\n")
    for t in result["threads"][:12]:
        print(t["tid"],t["name"],t["mean_core_percent"],t["allowed_cpus"],t["placements"])
if __name__=="__main__":main()
