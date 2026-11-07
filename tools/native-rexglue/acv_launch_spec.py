"""Probe command line and environment for the Windows launcher.

Mirrors the launch construction of probe_linux.py (config [sdk]/[profile]/[compat], saved graphics and input
settings, profile names and XUID, DLC/system mounts) without the Linux-only parts (MangoHud, SDL video driver,
RenderDoc, /dev/shm, flock). The launcher has no Python call per option, so it runs this once on Play."""
import configparser, datetime, re
from pathlib import Path
import acv_paths, acv_settings as S, acv_profiles as P, acv_input

RESERVED = {"game_data_root", "user_data_root", "cache_root", "log_file", "log_level", "gpu_plugin"}
COMPAT_FSI = ["--protect_zero=false", "--render_target_path_vulkan=fsi", "--readback_resolve=full"]


def read_config(path):
    config = configparser.ConfigParser(inline_comment_prefixes=("#", ";"))
    path = Path(path)
    if not path.is_file():
        raise S.SettingsError(f"config file not found: {path}")
    config.read_string(path.read_bytes().decode("utf-8-sig"), source=str(path))
    sdk_options = []
    for key, value in config.items("sdk") if config.has_section("sdk") else []:
        if not re.fullmatch(r"[a-z0-9_]+", key) or key in RESERVED:
            raise S.SettingsError(f"{path}: [sdk] key '{key}' is not allowed")
        sdk_options.append(f"--{key}={value}")
    return config, sdk_options


def build(profiles_root, config_path=None, run_id=None):
    """{"args": [...], "env": {...}, "log_file": str, ...}; env holds only additions to the launcher environment."""
    root = Path(profiles_root)
    settings_path = root / "settings.json"
    saved = S.load_json(settings_path, "settings") if settings_path.is_file() else {"graphics": {}, "launcher": {}}
    launcher = saved.get("launcher", {})
    if not isinstance(launcher, dict):
        raise S.SettingsError("launcher must be an object")
    config, config_sdk_options = read_config(config_path or acv_paths.ROOT / "tools/native-rexglue/acv.config")
    features = S.runtime_features(acv_paths.EXECUTABLE.parent)
    run_id = run_id or datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")

    selected = None
    profile_uuid = launcher.get("profile")
    if profile_uuid:
        store = P.ProfileStore(root)
        selected = store.get(profile_uuid)
        user, cache = store.userdata(profile_uuid), store.cache(profile_uuid)
    else:  # no profile chosen: one persistent default so saves survive between launches
        user, cache = acv_paths.DATA / "user", acv_paths.DATA / "cache"
    user.mkdir(parents=True, exist_ok=True)
    cache.mkdir(parents=True, exist_ok=True)

    settings_options = S.graphics_to_cvars(saved.get("graphics", {}), bool(launcher.get("allow_experimental", False)), features)
    env = dict(S.graphics_to_env(saved.get("graphics", {}), features))
    input_mode = S.launcher_input_mode(launcher)
    input_options = acv_input.cvar_options(input_mode)
    env.update(acv_input.mode_environment(input_mode, {}))

    log_file = acv_paths.ROOT / "logs" / f"runtime-{run_id}.log"
    profile_options = [f"--user_language={selected.language}"] if selected else []
    options = ["--game_data_root=" + str(acv_paths.GAME), "--user_data_root=" + str(user), "--cache_root=" + str(cache),
               "--gpu_plugin=xenos", "--log_file=" + str(log_file), "--log_level=info",
               *input_options, *COMPAT_FSI, *config_sdk_options, *profile_options, *settings_options]
    options = S.dedupe_options(options)

    gamma = config.get("compat", "display_gamma_type", fallback=None)
    if gamma not in (None, "sdk"):
        if gamma not in ("0", "1", "2", "3"):
            raise S.SettingsError("[compat] display_gamma_type must be sdk or 0..3")
        env["ACV_DISPLAY_GAMMA_TYPE"] = gamma
    if config.getboolean("compat", "mm_wc_rw", fallback=False):
        env["ACV_COMPAT_MM_WC_RW"] = "1"
    for variable, folder in (("ACV_DLC_SOUND_ROOT", "acv-dlc-sound"), ("ACV_DLC_IMAGE_ROOT", "acv-dlc-image")):
        if (acv_paths.DATA / folder).is_dir():
            env[variable] = str(acv_paths.DATA / folder)
    system_root = cache / "system"  # empty on purpose: the optional system:\system.properties stays absent
    system_root.mkdir(parents=True, exist_ok=True)
    env["ACV_SYSTEM_ROOT"] = str(system_root)
    for variable, option in (("ACV_PILOT_NAME", "pilot_name"), ("ACV_GAMERTAG", "gamertag")):
        value = config.get("profile", option, fallback="").strip()
        if value:
            env[variable] = value
    if selected:
        env.update(S.runtime_env(selected))
        if "ACV_XUID" in features:
            env["ACV_XUID"] = selected.xuid
    return {"args": options, "env": env, "log_file": str(log_file), "user_data_root": str(user),
            "input_mode": input_mode, "profile": selected.uuid if selected else None}
