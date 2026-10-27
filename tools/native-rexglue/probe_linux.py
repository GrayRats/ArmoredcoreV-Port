#!/usr/bin/env python3
"""Bounded launch of our private Linux build; alive is not gameplay verification."""
import argparse, collections, configparser, datetime, hashlib, json, os, re, signal, subprocess, sys, threading
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import acv_profiles, acv_settings, acv_shm, acv_paths, acv_display
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--executable", type=Path, default=acv_paths.EXECUTABLE)
parser.add_argument("--seconds", type=int, default=60)
parser.add_argument("--extra", action="append", default=[], metavar="--cvar=VALUE",
                    help="repeat --extra=--cvar=VALUE for additional SDK options")
parser.add_argument("--config", type=Path, default=ROOT / "tools/native-rexglue/acv.config",
                    help="single configuration file (profile names, SDK options, compat toggles, capture)")
parser.add_argument("--display-gamma-type", choices=("sdk", "0", "1", "2", "3"),
                    help="diagnostic VdGetCurrentDisplayGamma override; sdk preserves the SDK return")
parser.add_argument("--compat-mm-wc-rw", action="store_true",
                    help="experiment: add READWRITE to MmAllocatePhysicalMemoryEx protect=LARGE|WRITECOMBINE (acv_compat_shims.cpp)")
parser.add_argument("--renderdoc", action="store_true",
                    help="inject RenderDoc (LD_PRELOAD + Vulkan layer); F12 in the game window captures a frame")
parser.add_argument("--renderdoc-at", default="",
                    help="comma-separated seconds for automatic captures (needs --renderdoc)")
parser.add_argument("--renderdoc-frames", type=int, choices=range(1,9),
                    help="consecutive presents per automatic capture, 1..8; config/default 1")
parser.add_argument("--renderdoc-window-ms", type=int, default=0,
                    help="deprecated; only 0 is accepted (explicit intervals disabled after device loss)")
parser.add_argument("--log-level", choices=("info", "debug", "trace"), default="info")
parser.add_argument("--video-driver", choices=("auto", "x11", "wayland"), default="auto",
                    help="child-only SDL backend; auto uses X11 for RenderDoc capture")
parser.add_argument("--controls", choices=("keyboard-mouse", "gamepad"), default="keyboard-mouse",
                    help="keyboard/mouse emulates player-one controller; F8 toggles mouse look")
parser.add_argument("--graphics-profile", choices=("acv-fsi", "sdk"), default="acv-fsi",
                    help="ACV compatibility defaults (FSI, protect_zero=false), or unmodified SDK defaults; --extra overrides")
parser.add_argument("--user-root", type=Path,
                    help="optional persistent profile directory inside workspace tmp/")
parser.add_argument("--profile", metavar="UUID",
                    help="persistent profile from --profiles-root: its own user_data_root, cache, XUID and names; locked while running")
parser.add_argument("--profiles-root", type=Path, default=acv_paths.PROFILES,
                    help="profile store root inside workspace tmp/")
parser.add_argument("--settings", type=Path,
                    help="launcher settings JSON (schema_version 1, \"graphics\" object); validated by acv_settings.py")
parser.add_argument("--allow-experimental", action="store_true",
                    help="permit settings marked experimental in acv_settings.capabilities()")
parser.add_argument("--startup-profile", action="store_true",
                    help="sample startup with GDB on X11 using this same launch configuration")
parser.add_argument("--projection-trace", type=Path, metavar="LAYOUT_JSON",
                    help="bounded GDB projection argument trace using a binary-pinned PPCContext layout")
parser.add_argument("--session", action="store_true",
                    help="normal play session: no time limit (--seconds is ignored); the exit code is the game's")
parser.add_argument("--screenshot-every", type=int, default=0,
                    help="deprecated; only 0 is accepted (analysis screenshots disabled)")
args = parser.parse_args()
if args.renderdoc_window_ms != 0:
    parser.error("Explicit capture intervals are disabled after device loss; use single-frame captures")
if args.screenshot_every != 0:
    parser.error("Screenshot saving is disabled by project instruction; use RenderDoc data or logs")
config = configparser.ConfigParser(inline_comment_prefixes=("#", ";"))
config_sdk_options = []
if args.config.is_file():
    config_bytes = args.config.read_bytes()
    # Accept an optional UTF-8 BOM while retaining raw bytes for the launch hash.
    config.read_string(config_bytes.decode("utf-8-sig"), source=str(args.config))
    reserved = {"game_data_root", "user_data_root", "cache_root", "log_file", "log_level", "gpu_plugin"}
    for key, value in config.items("sdk") if config.has_section("sdk") else []:
        if not re.fullmatch(r"[a-z0-9_]+", key) or key in reserved:
            parser.error(f"{args.config}: [sdk] key '{key}' is not allowed")
        config_sdk_options.append(f"--{key}={value}")
    if config.getboolean("compat", "mm_wc_rw", fallback=False):
        args.compat_mm_wc_rw = True
    if config.getboolean("capture", "renderdoc", fallback=False) and not args.renderdoc:
        args.renderdoc = True
        args.renderdoc_at = args.renderdoc_at or config.get("capture", "renderdoc_at", fallback="")
else:
    parser.error(f"config file not found: {args.config}")
if args.renderdoc_frames is None:
    args.renderdoc_frames = config.getint("capture", "renderdoc_frames", fallback=1)
if not 1 <= args.renderdoc_frames <= 8:
    parser.error("[capture] renderdoc_frames must be 1..8")
if args.projection_trace:
    args.startup_profile = True
if args.startup_profile and args.session:
    parser.error("--startup-profile requires a bounded run")
if args.session and args.seconds != 60:
    parser.error("--session has no time limit; do not combine it with --seconds")
if args.profile and args.user_root:
    parser.error("--profile and --user-root are mutually exclusive")
if not args.session and not 1 <= args.seconds <= 300:
    parser.error("Probe duration must be 1..300 seconds")
exe = args.executable.resolve()
if not exe.is_file():
    parser.error("Expected an existing Linux executable")
with exe.open("rb") as f:
    if f.read(4) != b"\x7fELF":
        parser.error("Expected a Linux ELF executable")
game = acv_paths.GAME
xex = game / "default.xex"
if hashlib.sha256(xex.read_bytes()).hexdigest() != "4fac87348f827e84633013375012284cd8c769a3c155a675b2989757e666a12b":
    parser.error("Extracted XEX hash differs from pinned input")
run_id = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
output = acv_paths.LOGS / "acv-native-linux" / run_id
output.mkdir(parents=True, exist_ok=False)
profile_lock = None
selected_profile = None
cache_dir = None
if args.profile:
    store_root = args.profiles_root.resolve()
    store = acv_profiles.ProfileStore(store_root)
    try:
        selected_profile = store.get(args.profile)
        profile_lock = store.lock(args.profile).acquire()
    except (acv_settings.SettingsError, OSError, ValueError) as error:
        parser.error(f"profile {args.profile}: {error}")
    except acv_profiles.ProfileBusy as error:
        parser.error(str(error))
    user = store.userdata(args.profile)
    cache_dir = store.cache(args.profile)
    cache_dir.mkdir(parents=True, exist_ok=True)
else:
    user = args.user_root.resolve() if args.user_root else acv_paths.PROFILES / "diagnostics" / run_id
user.mkdir(parents=True, exist_ok=bool(args.user_root or args.profile))
profile = json.loads((ROOT / "tools/native-rexglue/input-profile.json").read_text())
if args.controls == "gamepad":
    profile = {"mnk_mode": False, "mnk_mouse": False}
input_options = ["--"+name+"="+(str(value).lower() if isinstance(value, bool) else str(value))
                 for name, value in profile.items()]
# ACV bakes textures on the GPU, then reads their pixels for CPU compression.
# Without full resolve readback the original RGBA allocation stays zero.
# Explicit [sdk] config entries and --extra options below may override this.
compatibility_options = (["--protect_zero=false", "--render_target_path_vulkan=fsi",
                          "--readback_resolve=full"]
                         if args.graphics_profile == "acv-fsi" else [])
features = acv_settings.runtime_features(exe.parent)
settings_options = []
settings_env = {}
if args.settings:
    try:
        saved = acv_settings.load_json(args.settings, "settings")
        settings_options = acv_settings.graphics_to_cvars(saved.get("graphics", {}), args.allow_experimental, features)
        settings_env = acv_settings.graphics_to_env(saved.get("graphics", {}), features)
    except (acv_settings.SettingsError, OSError, ValueError) as error:
        parser.error(f"--settings: {error}")
profile_options = [f"--user_language={selected_profile.language}"] if selected_profile else []
command = [str(exe), "--game_data_root="+str(game), "--user_data_root="+str(user),
           "--cache_root="+str(cache_dir or user / "cache"), "--gpu_plugin=xenos",
           "--log_file="+str(output / "runtime.log"), "--log_level="+args.log_level,
           *input_options, *compatibility_options, *config_sdk_options, *profile_options, *settings_options, *args.extra]
command = [command[0], *acv_settings.dedupe_options(command[1:])]
report = {"scope": "bounded native launch; duration/alive alone does not establish boot, graphics or gameplay", "command": command, "binary_sha256": hashlib.sha256(exe.read_bytes()).hexdigest(), "duration_limit_seconds": None if args.session else args.seconds, "user_root": str(user)}
report["runtime_features"] = sorted(features)
report["settings_file"] = str(args.settings) if args.settings else None
report["settings_cvars"] = settings_options
report["settings_env"] = settings_env
report["controls"] = args.controls
report["graphics_profile"] = args.graphics_profile
env = dict(os.environ)
env.update(settings_env)  # wide-view hooks of the host (not SDK cvars)
# Prefer the selected build over absolute SDK RUNPATHs and inherited library search paths.
env["LD_LIBRARY_PATH"] = str(exe.parent) + (":" + env["LD_LIBRARY_PATH"] if env.get("LD_LIBRARY_PATH") else "")
display_gamma = args.display_gamma_type or config.get("compat", "display_gamma_type", fallback=None)
if display_gamma is not None:
    if display_gamma not in ("sdk", "0", "1", "2", "3"):
        parser.error("[compat] display_gamma_type must be sdk or 0..3")
    if display_gamma == "sdk":
        env.pop("ACV_DISPLAY_GAMMA_TYPE", None)
    else:
        env["ACV_DISPLAY_GAMMA_TYPE"] = display_gamma
report["display_gamma_type_override"] = env.get("ACV_DISPLAY_GAMMA_TYPE")
if args.compat_mm_wc_rw:
    env["ACV_COMPAT_MM_WC_RW"] = "1"
dlc_sound_root = acv_paths.DATA / "acv-dlc-sound"
if dlc_sound_root.is_dir():
    env["ACV_DLC_SOUND_ROOT"] = str(dlc_sound_root)
# Empty on purpose: the guest's optional system:\\system.properties stays absent, only the device exists.
system_root = (cache_dir if cache_dir is not None else user / "cache") / "system"
system_root.mkdir(parents=True, exist_ok=True)
env["ACV_SYSTEM_ROOT"] = str(system_root)
report["system_root"] = str(system_root)
report["dlc_sound_root"] = env.get("ACV_DLC_SOUND_ROOT")
dlc_image_root = acv_paths.DATA / "acv-dlc-image"
if dlc_image_root.is_dir():
    env["ACV_DLC_IMAGE_ROOT"] = str(dlc_image_root)
report["dlc_image_root"] = env.get("ACV_DLC_IMAGE_ROOT")
for variable, option in (("ACV_PILOT_NAME", "pilot_name"), ("ACV_GAMERTAG", "gamertag")):
    value = config.get("profile", option, fallback="").strip()
    if value:
        env[variable] = value
if selected_profile:
    env.update(acv_settings.runtime_env(selected_profile))
    # Runtimes without ACV_XUID support keep the SDK XUID; saves stay isolated by user_data_root.
    if "ACV_XUID" in features:
        env["ACV_XUID"] = selected_profile.xuid
    report["profile"] = {"uuid": selected_profile.uuid, "profile_xuid": selected_profile.xuid,
                         "runtime_xuid": selected_profile.xuid if "ACV_XUID" in features else "B13EBABEBABEBABE"}
report["config"] = {"file": str(args.config), "sha256": hashlib.sha256(config_bytes).hexdigest(),
                    "sdk_options": config_sdk_options,
                    "pilot_name": env.get("ACV_PILOT_NAME"), "gamertag": env.get("ACV_GAMERTAG")}
video_driver = args.video_driver
if video_driver == "auto" and not env.get("SDL_VIDEODRIVER") and env.get("DISPLAY"):
    # Wayland backend: the game window is never shown and ignores close (no frame is presented);
    # X11/XWayland works (observed 2026-10-24). Pass --video-driver wayland to override.
    video_driver = "x11"
if video_driver == "auto" and args.renderdoc:
    # RenderDoc capture on this host filters VK_KHR_wayland_surface. SDL must
    # create an X11 window to match the remaining VK_KHR_xcb_surface support.
    video_driver = "x11"
if video_driver != "auto":
    if video_driver == "x11" and not env.get("DISPLAY"):
        parser.error("X11 capture requires DISPLAY; no desktop settings are changed")
    env["SDL_VIDEODRIVER"] = video_driver
report["video_driver"] = env.get("SDL_VIDEODRIVER", "auto")
if args.renderdoc:
    captures = acv_paths.LOGS / "acv-renderdoc" / run_id
    captures.mkdir(parents=True, exist_ok=False)
    env.update({"LD_PRELOAD": "/usr/lib/librenderdoc.so", "ENABLE_VULKAN_RENDERDOC_CAPTURE": "1",
                "ACV_RENDERDOC_CAPTURE_PATH": str(captures / "acv"),
                "ACV_RENDERDOC_CAPTURE_SECONDS": args.renderdoc_at,
                "ACV_RENDERDOC_CAPTURE_FRAMES": str(args.renderdoc_frames)})
    report["renderdoc_captures"] = str(captures)
    report["renderdoc_at"] = args.renderdoc_at
    report["renderdoc_frames"] = args.renderdoc_frames
    report["renderdoc_window_ms"] = args.renderdoc_window_ms
report["compat_mm_wc_rw"] = args.compat_mm_wc_rw
report["shm_before"] = acv_shm.status()
if report["shm_before"]["free"] < 6 * 1024**3:
    parser.error("At least 6 GiB free in /dev/shm is required")
launch_command = command
if args.startup_profile:
    import shutil
    if not shutil.which("gdb"):
        parser.error("Startup profiling requires gdb")
    if report["video_driver"] != "x11" or report["shm_before"]["orphans"]:
        parser.error("Startup profiling requires X11 and no orphaned xenia_memory files")
    if args.projection_trace:
        layout = json.loads(args.projection_trace.read_text())
        if layout.get("binary_sha256") != report["binary_sha256"]:
            parser.error("Projection layout is not pinned to this executable")
        env["ACV_PROJECTION_LAYOUT"] = json.dumps(layout)
        report["projection_trace"] = str(output / "projection.json")
    env["ACV_PROFILE_INNER"] = "1"
    env["ACV_PROFILE_CFG"] = json.dumps({"seconds": args.seconds, "interval": 1.0,
                                         "depth": 10, "out": str(output / "samples.json")})
    launch_command = ["gdb", "-batch", "-nx", "-x",
                      str(Path(__file__).with_name("profile_startup.py")), "--args", *command]
    report["startup_profile"] = {"samples": str(output / "samples.json"),
                                  "scope": "GDB sampling adds overhead; not an FPS benchmark"}
# The runtime retains the flock if this supervisor is killed while the game is still writing.
if os.environ.get("ACV_PROBE_DRYRUN"):
    # Used by hang_watch.py: record the exact game command and environment, then stop without starting.
    Path(os.environ["ACV_PROBE_DRYRUN"]).write_text(json.dumps({"command": launch_command, "cwd": str(exe.parent),
                                                                "env": env}, ensure_ascii=False))
    if profile_lock is not None:
        profile_lock.release()
    sys.exit(0)
child = subprocess.Popen(launch_command, cwd=exe.parent, stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE, env=env,
                         pass_fds=(profile_lock.fd,) if profile_lock is not None else ())
report["owned_pid"] = child.pid
# No new session: the outer SQLite runner can also terminate this child group.
for sig in (signal.SIGTERM, signal.SIGINT):
    signal.signal(sig, lambda signum, frame: child.send_signal(signum) if child.poll() is None else None)
def pump(source, target, filename):
    with (output / filename).open("wb") as saved:
        while chunk := source.read1(65536):
            saved.write(chunk)
            saved.flush()  # Keep live stderr/hook evidence visible before child exit.
            target.write(chunk)
            target.flush()
threads = [threading.Thread(target=pump, args=(child.stdout,sys.stdout.buffer,"stdout.txt")),
           threading.Thread(target=pump, args=(child.stderr,sys.stderr.buffer,"stderr.txt"))]
runtime_stop = threading.Event()
runtime_errors = []
# Harmless errors (acv_settings.BENIGN_RUNTIME_ERRORS) are logged and counted, but do not make the probe
# exit non-zero: the launcher showed "код 1" after a normal close because of the first one.
fatal_error_count = 0
runtime_error_counts = collections.Counter()
fault_count = 0
abort_reason = None
kill_timer = None
def follow_runtime():
    """Forward the SDK's file-only log into the outer SQLite captured output."""
    global fault_count, abort_reason, kill_timer, fatal_error_count
    runtime_path = output / "runtime.log"
    while not runtime_path.exists():
        if runtime_stop.wait(0.05):
            return
    with runtime_path.open("rb") as stream:
        while True:
            line = stream.readline()
            if line:
                is_error = any(marker in line.lower() for marker in (b"[critical]", b"[error]", b"[fatal]"))
                forward = True
                if is_error:
                    message = line.decode("utf-8", errors="replace").rstrip()
                    key = re.sub(r"^.*?\[(?:error|critical|fatal)\]\s*", "", message, flags=re.I)
                    # Bound JSON/SQLite duplication; the complete SDK log remains on disk.
                    if key not in runtime_error_counts and len(runtime_error_counts) >= 128:
                        key = "[additional distinct errors]"
                    runtime_error_counts[key] += 1
                    if len(runtime_errors) < 32:
                        runtime_errors.append(message)
                    if acv_settings.is_fatal_runtime_error(message):
                        fatal_error_count += 1
                    forward = runtime_error_counts[key] <= 8
                    if runtime_error_counts[key] == 9:
                        sys.stdout.buffer.write(b"[probe] Repeated error forwarding suppressed; full evidence in runtime.log\n")
                if forward:
                    sys.stdout.buffer.write(line)
                    sys.stdout.buffer.flush()
                if b"Unhandled guest access violation" in line:
                    fault_count += 1
                    if fault_count == 8 and child.poll() is None:
                        abort_reason = "Eight unhandled guest access violations; stopped owned child to prevent a fault/log loop"
                        child.terminate()
                        def kill_faulting_child():
                            if child.poll() is None:
                                child.kill()
                        kill_timer = threading.Timer(2, kill_faulting_child)
                        kill_timer.daemon = True
                        kill_timer.start()
            elif runtime_stop.is_set():
                break
            else:
                runtime_stop.wait(0.05)
runtime_thread = threading.Thread(target=follow_runtime)
runtime_thread.start()
for t in threads:
    t.start()
try:
    child.wait(timeout=None if args.session else args.seconds + (20 if args.startup_profile else 0))
    report["limit_reached"] = False
except subprocess.TimeoutExpired:
    report["limit_reached"] = True
    child.terminate()  # Only the process spawned directly above.
    try:
        child.wait(timeout=10)
    except subprocess.TimeoutExpired:
        child.kill()
        child.wait()
for t in threads:
    t.join()
runtime_stop.set()
runtime_thread.join()
if kill_timer is not None:
    kill_timer.cancel()
report["abort_reason"] = abort_reason
report["runtime_error_count"] = sum(runtime_error_counts.values())
report["runtime_error_counts"] = dict(runtime_error_counts)
report["runtime_errors_truncated"] = report["runtime_error_count"] > len(runtime_errors)
report["child_returncode"] = child.returncode
report["shm_after"] = acv_shm.status()
report["runtime_errors"] = runtime_errors
report["fatal_error_count"] = fatal_error_count
runtime_log = output / "runtime.log"
report["display"] = acv_display.summarize(runtime_log.read_text(errors="replace") if runtime_log.exists() else "")
if profile_lock is not None:
    profile_lock.release()
(output / "launch.json").write_text(json.dumps(report,indent=2)+"\n")
print(json.dumps({"evidence":str(output), "child_returncode":child.returncode, "duration_limit_reached":report["limit_reached"], "isError":bool(fatal_error_count), "runtime_errors":runtime_errors, "fatal_error_count":fatal_error_count}))
# A bounded live process is an observation, not a successful-game assertion.
sys.exit(1 if fatal_error_count else (0 if report["limit_reached"] else (child.returncode if child.returncode >= 0 else 128-child.returncode)))
