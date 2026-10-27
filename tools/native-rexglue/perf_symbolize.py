#!/usr/bin/env python3
"""Resolve sampled native IPs using same-session mappings and local ELF symbols."""
import argparse,bisect,collections,json,subprocess
from pathlib import Path

def summarize(folder):
    data=json.loads((folder/"samples.json").read_text())
    maps=[]
    for line in (folder/"maps.txt").read_text().splitlines():
        fields=line.split(maxsplit=5)
        if len(fields)<6:continue
        lo,hi=(int(x,16) for x in fields[0].split("-"))
        maps.append((lo,hi,int(fields[2],16),fields[5]))
    cache={}
    def resolve(ip):
        found=next((m for m in maps if m[0]<=ip<m[1]),None)
        if not found:return "[unmapped]",hex(ip)
        lo,hi,offset,name=found;address=ip-lo+offset
        if name not in cache:
            symbols=[]
            if Path(name).is_file():
                for dynamic in (False,True):
                    result=subprocess.run(["nm",*(["-D"] if dynamic else []),"-n","-S","--defined-only",name],capture_output=True,text=True)
                    for line in result.stdout.splitlines():
                        parts=line.split()
                        if len(parts)==4 and parts[2] in ("T","t","W","w"):
                            try:symbols.append((int(parts[0],16),int(parts[1],16),parts[3]))
                            except ValueError:pass
                    if symbols:break
            symbols.sort();cache[name]=([s[0] for s in symbols],symbols)
        starts,symbols=cache[name];index=bisect.bisect_right(starts,address)-1
        if index>=0:
            start,size,symbol=symbols[index]
            if size and address<start+size:return name,symbol
        return name,"offset "+hex(address)
    threads=[]
    for thread in data["threads"]:
        counts=collections.Counter()
        for ip,count in thread["ips"].items():counts[resolve(int(ip,16))]+=count
        top=[{"module":module,"symbol":symbol,"samples":count,"thread_percent":round(100*count/max(1,thread["samples"]),2)} for (module,symbol),count in counts.most_common(15)]
        threads.append({k:thread[k] for k in ("tid","name","samples","lost")}|{"top":top})
    threads.sort(key=lambda t:t["samples"],reverse=True)
    report={"gate":data.get("gate"),"scope":data["scope"],"seconds":data["seconds"],"unavailable":data["unavailable"],"threads":threads}
    (folder/"summary.json").write_text(json.dumps(report,indent=2))
    for t in threads[:5]:
        print(t["tid"],t["name"],"samples",t["samples"],"lost",t["lost"])
        for row in t["top"][:5]:print(" ",row["thread_percent"],Path(row["module"]).name,row["symbol"])
if __name__=="__main__":
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument("folder",type=Path);summarize(ap.parse_args().folder)
