#!/usr/bin/env python3
"""Audit codegen's unresolved conditional-branch diagnostics against retail bytes."""
import argparse, hashlib, json, re, struct
from pathlib import Path
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from xex_read_guest_range import extract_image
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('diagnostics',type=Path)
parser.add_argument('--output',type=Path,default=Path('logs/acv-rexglue-unresolved-20261021/conditionals'))
a=parser.parse_args(); out=(ROOT/a.output).resolve()
if not any(out.is_relative_to(ROOT/n) for n in ('logs','tmp')):parser.error('Output must stay under logs/ or tmp/')
out.mkdir(parents=True,exist_ok=True)
xex=ROOT/'games/acv/extracted/default.xex';raw=xex.read_bytes();image,base,_,_=extract_image(raw)
text=(ROOT/'tmp/scan/text.bin').read_bytes();pdata=(ROOT/'tmp/scan/pdata.bin').read_bytes()
assert base==0x82000000 and pdata==image[0x216A00:0x216A00+len(pdata)]
owners=[]
for begin,data in struct.iter_unpack('>II',pdata):
 size=((data>>8)&0x3fffff)*4
 if size:owners.append((begin,begin+size))
registry=sorted(set(int(x,16) for x in re.findall(r'PPC_EXTERN_FUNC\(sub_([0-9a-fA-F]{8})\)',(ROOT/'tmp/xenon-more-mulhd-output/ppc_recomp_shared.h').read_text())))
a.diagnostics=(ROOT/a.diagnostics).resolve() if not a.diagnostics.is_absolute() else a.diagnostics.resolve()
assert a.diagnostics.is_relative_to(ROOT)
sites=re.findall(r'Unresolved conditional branch to (0x[0-9a-fA-F]+) from (0x[0-9a-fA-F]+)',a.diagnostics.read_text())
assert len(sites)==95,len(sites)
groups={}
for t,s in sites:
 target,site=int(t,16),int(s,16);off=site-0x822b0000
 assert 0<=off<=len(text)-4
 word=struct.unpack_from('>I',text,off)[0]
 assert word>>26==16,'Expected PPC bc conditional branch'
 disp=word&0xfffc
 if disp&0x8000:disp-=0x10000
 assert ((site+disp)&0xffffffff)==target
 assert text[off:off+4]==image[site-base:site-base+4]
 i=__import__('bisect').bisect_right(registry,target)-1
 ri=registry[i] if i>=0 else None
 group=groups.setdefault(target,{'target':hex(target),'sites':[],'target_pdata_owners':[[hex(b),hex(e)] for b,e in owners if b<=target<e], 'nearest_registry_entry':hex(ri) if ri else None})
 group['sites'].append({'site':hex(site),'word':f'0x{word:08x}'})
for target,r in sorted(groups.items()):
 start,end=max(0x822b0000,target-32),target+64
 q=subprocess.run([sys.executable,str(ROOT/'tools/ppc_dis.py'),str(ROOT/'tmp/scan/text.bin'),hex(start),hex(end)],text=True,capture_output=True,check=True)
 name=f'raw-{target:08x}.asm';(out/name).write_text(q.stdout);r['listing']=name
report={'confidence':'95 raw relative conditional branch encodings and targets confirmed against original XEX; control-flow intent and missing-target repairs unresolved','branch_count':len(sites),'unique_targets':len(groups),'groups':list(groups.values()),'inputs':{str(x.relative_to(ROOT)):hashlib.sha256(x.read_bytes()).hexdigest() for x in [a.diagnostics,xex,ROOT/'tmp/scan/text.bin',ROOT/'tmp/scan/pdata.bin',ROOT/'tmp/xenon-more-mulhd-output/ppc_recomp_shared.h',Path(__file__)]}}
(out/'inventory.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'branches':report['branch_count'],'unique_targets':report['unique_targets'],'targets_in_pdata':sum(bool(x['target_pdata_owners']) for x in report['groups']),'evidence':str(out/'inventory.json')}))
