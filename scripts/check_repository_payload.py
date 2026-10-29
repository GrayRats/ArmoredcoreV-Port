#!/usr/bin/env python3
"""Reject unintended files in the Git index without reading local logs or caches."""
from pathlib import Path
import re
import subprocess
import sys

LIMIT = 512 * 1024
ALLOWED_ROOT = {'.gitignore','README.md','README.RU.MD','AGENTS.md','TASK-SQL.MD','PLAN_GUI.MD','TOOLS.MD','Install.MD','Install.RU.MD','report-fps.md','RELEASE_NOTES_1.1.md','ABOUT_DE-AC5.MD','LICENSE'}
FORBIDDEN = {'node_modules','__pycache__','.venv','build','out','bin','obj','target','dist',
             'generated','thirdparty','third_party','vendor','logs','cache','temp','tmp',
             'rexglue-sdk','XenonRecomp','XenosRecomp','360toolsUpdated','360tools-skill','ghidra-rpc-local'}
SECRETS = [re.compile(rb'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----'),
           re.compile(rb'gh[pousr]_[A-Za-z0-9]{30,}'),
           re.compile(rb'github_pat_[A-Za-z0-9_]{60,}'),
           re.compile(rb'AKIA[0-9A-Z]{16}')]
paths = subprocess.check_output(['git','ls-files','--stage','-z']).split(b'\0')
issues=[]; total=0; largest=('',0); count=0
for raw in paths:
    if not raw: continue
    mode, indexed_name=raw.split(b'\t',1)
    name=indexed_name.decode();p=Path(name);count+=1
    if mode.split()[0] not in {b'100644',b'100755'}:
        issues.append((name,'not a regular source file'));continue
    if name not in ALLOWED_ROOT and p.parts[0] not in {'tools','scripts','docs','fine','perf','SKILL','.github'}:
        issues.append((name,'outside allowed source directories'))
    if any(part in FORBIDDEN for part in p.parts):issues.append((name,'excluded directory'))
    if p.parts[0]=='fine' and (len(p.parts)!=2 or p.suffix.lower()!='.md'):
        issues.append((name,'fine/ may contain only root-level Markdown reports'))
    if p.is_symlink():issues.append((name,'symlink'));continue
    data=subprocess.check_output(['git','show',':'+name]);total+=len(data)
    if len(data)>largest[1]:largest=(name,len(data))
    if len(data)>LIMIT:issues.append((name,'larger than 512 KiB'))
    if b'\0' in data and not (p.parts[:2]==('docs','images') and p.suffix.lower()=='.jpg'):issues.append((name,'binary data'))
    if any(pattern.search(data) for pattern in SECRETS):issues.append((name,'credential pattern'))
    if name=='tools/native-rexglue/acv.config':issues.append((name,'personal profile'))
for name,reason in issues:print(f'{name}: {reason}',file=sys.stderr)
print(f'Source payload: {count} files, {total} bytes; largest {largest[0]} ({largest[1]} bytes); issues {len(issues)}')
sys.exit(bool(issues))
