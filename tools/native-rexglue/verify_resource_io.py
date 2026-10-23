#!/usr/bin/env python3
"""Verify bounded captured VFS reads against original file slices, read-only.
Implicit offsets are pre-call snapshots; a mismatch may be concurrent seeking.
Does not prove guest decompression or render-queue ownership.
"""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import struct
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from bhd5_index import decode_dcx_dflt

def fnv(data):
    h = 14695981039346656037
    for b in data:
        h = ((h ^ b) * 1099511628211) & 0xffffffffffffffff
    return f"{h:016x}"

def verify(trace, game_root, budget):
    rows = [json.loads(x) for x in trace.read_text().splitlines()]
    files = {}
    checked = []
    skipped = collections.Counter()
    anomalies = []
    total = 0
    game_root = game_root.resolve()
    for r in rows:
        if r['event'] == 'open' and r['status'] == 0:
            name = bytes.fromhex(r['path_hex']).decode('ascii', 'replace').replace('\\','/')
            if name.lower().startswith('game:/'):
                f = (game_root / name[6:].lstrip('/')).resolve()
                if f.is_relative_to(game_root):
                    files[r['token']] = f
        if r['event'] != 'read': continue
        if r['actual'] > r['requested']:
            anomalies.append({'sequence':r['sequence'], 'issue':'actual exceeds requested'})
        f = files.get(r['token'])
        if f is None or not f.is_file(): skipped['unmapped_file'] += 1; continue
        if not r.get('offset_observed'): skipped['unobserved_offset'] += 1; continue
        n = r['sampled']
        if not n or total+n > budget: skipped['empty_or_budget'] += 1; continue
        offset = r['observed_offset']
        if offset > f.stat().st_size or n > f.stat().st_size-offset:
            anomalies.append({'sequence':r['sequence'], 'issue':'slice exceeds file'}); continue
        with f.open('rb') as stream:
            stream.seek(offset); data=stream.read(n)
        total += n
        result = {'sequence':r['sequence'],'path':str(f),'offset':offset,'bytes':n,
                  'checksum_match':fnv(data)==r['fnv1a64'],
                  'nonzero_match':sum(b!=0 for b in data)==r['nonzero'],
                  'source_sha256':hashlib.sha256(data).hexdigest()}
        if not result['checksum_match'] or not result['nonzero_match']:
            anomalies.append({'sequence':r['sequence'],'issue':'payload mismatch'})
        if data[:4] == b'DCX\0' and len(data)>=0x4c:
            size=0x4c+struct.unpack_from('>I',data,0x20)[0]
            if size == len(data) and n == r['actual']:
                try:
                    decoded=decode_dcx_dflt(data)
                    result['dcx']={'decoded_bytes':len(decoded),'sha256':hashlib.sha256(decoded).hexdigest(),'magic':decoded[:4].hex()}
                except ValueError as e:
                    anomalies.append({'sequence':r['sequence'],'issue':str(e)})
            else: result['dcx']='partial or combined read; not decoded'
        checked.append(result)
    return {'scope':'bounded import-return payload checksums and source-slice DCX validation; no guest decompressor/queue parity',
            'events':dict(collections.Counter(r['event'] for r in rows)),
            'checked_bytes':total,'checked_reads':len(checked),'skipped':dict(skipped),
            'anomalies':anomalies,'reads':checked,
            'failed_allocations':[r for r in rows if r['event']=='allocate' and not r['va']],
            'overlap_candidates':[r for r in rows if r['event']=='allocate' and r['overlapping_live_records']]}

if __name__ == '__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('trace',type=Path);ap.add_argument('--game-root',type=Path,required=True)
    ap.add_argument('--output',type=Path,required=True);ap.add_argument('--max-bytes',type=int,default=32*1024*1024)
    args=ap.parse_args();report=verify(args.trace,args.game_root,args.max_bytes)
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k not in ('reads','failed_allocations','overlap_candidates')},indent=2))
    sys.exit(1 if report['anomalies'] or not report['checked_reads'] else 0)
