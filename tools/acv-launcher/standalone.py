#!/usr/bin/env python3
"""Assemble a relocatable local ACV folder, including owned game data when requested.

The existing output is never removed. Build into a sibling staging directory and publish
only after checking hashes, Python tools and local dynamic-library resolution.
"""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import shlex
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
NATIVE = ROOT / 'tools/native-rexglue'
BUILD = ROOT / 'tmp/acv-rexglue-linux-indexed/out/build/linux-release'


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def dependencies(path, env):
    result = subprocess.run(['ldd', str(path)], env=env, text=True, capture_output=True, check=True)
    if 'not found' in result.stdout:
        raise RuntimeError(result.stdout)
    return {name: Path(location) for name, location in
            re.findall(r'^\s*(\S+) => (/\S+)', result.stdout, re.M)}


def candidate_environment(items):
    permitted = {"ACV_GENERAL_DEFERRED_READBACK": {"0", "1"},
                 "ACV_PARALLEL_READBACK_COPY": {"0", "2", "4"},
                 "ACV_GPU_POLL_BACKOFF": {"0", "1"}}
    result = {}
    for item in items:
        key, separator, value = item.partition("=")
        if not separator or key not in permitted or value not in permitted[key] or key in result:
            raise ValueError("Invalid or duplicate candidate launch environment: " + item)
        result[key] = value
    return result


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--runtime-dir', type=Path, default=BUILD)
    ap.add_argument('--include-game-data', action='store_true')
    ap.add_argument('--frontend', choices=('qt', 'trg'), default='qt')
    ap.add_argument('--launcher', type=Path)
    ap.add_argument('--skip-settings-tests', action='store_true', help='Package without the settings suite, including effect checks; record the omission')
    ap.add_argument('--candidate-env', action='append', default=[], metavar='KEY=VALUE', help='Explicit bounded readback settings for an unaccepted candidate; repeatable')
    ap.add_argument('--qt-plugins', type=Path, default=Path('/usr/lib/qt6/plugins'))
    args = ap.parse_args()
    out = args.out.resolve()
    if out.exists():
        ap.error(f'Output already exists; choose a new folder: {out}')
    runtime = args.runtime_dir.resolve()
    baseline = json.loads((NATIVE / 'RUNTIME_BASELINE.json').read_text())
    artifacts = {name: runtime / name for name in baseline['runtime_artifacts']}
    if (runtime / 'libamd_fidelityfx_vk.so').exists():
        artifacts['libamd_fidelityfx_vk.so'] = runtime / 'libamd_fidelityfx_vk.so'
    hashes = {name: digest(path) for name, path in artifacts.items()}
    accepted = all(hashes.get(name) == record['sha256']
                   for name, record in baseline['runtime_artifacts'].items())
    launch_environment = baseline.get('launch_environment', {}) if accepted else {}
    permitted = {'ACV_GENERAL_DEFERRED_READBACK': {'0', '1'}, 'ACV_PARALLEL_READBACK_COPY': {'0', '2', '4'}, 'ACV_GPU_POLL_BACKOFF': {'0', '1'}}
    if any(key not in permitted or value not in permitted[key] for key, value in launch_environment.items()):
        raise RuntimeError('Invalid baseline launch environment')
    if args.candidate_env:
        launch_environment = candidate_environment(args.candidate_env)
        accepted = False
    tests = {}
    for name in ['test_acv_settings.py', 'test_acv_profiles.py', 'test_acv_shm.py', 'test_acv_display.py', 'test_acv_sessions.py', 'test_probe_runtime_errors.py', 'test_acv_install.py']:
        if args.skip_settings_tests and name == 'test_acv_settings.py':
            tests[name] = {'skipped': True, 'reason': 'User requested CAS/FSR integration without effect tests'}
            continue
        result = subprocess.run([sys.executable, '-I', str(NATIVE / name)],
                                text=True, capture_output=True)
        tests[name] = {'returncode': result.returncode, 'output': result.stdout + result.stderr}
        if result.returncode:
            raise RuntimeError(tests[name]['output'])
    out.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix=out.name + '.staging-', dir=out.parent))
    files = {}

    def put(source, relative):
        target = stage / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        # No links to the build tree, SDK, or original media.
        files[relative] = {'sha256': digest(target), 'size': target.stat().st_size}

    def write(relative, text, executable=False):
        target = stage / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text)
        if executable:
            target.chmod(0o755)
        files[relative] = {'sha256': digest(target), 'size': target.stat().st_size}

    try:
        put(args.launcher or ROOT / ('tmp/acv-trg-build/acv-launcher' if args.frontend == 'trg' else 'tmp/acv-launcher-build/acv-launcher'), 'bin/acv-launcher')
        if args.frontend == 'trg':
            put(ROOT / 'tools/acv-trg-launcher/backend.py', 'tools/acv-trg-launcher/backend.py')
            for name in ('DejaVuSans.ttf', 'DejaVuSans-Bold.ttf'):
                put(Path('/usr/share/fonts/TTF') / name, 'data/fonts/' + name)
            put(Path('/usr/share/licenses/ttf-dejavu/LICENSE'), 'licenses/DejaVu.txt')
            for source, name in [('tools/trg-launcher/README.md', 'TRG-upstream-notice'),
                                  ('tmp/acv-trg-build/_deps/acv_sdl2-src/LICENSE.txt', 'SDL2'),
                                  ('tmp/acv-trg-build/_deps/trg_imgui_src-src/LICENSE.txt', 'ImGui'),
                                  ('tools/rexglue-sdk/thirdparty/inja/third_party/include/nlohmann/LICENSE.MIT', 'nlohmann-json')]:
                if not (ROOT / source).is_file():
                    raise RuntimeError(f'Missing dependency license: {source}')
                put(ROOT / source, 'licenses/' + name + '.txt')
        for name in ['acv_paths.py', 'acv_display.py', 'acv_sessions.py', 'acv_install.py', 'game_data_manifest.json', 'acv_shm.py', 'acv_settings.py', 'acv_profiles.py',
                     'acv_profile_cli.py', 'probe_linux.py', 'profile_startup.py', 'trace_projection.py',
                     'acv.config', 'input-profile.json', 'RUNTIME_BASELINE.json']:
            put(NATIVE / name, 'tools/native-rexglue/' + name)
        for name, source in artifacts.items():
            put(source, 'bin/game/' + name)
        env = dict(os.environ, LD_LIBRARY_PATH=str(stage / 'bin/game'))
        if args.frontend == 'qt':
            for name in ['libqxcb.so', 'libqoffscreen.so']:
                put(args.qt_plugins / 'platforms' / name, 'plugins/platforms/' + name)
            env = dict(os.environ, LD_LIBRARY_PATH=str(stage / 'bin/game'))
            # Bundle Qt and its version-specific ICU dependencies; OS libraries and GPU drivers stay system-owned.
            for binary in [stage / 'bin/acv-launcher', *list((stage / 'plugins/platforms').glob('*.so'))]:
                for name, source in dependencies(binary, env).items():
                    if name.startswith(('libQt6', 'libicu')) and 'lib/' + name not in files:
                        put(source, 'lib/' + name)
            write('bin/qt.conf', '[Paths]\nPrefix=..\nPlugins=plugins\nLibraries=lib\n')
        write('acv-package.json', json.dumps({'schema_version': 1, 'layout': 'standalone', 'frontend': args.frontend,
              'launch_environment': launch_environment, 'runtime_status': 'accepted baseline' if accepted else 'candidate: gameplay acceptance pending'}, indent=2)+'\n')
        startup = '''#!/bin/sh
set -eu
ACV_PACKAGE_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
export ACV_ROOT="$ACV_PACKAGE_DIR"
export LD_LIBRARY_PATH="$ACV_PACKAGE_DIR/lib:$ACV_PACKAGE_DIR/bin/game${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="$ACV_PACKAGE_DIR/plugins"
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"
exec "$ACV_PACKAGE_DIR/bin/acv-launcher" "$@"
'''
        if args.frontend == 'trg':
            startup = startup.replace('export QT_PLUGIN_PATH="$ACV_PACKAGE_DIR/plugins"\n', '').replace('export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"\n', '')
            startup = startup.replace('exec "$ACV_PACKAGE_DIR/bin/acv-launcher"', 'export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-x11}"\nexec "$ACV_PACKAGE_DIR/bin/acv-launcher"')
        exports = ''.join('export ' + key + '=' + shlex.quote(value) + '\n' for key, value in launch_environment.items())
        startup = startup.replace('exec "$ACV_PACKAGE_DIR/bin/acv-launcher"', exports + 'exec "$ACV_PACKAGE_DIR/bin/acv-launcher"')
        write('start.sh', startup, True)
        if args.include_game_data:
            for source, relative in [(ROOT / 'tmp/iso_root', 'data/game'),
                                     (ROOT / 'tmp/acv-dlc-sound', 'data/acv-dlc-sound'),
                                     (ROOT / 'tmp/acv-dlc-image', 'data/acv-dlc-image')]:
                if not source.is_dir():
                    if relative == 'data/game':
                        raise RuntimeError('Game data is missing')
                    continue
                for path in sorted(source.rglob('*')):
                    if path.is_file():
                        put(path, relative + '/' + path.relative_to(source).as_posix())
            if digest(stage / 'data/game/default.xex') != '4fac87348f827e84633013375012284cd8c769a3c155a675b2989757e666a12b':
                raise RuntimeError('Game XEX differs from pinned input')
        readme = '''ARMORED CORE V — Linux / ReXGlue
Запуск: ./start.sh (можно из другого рабочего каталога).
Игра: bin/game/acv_linux_probe; игровые данные: data/game/.
Лаунчер и runtime находятся в этой папке; дерево разработки и SDK не нужны.
Сохранения/профили: ${XDG_DATA_HOME:-~/.local/share}/DEArmoredcore/acv/.
Логи: ${XDG_STATE_HOME:-~/.local/state}/DEArmoredcore/acv/logs/.
ACV_PROFILES_ROOT позволяет явно выбрать другое хранилище профилей.
Обновление: новая папка программы использует прежнее хранилище профилей.
Нужны Linux x86_64 с совместимой glibc, Python 3, X11/XWayland и Vulkan-драйвер.
Qt/ICU и плагины платформ включены; системные библиотеки перечислены в manifest.json.
Это локальная сборка со статически перекомпилированной игрой, не полный восстановленный исходный код.
CAS/FSR экспериментальны. Игровой save/load и темп требуют отдельной приёмки.
Расширенный обзор UltraWide/16:10 и центрирование HUD доступны как экспериментальная настройка.
CAS/FSR 1 интегрированы; для них и расширенного обзора включите экспериментальные настройки.
Качество CAS/FSR не проверялось по указанию пользователя.
Физический вывод Y′CbCr в текущем backend недоступен.
10-битный SDR экспериментален: при отсутствии формата поверхности используется 8 бит.
Фактические режим, геометрия и интервалы подачи кадров отображаются в лаунчере.
'''
        if args.frontend == 'trg':
            readme = readme.replace('Qt/ICU и плагины платформ включены;', 'TRG / Dear ImGui / SDL2 включены, Qt/ICU не используются;')
        write('README.txt', readme)
        env['LD_LIBRARY_PATH'] = str(stage / 'lib') + ':' + str(stage / 'bin/game')
        system_dependencies = {}
        binaries = [stage / 'bin/acv-launcher', *[stage / 'bin/game' / name for name in artifacts],
                    *list((stage / 'plugins/platforms').glob('*.so'))]
        for binary in binaries:
            deps = dependencies(binary, env)
            for name, path in deps.items():
                if path.is_relative_to(stage):
                    continue
                if path.is_relative_to(ROOT):
                    raise RuntimeError(f'Build-tree dependency remains: {binary}: {name}: {path}')
                system_dependencies[name] = str(path)
        env['ACV_PROFILES_ROOT'] = str(stage / '.check-profiles')
        check = subprocess.run([sys.executable, '-I', str(stage / 'tools/native-rexglue/acv_profile_cli.py'),
                                'features'], cwd='/', env=env, text=True, capture_output=True, check=True)
        features = json.loads(check.stdout)
        if not features['exists'] or (args.include_game_data and not features['game_data']):
            raise RuntimeError(f'Package self-check failed: {features}')
        frontend_dependencies = None
        if args.frontend == 'trg':
            frontend_dependencies = json.loads((ROOT / 'tools/acv-trg-launcher/dependencies.json').read_text())
            for item in frontend_dependencies['repositories']:
                actual = subprocess.check_output(['git', '-C', str(ROOT / item['directory']), 'rev-parse', 'HEAD'], text=True).strip()
                if actual != item['commit']:
                    raise RuntimeError(f'Frontend dependency drift: {item["directory"]}')
        manifest = {'frontend_dependencies': frontend_dependencies, 'created_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
                    'files': files, 'runtime_hashes': hashes, 'accepted_baseline': accepted, 'launch_environment': launch_environment,
                    'tests': tests, 'system_dependencies': system_dependencies,
                    'game_data_included': args.include_game_data, 'frontend': args.frontend,
                    'not_verified': ['in-game save/load', 'CAS/FSR image', 'gameplay speed', 'other Linux distributions']}
        (stage / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2)+'\n')
        os.rename(stage, out)
        print(json.dumps({'package': str(out), 'files': len(files), 'accepted_baseline': accepted,
                          'features': features['features']}, indent=2))
    except BaseException:
        # Keep failed staging evidence for inspection. Never touch an existing published package.
        print(f'Incomplete staging kept at {stage}', file=sys.stderr)
        raise


if __name__ == '__main__':
    main()
