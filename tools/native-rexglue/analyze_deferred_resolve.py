#!/usr/bin/env python3
"""Verify bounded deferred Resolve publication and first CPU-copy evidence."""
import argparse
import collections
import hashlib
import json
from pathlib import Path
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument("prefix",type=Path)
parser.add_argument("--oracle",type=Path,required=True)
parser.add_argument("--physical",type=lambda x:int(x,0),required=True)
parser.add_argument("--output",type=Path,required=True)
a=parser.parse_args()
def path(suffix):return Path(str(a.prefix)+suffix)
def rows(domain):return [json.loads(x) for x in path("."+domain+".jsonl").read_text().splitlines()]
cpu,gpu,deferred=rows("cpu"),rows("gpu"),rows("deferred")
assert not any(x["event"]=="truncated" for x in cpu+gpu+deferred),"Truncated trace"
resolve=path(".resolve-first-2m.bin").read_bytes()
source=path(".converter-source-first-2m.bin").read_bytes()
assert len(resolve)==0x200000 and resolve==source==a.oracle.read_bytes(),"First 2MiB bytes differ"
reads=[x for x in cpu if x["event"]=="converter_source_copy" and x["physical"]==a.physical]
pubs=[x for x in gpu if x["event"]=="resolve_publish" and x["physical"]==a.physical]
enqueues={x["ticket"]:x for x in deferred if x["event"]=="deferred_enqueue"}
published={x["ticket"]:x for x in deferred if x["event"]=="deferred_publish"}
assert reads and pubs and enqueues
pairs=[]
for read in reads:
    before=[x for x in pubs if x["time_us"]<=read["time_us"]]
    assert before,"CPU read before any publication"
    pub=max(before,key=lambda x:x["time_us"])
    assert pub["bytes"]==read["bytes"] and pub["sample_fnv1a64"]==read["sample_fnv1a64"],"Latest publication differs from CPU read"
    assert pub["completed"]>=pub["ticket"] and pub["ticket"] in enqueues
    queued=enqueues[pub["ticket"]]
    assert queued["time_us"]<pub["time_us"]<=read["time_us"]
    pairs.append({"bytes":read["bytes"],"ticket":pub["ticket"],"defer_us":pub["time_us"]-queued["time_us"],"publication_to_cpu_us":read["time_us"]-pub["time_us"]})
first=next(x for x in reads if x["bytes"]==0x200000)
firstpub=max((x for x in pubs if x["time_us"]<=first["time_us"]),key=lambda x:x["time_us"])
flushes=[x for x in deferred if x["event"]=="deferred_flush"]
firstflush=next((x for x in flushes if enqueues[firstpub["ticket"]]["time_us"]<=x["time_us"]<=firstpub["time_us"]),None)
if any(x["event"]=="cpu_sync_request" for x in cpu):
    request=max((x for x in cpu if x["event"]=="cpu_sync_request" and x["physical"]==a.physical and x["time_us"]<=first["time_us"]),key=lambda x:x["time_us"])
    ready=next(x for x in cpu if x["event"]=="cpu_sync_ready" and x["physical"]==a.physical and x["time_us"]>=request["time_us"])
    assert request["time_us"]<=ready["time_us"]<=first["time_us"]
else:request=ready=None
out={"scope":"Private leased resource; first 2MiB exact vs same-run CPU and full oracle; other reads only 4KiB samples. No generic allocation ABA/consumer coverage or FPS proof.","sha256":hashlib.sha256(resolve).hexdigest(),"first_snapshot_bytes":len(resolve),"full_oracle_equal":True,"matched_cpu_reads":len(pairs),"enqueue_count":len(enqueues),"publication_count":len(published),"pending_tickets":sorted(set(enqueues)-set(published)),"flush_reasons":dict(collections.Counter(x["reason"] for x in flushes)),"first_cpu_read":first,"first_publish":firstpub,"first_flush":firstflush,"first_sync_request":request,"first_sync_ready":ready,"pairs":pairs}
a.output.write_text(json.dumps(out,indent=2)+"\n")
print(json.dumps({k:v for k,v in out.items() if k!="pairs"},indent=2))
