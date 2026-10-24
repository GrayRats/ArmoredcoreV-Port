#!/usr/bin/env python3
"""Verify hashes, actual loaded libraries, launcher window and bounded game startup.
Uses an isolated profile/state directory; never modifies personal saves.
"""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import subprocess
import sys
import time

ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument('--package', type=Path, required=True)
ap.add_argument('--evidence', type=Path, required=True)
a = ap.parse_args()
p, out = a.package.resolve(), a.evidence.resolve()
out.mkdir(parents=True, exist_ok=False)
m = json.loads((p / 'manifest.json').read_text())
for name, record in m['files'].items():
    with (p / name).open('rb') as stream:
        assert hashlib.file_digest(stream, 'sha256').hexdigest() == record['sha256'], name
assert not any(f.is_symlink() for f in p.rglob('*')), 'Package has external symlinks'
env = dict(os.environ, ACV_PROFILES_ROOT=str(out / 'profiles'), XDG_STATE_HOME=str(out / 'state'))
# Avoid inheriting developer instrumentation in the child processes only.
for key in ('LD_PRELOAD', 'LD_LIBRARY_PATH', 'VK_INSTANCE_LAYERS', 'ENABLE_VULKAN_RENDERDOC_CAPTURE', 'ACV_ROOT'):
    env.pop(key, None)

def local_maps(pid):
    text = Path(f'/proc/{pid}/maps').read_text()
    paths = sorted({line.split(None, 5)[-1].strip() for line in text.splitlines() if '/' in line})
    assert not any('/DEArmoredcore/' in path for path in paths), paths
    return paths

with (out / 'launcher.log').open('w') as log:
    gui = subprocess.Popen([str(p / 'start.sh')], cwd='/', env=env, stdout=log, stderr=log)
    try:
        time.sleep(3)
        assert gui.poll() is None, 'Launcher exited at startup'
        launcher_maps = local_maps(gui.pid)
        assert any(str(p / 'lib/libQt6Core') in x for x in launcher_maps), 'Qt loaded outside package'
        window = subprocess.run(['xdotool', 'search', '--onlyvisible', '--pid', str(gui.pid)],
                                text=True, capture_output=True, check=True).stdout.strip()
        assert window, 'Launcher has no visible X11 window'
    finally:
        if gui.poll() is None:
            gui.terminate()
        gui.wait(timeout=10)
cli = p / 'tools/native-rexglue/acv_profile_cli.py'
profile = json.loads(subprocess.check_output([sys.executable, '-I', str(cli), 'create', 'Package Test'],
                                            cwd='/', env=env, text=True))
settings = out / 'settings.json'
settings.write_text(json.dumps({'schema_version': 1, 'graphics': {'fps_limit': 60}}))
with (out / 'probe.log').open('w') as log:
    probe = subprocess.Popen([sys.executable, '-I', str(p / 'tools/native-rexglue/probe_linux.py'),
                              '--profile', profile['uuid'], '--seconds', '20', '--video-driver', 'x11',
                              '--settings', str(settings)], cwd='/', env=env, stdout=log, stderr=log)
    try:
        child = None
        for _ in range(100):
            children = Path(f'/proc/{probe.pid}/task/{probe.pid}/children')
            ids = children.read_text().split() if children.exists() else []
            if ids:
                child = int(ids[0]); break
            if probe.poll() is not None:
                break
            time.sleep(.1)
        assert child, 'Game child was not started'
        time.sleep(3)
        game_maps = local_maps(child)
        for name in ('librexruntime.so', 'librexgpu-xenos.so', 'libamd_fidelityfx_vk.so'):
            assert str(p / 'bin/game' / name) in game_maps, f'{name} not loaded from package'
        probe.wait(timeout=40)
    finally:
        if probe.poll() is None:
            probe.terminate()
            probe.wait(timeout=15)
launch_path = next((out / 'state/DEArmoredcore/acv/logs/acv-native-linux').glob('*/launch.json'))
launch = json.loads(launch_path.read_text())
assert launch['video_driver'] == 'x11'
assert launch['profile']['runtime_xuid'] == profile['xuid']
assert launch['shm_after']['orphans'] == launch['shm_before']['orphans']
assert '--frame_limit_fps=60' in launch['command']
runtime = (launch_path.parent / 'runtime.log').read_text()
assert 'VulkanPresenter: Created' in runtime
assert 'Creating graphics pipeline' in runtime
assert 'device lost' not in runtime.lower()
assert launch['child_returncode'] in (0, -15), launch['child_returncode']
for message in launch['runtime_error_counts']:
    assert re.fullmatch(r"\[fs\] \[t\d+\] ResolvePath\(system:\\\) failed - device not found", message), message
result = {'package': str(p), 'hashes_checked': len(m['files']), 'launcher_window': window,
          'launcher_maps': launcher_maps, 'game_maps': game_maps,
          'launch_report': str(launch_path), 'probe_returncode': probe.returncode,
          'runtime_error_counts': launch['runtime_error_counts'],
          'profile_xuid': profile['xuid'], 'runtime_xuid': launch['profile']['runtime_xuid'],
          'scope': 'relocation, local libraries, visible launcher, X11 startup, XUID, no shm leaks; not gameplay acceptance'}
(out / 'verification.json').write_text(json.dumps(result, indent=2)+'\n')
print(json.dumps({k:v for k,v in result.items() if not k.endswith('_maps')}, indent=2))
