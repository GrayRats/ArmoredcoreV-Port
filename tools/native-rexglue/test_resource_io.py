#!/usr/bin/env python3
import json
from pathlib import Path
import tempfile
import unittest
from verify_resource_io import verify, fnv

class ResourceIoTests(unittest.TestCase):
    def test_source_offset_and_handle_generation(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d); (root/'a.bdt').write_bytes(b'abcdef'); (root/'b.bdt').write_bytes(b'uvwxyz')
            rows=[]
            def opened(generation,name):
                rows.append({'event':'open','status':0,'handle':7,'token':generation,'path_hex':('game:/'+name).encode().hex()})
            def read(generation,data,offset):
                rows.append({'event':'read','sequence':len(rows)+1,'handle':7,'token':generation,
                    'actual':len(data),'requested':len(data),'sampled':len(data),'offset_observed':True,
                    'observed_offset':offset,'fnv1a64':fnv(data),'nonzero':sum(x!=0 for x in data)})
            opened(1,'a.bdt'); opened(2,'b.bdt')
            read(1,b'cd',2);read(2,b'wx',2) # old in-flight read retains its opening identity
            trace=root/'trace.jsonl'
            def save(): trace.write_text(''.join(json.dumps(r)+'\n' for r in rows))
            save(); result=verify(trace,root,100)
            self.assertEqual(result['checked_reads'],2);self.assertFalse(result['anomalies'])
            rows[-1]['observed_offset']=1;save()
            self.assertEqual(verify(trace,root,100)['anomalies'][0]['issue'],'payload mismatch')
            rows[-1]['observed_offset']=6;save()
            self.assertEqual(verify(trace,root,100)['anomalies'][0]['issue'],'slice exceeds file')
            rows[-1]['offset_observed']=False;save()
            self.assertEqual(verify(trace,root,100)['skipped']['unobserved_offset'],1)
    def test_failed_allocations_are_not_overlap_proof(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'trace.jsonl'
            p.write_text(json.dumps({'event':'allocate','va':0,'overlapping_live_records':0})+'\n')
            r=verify(p,Path(d),100)
            self.assertEqual(len(r['failed_allocations']),1);self.assertFalse(r['overlap_candidates'])

if __name__=='__main__':unittest.main()
