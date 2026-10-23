#!/usr/bin/env python3
"""Correlate bounded Resolve publication and guest texture-read traces.
Clock order/overlap and 4KiB sample matches are evidence, not full-resource parity.
"""
from pathlib import Path
import argparse,json,collections

def analyze(prefix):
    cpu=[json.loads(x) for x in Path(str(prefix)+'.cpu.jsonl').read_text().splitlines()]
    gpu=[json.loads(x) for x in Path(str(prefix)+'.gpu.jsonl').read_text().splitlines()]
    writes=[x for x in gpu if x['event']=='resolve_publish']
    locks={}; returns={}; correlations=[]
    for c in cpu:
        tid=c.get('host_tid')
        if c['event']=='lock_enter': locks[tid]=c
        if c['event']=='lock_return':
            enter=locks.get(tid)
            c['duration_us']=c['time_us']-enter['time_us'] if enter else None
            returns[c['physical']]=c
        if c['event']!='cpu_copy_read':continue
        before=[w for w in writes if w['time_us']<=c['time_us'] and
                w['physical']<c['physical']+c['bytes'] and c['physical']<w['physical']+w['bytes']]
        last=before[-1] if before else None
        row={'copy':c,'lock':returns.get(c['physical']),'last_overlapping_resolve':last,
             'publish_to_copy_us':c['time_us']-last['time_us'] if last else None}
        if last:
            row['covers_whole_copy']=last['physical']<=c['physical'] and last['physical']+last['bytes']>=c['physical']+c['bytes']
            if last['physical']==c['physical'] and last['sampled']==c['sampled']:
                row['sample_match']=last['sample_fnv1a64']==c['sample_fnv1a64']
            row['ticket_completed']=last['completed']>=last['ticket']
        correlations.append(row)
    return {'scope':'ordered range and bounded 4KiB checksum correlation; not full-resource generation proof',
            'cpu_events':dict(collections.Counter(x['event'] for x in cpu)),
            'gpu_events':dict(collections.Counter(x['event'] for x in gpu)),
            'copy_count':len(correlations),'overlap_matches':sum(x['last_overlapping_resolve'] is not None for x in correlations),
            'correlations':correlations}

if __name__=='__main__':
    a=argparse.ArgumentParser(description=__doc__);a.add_argument('prefix',type=Path);a.add_argument('--output',type=Path,required=True)
    args=a.parse_args();r=analyze(args.prefix);args.output.write_text(json.dumps(r,indent=2)+'\n')
    print(json.dumps({k:v for k,v in r.items() if k!='correlations'},indent=2))
