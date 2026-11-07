#!/usr/bin/env python3
"""Read-only settings contract audit; no game, display-mode or image-effect tests."""
import argparse
import configparser
import json
from pathlib import Path
import re
import subprocess
import acv_settings as S


def declaration_end(text, start):
    """Find a C++ declaration terminator, ignoring semicolons in cvar help strings."""
    quoted = None
    escaped = False
    for i in range(start, len(text)):
        char = text[i]
        if quoted:
            if escaped: escaped = False
            elif char == '\\': escaped = True
            elif char == quoted: quoted = None
        elif char in ('"', "'"): quoted = char
        elif char == ';': return i
    return len(text)


def audit(runtime, saved=None):
    root = Path(__file__).resolve().parents[2]
    names = subprocess.check_output(['rg', '-l', 'REXCVAR_DEFINE_',
                                    str(root/'tools/rexglue-sdk/src'),
                                    str(root/'tools/rexglue-sdk/include')], text=True).splitlines()
    declarations = {}
    ranges = {}
    for name in names:
        text = Path(name).read_text(errors='replace')
        for match in re.finditer(r'^\s*REXCVAR_DEFINE_\w+\(\s*(\w+)', text,re.M):
            declarations.setdefault(match[1],f'{Path(name).relative_to(root)}:{text.count(chr(10),0,match.start())+1}')
            bounds=re.search(r'\.range\(\s*([\d.xa-fA-F+-]+)\s*,\s*([\d.xa-fA-F+-]+)\s*\)',text[match.end():declaration_end(text,match.end())])
            if bounds:
                def number(v):return int(v,0) if 'x' in v.lower() else float(v)
                ranges[match[1]]=tuple(number(v) for v in bounds.groups())
    features = S.runtime_features(runtime)
    images = b''.join((runtime/name).read_bytes() for name in ('librexruntime.so','librexgpu-xenos.so'))
    mapped, checked, rejected = {}, [], []
    for key, (default, options) in S.GRAPHICS_SCHEMA.items():
        if isinstance(options, re.Pattern): options = tuple(value for value,label in S.RESOLUTION_PRESETS)
        for value in options:
            doc = {key: value}
            if key == 'wide_view' and value == 'auto': doc['resolution'] = '2560x1080'
            if key == 'color_depth' and value == 10: doc['dither'] = False
            if key == 'texture_cache_soft_mb': doc['texture_cache_hard_mb'] = 8192
            if key == 'texture_cache_hard_mb': doc['texture_cache_soft_mb'] = 384
            try:
                args = S.graphics_to_cvars(doc, True, features)
                S.graphics_to_env(doc, features)
            except S.SettingsError as error:
                # Unavailable options must be explicitly classified, not silently skipped.
                if (key,value) not in S._UNAVAILABLE: raise
                rejected.append({'key':key,'value':value,'reason':str(error)})
                continue
            assert len({arg.split('=',1)[0] for arg in args}) == len(args), args
            for arg in args:
                name = arg[2:].split('=',1)[0]
                assert name in declarations, f'No SDK declaration for {name}'
                assert name.encode() in images, f'Selected runtime does not expose {name}'
                if name in ranges:
                    lo,hi=ranges[name];numeric_value=float(arg.split('=',1)[1])
                    assert lo<=numeric_value<=hi,f'{arg} outside SDK range {lo}..{hi}'
                mapped[name] = declarations[name]
            checked.append({'key':key,'value':value,'args':args})
    memory = S.host_memory_info()
    presets = {}
    for name in S.GRAPHICS_PRESETS:
        for bits in (8,10):
            doc = {'schema_version':1,'graphics':{'color_depth':bits,'resolution':'2560x1080'},
                   'launcher':{'allow_experimental':True}}
            result = S.apply_graphics_preset(doc,name,features,memory)
            presets[f'{name}-{bits}bit'] = S.graphics_to_cvars(result['graphics'],True,features)
    config_path = root/'tools/native-rexglue/acv.config'
    config = configparser.ConfigParser(inline_comment_prefixes=('#',';'))
    config.read_string(config_path.read_bytes().decode('utf-8-sig'))
    for name in config['sdk']:
        assert name in declarations, f'Unknown canonical config cvar: {name}'
        assert name.encode() in images, f'Config cvar not in selected runtime: {name}'
    saved_check = 'not requested'
    if saved and saved.is_file():
        document = S.load_json(saved,'settings')
        launcher = document.get('launcher',{})
        S.launcher_mangohud(launcher); S.launcher_input_mode(launcher)
        selected = Path(launcher.get('build',runtime/'acv_linux_probe')).parent
        S.graphics_to_cvars(document.get('graphics',{}),launcher.get('allow_experimental',False),S.runtime_features(selected))
        S.graphics_to_env(document.get('graphics',{}),S.runtime_features(selected))
        saved_check = 'valid; read only'
    return {'scope':'schema, SDK declarations, runtime feature/cvar names, preset combinations and canonical config; no image/gameplay acceptance',
            'settings':len(S.GRAPHICS_SCHEMA),'values_checked':len(checked),'explicitly_unavailable':rejected,
            'cvar_sources':mapped,'numeric_ranges':{k:v for k,v in ranges.items() if k in mapped},'features':sorted(features),'memory':memory,
            'presets':presets,'checks':checked,'saved_settings':saved_check}


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime-dir',type=Path,required=True)
    parser.add_argument('--settings',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    report=audit(args.runtime_dir,args.settings)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2,ensure_ascii=False)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k not in ('checks','presets','cvar_sources')},indent=2,ensure_ascii=False))
