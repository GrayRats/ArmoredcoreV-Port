#!/usr/bin/env python3
"""Compare actual old/new readback admission on identical Vulkan pressure.
Synthetic component throughput only, never gameplay FPS. No game may be profiled
concurrently. Baseline is reconstructed from the durable reverse patch, not logs.
"""
import argparse,hashlib,json,statistics,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
 ap=argparse.ArgumentParser(description=__doc__)
 ap.add_argument('--output',type=Path,required=True)
 ap.add_argument('--mixed-pool',action='store_true',help='Compare allocator full drain with oldest-fitting retirement under the same minimal-prefix queue')
 args=ap.parse_args();output=args.output.resolve();output.mkdir(parents=True,exist_ok=False)
 source=ROOT/'tools/native-rexglue'/('vulkan_readback_mixed_pool_benchmark.cpp' if args.mixed_pool else 'vulkan_readback_pressure_benchmark.cpp')
 patch=ROOT/'tools/native-rexglue/patches/gpu-readback-minimal-admission.patch'
 relative=Path('src/graphics/vulkan/deferred_readback_queue.h')
 current=ROOT/'tools/rexglue-sdk'/relative
 with tempfile.TemporaryDirectory(prefix='acv-pressure-baseline-') as directory:
  temporary=Path(directory);restored=temporary/relative;restored.parent.mkdir(parents=True)
  restored.write_bytes(current.read_bytes())
  subprocess.run(['patch','--batch','--fuzz=0','--reverse','-p1','-i',str(patch)],cwd=temporary,check=True)
  (temporary/'acv_readback_baseline.h').write_bytes(restored.read_bytes())
  executable=output/'benchmark'
  subprocess.run(['clang++','-std=c++23','-O2','-Wall','-Wextra','-Werror','-pthread','-I',str(temporary),str(source),'-lvulkan','-o',str(executable)],cwd=ROOT,check=True)
  result=subprocess.run(['timeout','60s',str(executable)],capture_output=True,text=True,cwd=ROOT)
  (output/'stdout.txt').write_text(result.stdout);(output/'stderr.txt').write_text(result.stderr)
  print(result.stdout,end='');print(result.stderr,end='');result.check_returncode()
  rows=[json.loads(line) for line in result.stdout.splitlines() if line.startswith('{')]
  assert len(rows)==6 and all(row['copies']==512 and row['peak_jobs']<=16 for row in rows)
  policies=['drain_all_allocator','oldest_fitting'] if args.mixed_pool else ['drain_all','minimal_prefix']
  groups={policy:[row for row in rows if row['policy']==policy] for policy in policies}
  assert all(len(group)==3 for group in groups.values())
  if args.mixed_pool:assert len({row['request_bytes_total'] for row in rows})==1 and all(row['pool_bytes']==40<<20 for row in rows)
  record={'scope':'Synthetic same-input Vulkan transfer-pressure component benchmark, not gameplay/FPS',
   'source_sha256':sha(source),'support_source_sha256':sha(ROOT/'tools/native-rexglue/vulkan_readback_pressure_benchmark.cpp'),'pool_selection_sha256':sha(ROOT/'tools/rexglue-sdk/src/graphics/vulkan/readback_pool_selection.h') if args.mixed_pool else None,'production_header_sha256':sha(current),'baseline_header_sha256':sha(restored),
   'reverse_patch_sha256':sha(patch),'executable_sha256':sha(executable),'rows':rows,
   'median_elapsed_ms':{key:statistics.median(row['elapsed_ms'] for row in group) for key,group in groups.items()},
   'median_wait_ms':{key:statistics.median(row['wait_ms'] for row in group) for key,group in groups.items()}}
  record['median_throughput_ratio']=record['median_elapsed_ms'][policies[0]]/record['median_elapsed_ms'][policies[1]]
  (output/'summary.json').write_text(json.dumps(record,indent=2)+'\n')
  print('Median component throughput ratio',record['median_throughput_ratio'],'not gameplay FPS')
if __name__=='__main__':main()
