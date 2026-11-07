"""Optional host MangoHud activation for the Vulkan game process."""
import shutil
import sys
import math
import os
import re
import time
from pathlib import Path


def availability():
    command = shutil.which("mangohud") if sys.platform == "linux" else None
    return {"available": command is not None, "command": command,
            "reason": ("MangoHud найден. Показывается только Guest — частота подачи кадров игрой (среднее за 300 кадров); собственный счётчик Host FPS отключён: он считает показы окна, а не новые кадры игры."
                       if command else
                       "MangoHud не найден. Установите системный пакет mangohud или выключите оверлей.")}


def game_environment(enabled, environment):
    """Copy child env, preserve user HUD config/preloads, and record request scope."""
    if type(enabled) is not bool:
        raise ValueError("MangoHud selection must be boolean")
    status = availability()
    if enabled and not status["available"]:
        raise ValueError(status["reason"])
    child = dict(environment)
    child["MANGOHUD"] = "1" if enabled else "0"
    if enabled:
        child.pop("DISABLE_MANGOHUD", None)
    else:
        child["DISABLE_MANGOHUD"] = "1"
    return child, {**status, "requested": enabled, "stripped_inherited": [],
                   "method": "Vulkan implicit layer environment",
                   "scope": "Activation request; actual layer loading requires runtime evidence"}


class GuestCadence:
    """Export existing GPU command-processor stats; no game hooks, guest writes or extra sampling.

    XE_SWAP log batches contain 300 paced CPU intervals. Host presents may repeat the
    guest image. This is guest swap cadence, not unique-image or simulation FPS.
    """
    def __init__(self, path):
        self.path = Path(path).resolve()
        self.sample = None
        self.received = None
        self.next_refresh = 0
        self.refresh()

    def consume(self, line, now=None):
        if isinstance(line, bytes): line = line.decode('utf-8', errors='replace')
        if '[acv-display] pacing ' not in line: return
        fields = dict(re.findall(r'(\w+)=([^\s]+)', line))
        try:
            fps, ms, samples = float(fields['fps']), float(fields['mean_ms']), int(fields['samples'])
            if samples != 300 or not all(math.isfinite(v) and v > 0 for v in (fps, ms)): return
            if abs(fps * ms - 1000) > 2: return
        except (KeyError, ValueError): return
        self.sample = {'swaps_per_second': fps, 'mean_ms': ms, 'samples': samples,
                       'window_seconds': samples * ms / 1000}
        self.received = time.monotonic() if now is None else now
        self.next_refresh = 0
        self.refresh(self.received)

    def refresh(self, now=None, stopped=False):
        now = time.monotonic() if now is None else now
        if not stopped and now < self.next_refresh: return
        self.next_refresh = now + 1
        if stopped: text = 'Guest: stopped'
        elif self.sample is None: text = 'Guest: waiting for 300 swaps'
        else:
            age = max(0, now - self.received)
            if age > max(3, self.sample['window_seconds'] * 1.5):
                text = 'Guest: no fresh swap sample'
            else:
                text = (f"Guest {self.sample['swaps_per_second']:.1f}/s {self.sample['mean_ms']:.1f}ms "
                        f"[avg300 age {age:.0f}s]")
        temporary = self.path.with_suffix('.tmp')
        temporary.write_text(text + '\n')
        os.replace(temporary, self.path)


def guest_overlay(environment, output):
    """MangoHud 0.8.4 supports read_cfg and ordered exec rows.

    The native MangoHud FPS counter (vkQueuePresent rate) is disabled: it counts host window
    presents, which repeat the last guest image and do not follow the game frame rate.

    A quoted variable supplies the path to cat, so paths containing shell metacharacters
    or MangoHud delimiters never become command/config syntax. User configs stay on disk.
    """
    child = dict(environment)
    stats = GuestCadence(Path(output) / 'mangohud-guest.txt')
    child['ACV_MANGOHUD_STATS_FILE'] = str(stats.path)
    # Host FPS counts window presents that repeat the last guest image. Off by default; set
    # ACV_MANGOHUD_HOST_FPS=1 to show it (labelled as a presenter counter, not game FPS).
    host_fps = '1' if child.get('ACV_MANGOHUD_HOST_FPS') == '1' else '0'
    options = ('read_cfg=1,legacy_layout=0,fps_only=0,gpu_stats=1,cpu_stats=1,ram=1,vram=1,'
               f'fps={host_fps},frametime=0,frame_timing=0,'
               'exec=cat -- "$ACV_MANGOHUD_STATS_FILE"')
    prefix = child.get('MANGOHUD_CONFIG', '').rstrip(',')
    child['MANGOHUD_CONFIG'] = (prefix + ',' if prefix else '') + options
    return child, stats
