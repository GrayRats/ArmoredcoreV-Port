#!/usr/bin/env python3
"""Linux perf_event_open user-IP sampling; no ptrace or injected signals."""
import argparse, collections, ctypes, fcntl, json, mmap, os, struct, time
from pathlib import Path
from perf_gate import wait_for_process_age, process_identity
PAGE=os.sysconf("SC_PAGE_SIZE")
class Ring:
    def __init__(self,tid):
        self.attr=ctypes.create_string_buffer(128)
        # PERF_TYPE_SOFTWARE/CPU_CLOCK; 50 samples per active CPU second.
        struct.pack_into("IIQQQQQ",self.attr,0,1,128,0,20000000,3,0,1|(1<<5)|(1<<6))
        struct.pack_into("I",self.attr,48,1)
        libc=ctypes.CDLL(None,use_errno=True)
        self.fd=libc.syscall(298,ctypes.byref(self.attr),tid,-1,-1,8) # FD_CLOEXEC
        if self.fd<0:raise OSError(ctypes.get_errno(),os.strerror(ctypes.get_errno()))
        try:
            self.mapping=mmap.mmap(self.fd,PAGE*9,mmap.MAP_SHARED,mmap.PROT_READ|mmap.PROT_WRITE)
            self.offset,self.size=struct.unpack_from("QQ",self.mapping,1040)
            self.tail=0;self.lost=0;self.counts=collections.Counter()
            fcntl.ioctl(self.fd,0x2400,0) # PERF_EVENT_IOC_ENABLE
        except BaseException:
            os.close(self.fd);raise
    def read(self):
        head=struct.unpack_from("Q",self.mapping,1024)[0]
        while self.tail<head:
            position=self.tail%self.size
            def data(n):
                part=self.mapping[self.offset+position:self.offset+min(position+n,self.size)]
                return part+self.mapping[self.offset:self.offset+max(0,n-len(part))]
            typ,misc,size=struct.unpack("IHH",data(8))
            if size<8 or self.tail+size>head:raise RuntimeError("Incomplete perf record")
            record=data(size)
            if typ==9:
                ip,pid,tid=struct.unpack_from("QII",record,8);self.counts[ip]+=1
            elif typ==2:self.lost+=struct.unpack_from("Q",record,16)[0]
            self.tail+=size
        struct.pack_into("Q",self.mapping,1032,self.tail)
    def close(self):
        fcntl.ioctl(self.fd,0x2401,0)
        self.read();self.mapping.close();os.close(self.fd)

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--pid",type=int,required=True)
    ap.add_argument("--executable",type=Path,required=True)
    ap.add_argument("--output",type=Path,required=True)
    ap.add_argument("--seconds",type=int,choices=range(1,301),required=True)
    ap.add_argument("--after-start",type=float,default=90,help="Minimum game-process age before recording (default90s)")
    args=ap.parse_args();root=Path("/proc")/str(args.pid)
    if (root/"exe").resolve()!=args.executable.resolve(strict=True):raise RuntimeError("Wrong executable")
    if not 0<=args.after_start<=300:ap.error("--after-start must be0..300 seconds")
    gate=wait_for_process_age(args.pid,args.executable,args.after_start)
    args.output.mkdir(parents=True,exist_ok=False)
    (args.output/"gate.json").write_text(json.dumps(gate,indent=2))
    (args.output/"maps.txt").write_text((root/"maps").read_text())
    rings={};names={};failures={};start=time.monotonic()
    try:
        while time.monotonic()-start<args.seconds and (root/"exe").exists():
            if process_identity(args.pid)!=gate["process_start_ticks"]:raise RuntimeError("Profiled PID reused")
            for task in (root/"task").iterdir():
                tid=int(task.name)
                try:names[tid]=(task/"comm").read_text().strip()
                except OSError:continue
                if tid in rings or tid in failures:continue
                try:
                    rings[tid]=Ring(tid);names[tid]=(task/"comm").read_text().strip()
                except OSError as error:failures[tid]=str(error)
            for ring in rings.values():ring.read()
            time.sleep(.1)
    finally:
        for ring in rings.values():ring.close()
    rows=[{"tid":tid,"name":names[tid],"samples":sum(r.counts.values()),"lost":r.lost,
           "ips":{hex(ip):count for ip,count in r.counts.items()}} for tid,r in rings.items()]
    result={"gate":gate,"pid":args.pid,"seconds":time.monotonic()-start,"threads":rows,"unavailable":failures,"scope":"50Hz per-active-CPU-second user IPs, excludes kernel and hypervisor"}
    (args.output/"samples.json").write_text(json.dumps(result))
    print(json.dumps({"evidence":str(args.output),"sample_count":sum(t["samples"] for t in rows),"lost":sum(t["lost"] for t in rows),"unavailable":len(failures)}))
    if not any(t["samples"] for t in rows):raise RuntimeError("No user IP samples obtained")
if __name__=="__main__":main()
