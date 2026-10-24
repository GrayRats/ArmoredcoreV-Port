#!/usr/bin/env python3
"""Versioned settings/profile contract shared by the CLI probe and the launcher (PLAN_GUI.MD stage A).

Precedence, lowest to highest: built-in defaults -> saved settings -> explicit launcher
choice -> per-launch overrides. Unsupported values are rejected, never clamped silently.
"""
import configparser, copy, json, os, re, tempfile, uuid
from dataclasses import dataclass, asdict
from pathlib import Path

SCHEMA_VERSION = 1

# name -> (default, validator description, predicate)
_RESOLUTION = re.compile(r"auto|[1-9][0-9]{2,4}x[1-9][0-9]{2,4}")
GRAPHICS_SCHEMA = {
    "window_mode": ("windowed", ("windowed", "fullscreen")),
    "monitor": (0, tuple(range(0, 17))),
    "resolution": ("auto", _RESOLUTION),
    "render_scale": (1, (1, 2, 3, 4)),
    "vsync": (True, (True, False)),
    "fps_limit": (60, (60, 120, 144, 165)),
    "anisotropy": ("game", ("game", "off", 2, 4, 8, 16)),
    "antialiasing": ("off", ("off", "fxaa", "fxaa_extreme")),
    "dither": (False, (True, False)),
    "fsr_sharpness": ("default", ("default", 0.0, 0.5, 1.0, 1.5, 2.0)),
    "sharpness": ("default", ("default", 0.0, 0.25, 0.5, 0.75, 1.0)),
    "output_effect": ("bilinear", ("bilinear", "cas", "fsr", "fsr2", "fsr3")),
    "color_encoding": ("rgb_full", ("rgb_full", "ycbcr444", "ycbcr422", "ycbcr420")),
    "color_depth": (8, (8, 10)),
}

# SDK cvar values for anisotropic_override (cache.cpp:52): -1 game, 0 off, 1..5 = 1x,2x,4x,8x,16x.
_ANISO_CVAR = {"game": -1, "off": 0, 2: 2, 4: 3, 8: 4, 16: 5}

# Values the pinned runtime cannot honour yet, with the reason shown by the launcher.
_UNAVAILABLE = {
    ("fps_limit", 120): "guest timestep, animation and audio have not been validated above 60 FPS (PLAN_GUI.MD D3)",
    ("fps_limit", 144): "guest timestep, animation and audio have not been validated above 60 FPS (PLAN_GUI.MD D3)",
    ("fps_limit", 165): "guest timestep, animation and audio have not been validated above 60 FPS (PLAN_GUI.MD D3)",
    ("color_encoding", "ycbcr444"): "no verified output backend for Y'CbCr on the Vulkan desktop path",
    ("color_encoding", "ycbcr422"): "no verified output backend for Y'CbCr on the Vulkan desktop path",
    ("color_encoding", "ycbcr420"): "no verified output backend for Y'CbCr on the Vulkan desktop path",
    ("output_effect", "fsr2"): "temporal FSR needs verified motion vectors, depth and frame history",
    ("output_effect", "fsr3"): "temporal FSR needs verified motion vectors, depth and frame history",
}
_EXPERIMENTAL = {
    ("color_depth", 10): "requires a 10-bit SDR surface format; runtime reports fallback; image precision not verified",
    ("render_scale", 2): "SDK scale support depends on device limits; not verified for ACV",
    ("render_scale", 3): "SDK scale support depends on device limits; not verified for ACV",
    ("render_scale", 4): "SDK scale support depends on device limits; not verified for ACV",
    ("output_effect", "cas"): "needs a runtime built with REXGLUE_ENABLE_FIDELITYFX; visual result not verified",
    ("output_effect", "fsr"): "needs a runtime built with REXGLUE_ENABLE_FIDELITYFX; visual result not verified",
    ("antialiasing", "fxaa"): "final-pass FXAA is not verified against the ACV HUD",
    ("antialiasing", "fxaa_extreme"): "final-pass FXAA is not verified against the ACV HUD",
}


def capabilities():
    """Per-option status for the launcher: available / experimental / unavailable (+ reason)."""
    result = {}
    for key, (default, rule) in GRAPHICS_SCHEMA.items():
        if isinstance(rule, re.Pattern):
            result[key] = {"default": default, "options": None}
            continue
        options = []
        for value in rule:
            if (key, value) in _UNAVAILABLE:
                options.append({"value": value, "status": "unavailable", "reason": _UNAVAILABLE[(key, value)]})
            elif (key, value) in _EXPERIMENTAL:
                options.append({"value": value, "status": "experimental", "reason": _EXPERIMENTAL[(key, value)]})
            else:
                options.append({"value": value, "status": "available"})
        result[key] = {"default": default, "options": options}
    return result


class SettingsError(ValueError):
    pass


def _accepts(rule, value):
    if isinstance(rule, re.Pattern):
        return isinstance(value, str) and rule.fullmatch(value) is not None
    # bool is an int subclass: keep True from matching 1 and vice versa.
    return any(type(value) is type(option) and value == option for option in rule)


def validate_graphics(values):
    for key, value in values.items():
        if key not in GRAPHICS_SCHEMA:
            raise SettingsError(f"unknown graphics setting '{key}'")
        if not _accepts(GRAPHICS_SCHEMA[key][1], value):
            raise SettingsError(f"unsupported value {value!r} for graphics setting '{key}'")
    return values


def resolve_graphics(saved=None, launcher=None, launch=None):
    """Merge layers in precedence order; each layer is validated before merging."""
    result = {key: default for key, (default, _) in GRAPHICS_SCHEMA.items()}
    for layer in (saved, launcher, launch):
        if layer:
            result.update(validate_graphics(dict(layer)))
    return result


def _check_name(label, value, limit):
    if not isinstance(value, str) or not value.strip():
        raise SettingsError(f"{label} must be a non-empty string")
    if len(value) > limit or any(ord(c) < 32 for c in value):
        raise SettingsError(f"{label} must be at most {limit} characters without control characters")
    return value


@dataclass
class Profile:
    """UUID, XUID, display name, gamertag and pilot name are independent identities."""
    uuid: str
    xuid: str
    display_name: str
    gamertag: str
    pilot_name: str
    language: int = 1
    data_dir: str = ""

    def validate(self):
        try:
            uuid.UUID(self.uuid)
        except ValueError:
            raise SettingsError("profile uuid is invalid") from None
        if not re.fullmatch(r"[0-9A-F]{16}", self.xuid):
            raise SettingsError("xuid must be 16 uppercase hex digits")
        _check_name("display_name", self.display_name, 64)
        _check_name("gamertag", self.gamertag, 15)
        _check_name("pilot_name", self.pilot_name, 32)
        if not isinstance(self.language, int) or isinstance(self.language, bool) or self.language < 0:
            raise SettingsError("language must be a non-negative integer")
        return self


def new_xuid():
    # Offline Xbox 360 profile XUIDs live in the 0x0009000000000000 range.
    return "0009" + uuid.uuid4().hex[:12].upper()


def new_profile(display_name, gamertag=None, pilot_name=None, language=1):
    gamertag = gamertag or display_name[:15]
    profile = Profile(str(uuid.uuid4()), new_xuid(), display_name, gamertag,
                      pilot_name or gamertag, language)
    profile.data_dir = f"profiles/{profile.uuid}"
    return profile.validate()


def rename_profile(profile, display_name):
    """Renaming changes only the launcher label: UUID, XUID, gamertag and pilot name stay."""
    renamed = copy.copy(profile)
    renamed.display_name = display_name
    return renamed.validate()


def set_names(profile, gamertag=None, pilot_name=None):
    """Change gamertag and/or pilot name only; UUID and XUID (the save owner) stay. A pilot name
    already written inside a save is not rewritten by this."""
    changed = copy.copy(profile)
    if gamertag is not None:
        changed.gamertag = gamertag
    if pilot_name is not None:
        changed.pilot_name = pilot_name
    return changed.validate()


def migrate_legacy_config(path):
    """Build a profile from an existing acv.config [profile] section without touching the file."""
    parser = configparser.ConfigParser(inline_comment_prefixes=("#", ";"))
    parser.read_string(Path(path).read_bytes().decode("utf-8-sig"), source=str(path))
    pilot = parser.get("profile", "pilot_name", fallback="").strip()
    gamertag = parser.get("profile", "gamertag", fallback="").strip()
    if not pilot and not gamertag:
        raise SettingsError(f"{path}: no [profile] names to migrate")
    name = gamertag or pilot
    return new_profile(name, gamertag or None, pilot or None,
                       parser.getint("sdk", "user_language", fallback=1))


def save_json_atomic(path, document):
    """Write fully to a sibling temp file, fsync, then replace; a crash keeps the old file."""
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, tmp = tempfile.mkstemp(dir=path.parent, prefix=path.name + ".", suffix=".tmp")
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as f:
            json.dump(document, f, ensure_ascii=False, indent=2, sort_keys=True)
            f.write("\n")
            f.flush()
            os.fsync(f.fileno())
        os.replace(tmp, path)
    except BaseException:
        Path(tmp).unlink(missing_ok=True)
        raise


def load_json(path, kind):
    document = json.loads(Path(path).read_text(encoding="utf-8"))
    version = document.get("schema_version")
    if version != SCHEMA_VERSION:
        raise SettingsError(f"{path}: unsupported {kind} schema_version {version!r}")
    return document


def save_profile(root, profile):
    profile.validate()
    save_json_atomic(Path(root) / profile.data_dir / "profile.json",
                     {"schema_version": SCHEMA_VERSION, **asdict(profile)})


def load_profile(root, profile_uuid):
    document = load_json(Path(root) / "profiles" / profile_uuid / "profile.json", "profile")
    document.pop("schema_version")
    try:
        return Profile(**document).validate()
    except TypeError as e:
        raise SettingsError(f"profile {profile_uuid}: {e}") from None


def runtime_env(profile):
    """Environment consumed by acv_compat_shims.cpp."""
    return {"ACV_PILOT_NAME": profile.pilot_name, "ACV_GAMERTAG": profile.gamertag}


# Runtime errors the project classified as harmless (docs/NATIVE_REXGLUE_LINUX.md): logged, not fatal.
BENIGN_RUNTIME_ERRORS = (re.compile(r"ResolvePath\(system:\\\)\s+failed\s+-\s+device not found"),)


def is_fatal_runtime_error(message):
    return not any(pattern.search(message) for pattern in BENIGN_RUNTIME_ERRORS)


def dedupe_options(options):
    """The SDK's CLI parser rejects a repeated --key: the whole cvar parse fails and every option,
    including --gpu_plugin, is dropped (seen 2026-10-24 with a doubled --user_language).
    Keep the last occurrence, because later layers outrank earlier ones."""
    last = {}
    for index, option in enumerate(options):
        if option.startswith("--"):
            last[option.split("=", 1)[0]] = index
    return [o for i, o in enumerate(options) if not o.startswith("--") or last[o.split("=", 1)[0]] == i]


def runtime_features(directory):
    """Features of the libraries staged in `directory`, found by their cvar/env names."""
    wanted = {"librexruntime.so": ("ACV_XUID", "cas_additional_sharpness", "present_output_bits"),
              "librexgpu-xenos.so": ("frame_limit_fps",)}
    found = set()
    for library, names in wanted.items():
        path = Path(directory) / library
        data = path.read_bytes() if path.is_file() else b""
        found.update(name for name in names if name.encode() in data)
    if "cas_additional_sharpness" in found:  # cvar exists only when REX_HAS_FIDELITYFX_SDK is compiled in
        found.add("fidelityfx")
    return found


def graphics_to_cvars(values, allow_experimental=False, features=frozenset()):
    """Translate resolved graphics settings into SDK --cvar=value arguments.
    Unavailable values raise; experimental ones need allow_experimental.
    `features` names runtime capabilities (e.g. "frame_limit_fps") detected in the staged runtime."""
    values = resolve_graphics(values)
    for key, value in values.items():
        if (key, value) in _UNAVAILABLE:
            raise SettingsError(f"{key}={value!r} is unavailable: {_UNAVAILABLE[(key, value)]}")
        if (key, value) in _EXPERIMENTAL and not allow_experimental:
            raise SettingsError(f"{key}={value!r} is experimental: {_EXPERIMENTAL[(key, value)]}")
    args = [f"--fullscreen={str(values['window_mode'] == 'fullscreen').lower()}",
            f"--monitor={values['monitor']}",
            f"--vsync={str(values['vsync']).lower()}",
            f"--resolution_scale={values['render_scale']}",
            f"--anisotropic_override={_ANISO_CVAR[values['anisotropy']]}",
            f"--swap_post_effect={'none' if values['antialiasing'] == 'off' else values['antialiasing']}",
            f"--present_dither={str(values['dither']).lower()}"]
    # Guest video mode and output window size are independent. Preserve the native 16:9
    # guest mode while the presenter fits it into the selected host rectangle.
    args += ["--present_letterbox=true", "--present_safe_area_x=100", "--present_safe_area_y=100",
             "--resolution=1280x720", "--video_mode_width=1280", "--video_mode_height=720",
             "--vulkan_allow_present_mode_immediate=" + str(not values["vsync"]).lower(),
             "--vulkan_allow_present_mode_mailbox=" + str(not values["vsync"]).lower(),
             "--vulkan_allow_present_mode_fifo_relaxed=false",
             "--present_effect=" + values["output_effect"]]
    if "present_output_bits" in features:
        args.append(f"--present_output_bits={values['color_depth']}")
    elif values["color_depth"] != 8:
        raise SettingsError("10-bit output requires a runtime with present_output_bits support")
    if values["color_depth"] == 10 and values["dither"]:
        raise SettingsError("Disable 8-bit output dithering before requesting 10-bit output")
    if values["output_effect"] != "bilinear":
        if "fidelityfx" not in features:
            raise SettingsError(f"output_effect={values['output_effect']!r} needs a FidelityFX-enabled runtime")
        if values["output_effect"] == "cas" and values["sharpness"] != "default":
            args.append(f"--present_cas_additional_sharpness={values['sharpness']}")
        if values["output_effect"] == "fsr" and values["fsr_sharpness"] != "default":
            args.append(f"--present_fsr_sharpness_reduction={values['fsr_sharpness']}")
    if "frame_limit_fps" in features:
        # Only 60 can be selected (see _UNAVAILABLE); pacing keeps VSync-off from running the game fast.
        args.append(f"--frame_limit_fps={values['fps_limit']}")
    if values["resolution"] != "auto":
        width, height = values["resolution"].split("x")
        args += [f"--window_width={width}", f"--window_height={height}"]
    else:
        args += ["--window_width=0", "--window_height=0"]
    return args
