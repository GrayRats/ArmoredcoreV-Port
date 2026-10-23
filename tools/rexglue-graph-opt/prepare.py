#!/usr/bin/env python3
"""Prepare an isolated target-index experiment from the exact installed SDK release.

No installed SDK, upstream checkout or PPC-generated output is modified.
"""
from pathlib import Path
import argparse
import hashlib
import io
import json
import subprocess
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--cli-deps', action='store_true', help='Materialize pinned CLI11/inja sources locally')
args = parser.parse_args()
COMMIT = 'f5337cdc947ff6d4c4196737e2c807a48f2a1fc2'
output = ROOT / 'tmp/rexglue-graph-opt'
baseline = output / 'baseline'
if not baseline.exists():
    baseline.mkdir(parents=True)
    archive = subprocess.run(['git', '-C', str(ROOT / 'tools/rexglue-sdk'), 'archive', COMMIT],
                             capture_output=True, check=True).stdout
    with tarfile.open(fileobj=io.BytesIO(archive)) as source:
        source.extractall(baseline, filter='data')
    (output / 'source-provenance.json').write_text(json.dumps({
        'release_commit': COMMIT, 'archive_sha256': hashlib.sha256(archive).hexdigest(),
        'scope': 'isolated exact-release source copy; installed SDK unchanged'}, indent=2) + '\n')
if json.loads((output / 'source-provenance.json').read_text())['release_commit'] != COMMIT:
    raise ValueError('Existing baseline copy belongs to another SDK revision')

header = 'include/rex/codegen/function_graph.h'
source = 'src/codegen/function_graph.cpp'
originals = {name: (baseline / name).read_text() for name in [header, source]}
modified = dict(originals)
old = '  std::unordered_map<uint32_t, bool> functionHasXrefs_;'
assert modified[header].count(old) == 1
modified[header] = modified[header].replace(old,
    '  // Target -> owner addresses, never raw pointers: replacements/removals stay safe.\n'
    '  std::unordered_map<uint32_t, std::unordered_set<uint32_t>> waitingForFunction_;\n' + old)
modified[header] = modified[header].replace('#pragma once', '#pragma once\n#include <unordered_set>')
old = '  node->addUnresolvedJump(site, target, isCall, conditional);'
assert modified[source].count(old) == 1
modified[source] = modified[source].replace(old, old + '\n  waitingForFunction_[target].insert(entry);')
old = '''void FunctionGraph::notifyFunctionAdded(FunctionNode* newFunction) {
  for (auto& [base, node] : functions_) {
    if (node.get() != newFunction && node->isPending()) {
      node->tryResolveAgainst(newFunction);
    }
  }
}'''
new = '''void FunctionGraph::notifyFunctionAdded(FunctionNode* newFunction) {
  auto found = waitingForFunction_.find(newFunction->base());
  if (found == waitingForFunction_.end())
    return;
  auto owners = std::move(found->second);
  waitingForFunction_.erase(found);
  for (uint32_t base : owners) {
    auto* node = getFunction(base);
    // A removed or replaced owner may leave an index entry. Recheck the live
    // node and its actual unresolved list, preserving original resolution rules.
    if (node && node != newFunction && node->isPending())
      node->tryResolveAgainst(newFunction);
  }
}'''
assert modified[source].count(old) == 1
modified[source] = modified[source].replace(old, new)
report = {'release_commit': COMMIT, 'scope': __doc__, 'files': {}}
for name in [header, source]:
    target = output / 'indexed' / name
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(modified[name])
    report['files'][name] = {'baseline_sha256': hashlib.sha256(originals[name].encode()).hexdigest(),
                            'indexed_sha256': hashlib.sha256(modified[name].encode()).hexdigest()}
(output / 'index-provenance.json').write_text(json.dumps(report, indent=2) + '\n')
print('Prepared isolated baseline/indexed sources at', output)

if args.cli_deps:
    dependencies = {}
    for name, repository, required in [('cli11', 'CLIUtils/CLI11', 'include/CLI/CLI.hpp'),
                                        ('inja', 'pantor/inja', 'single_include/inja/inja.hpp')]:
        tree = subprocess.run(['git', '-C', str(ROOT / 'tools/rexglue-sdk'), 'ls-tree', COMMIT,
                               'thirdparty/' + name], capture_output=True, text=True, check=True).stdout
        revision = tree.split()[2]
        url = f'https://codeload.github.com/{repository}/tar.gz/{revision}'
        archive_path = output / ('dependency-' + name + '.tar.gz')
        if not archive_path.exists():
            with urllib.request.urlopen(url, timeout=60) as response:
                archive_path.write_bytes(response.read())
        data = archive_path.read_bytes()
        parent = output / ('dependency-' + name)
        parent.mkdir(exist_ok=True)
        with tarfile.open(fileobj=io.BytesIO(data)) as archive:
            archive.extractall(parent, filter='data')
        roots = [p for p in parent.iterdir() if p.is_dir() and (p / required).exists()]
        if len(roots) != 1:
            raise ValueError('Dependency archive has unexpected layout: ' + name)
        dependencies[name] = {'revision': revision, 'url': url, 'root': str(roots[0]),
                              'archive_sha256': hashlib.sha256(data).hexdigest()}
        print(name, revision, 'ready')
    (output / 'dependencies.json').write_text(json.dumps(dependencies, indent=2) + '\n')
