"""Input mode layouts (input_keyboard.ini / input_gamepad.ini) and controller detection for the launcher."""
import configparser
from pathlib import Path

HERE = Path(__file__).resolve().parent
MODES = ("keyboard_mouse", "keyboard", "gamepad")
DEFAULT_MODE = "keyboard_mouse"
LABELS = {"keyboard_mouse": "Клавиатура и мышь", "keyboard": "Клавиатура", "gamepad": "Геймпад"}
_LAYOUT_FILES = {"keyboard_mouse": "input_keyboard.ini", "keyboard": "input_keyboard.ini", "gamepad": "input_gamepad.ini"}
_MOUSE_TOKENS = {"LMB", "RMB", "MMB", "MB4", "MB5"}
PROC_DEVICES = Path("/proc/bus/input/devices")


def normalize_mode(value):
    """Accept the CLI spelling (keyboard-mouse) as well as the stored one (keyboard_mouse)."""
    if not isinstance(value, str) or value.replace("-", "_") not in MODES:
        raise ValueError(f"input mode must be one of {', '.join(MODES)}")
    return value.replace("-", "_")


def layout(mode, directory=HERE):
    """(cvars, env) for a mode. cvars keep ini order; bool-like values are lowercase strings."""
    mode = normalize_mode(mode)
    parser = configparser.ConfigParser(interpolation=None, delimiters=("=",), inline_comment_prefixes=None)
    parser.optionxform = str
    path = Path(directory) / _LAYOUT_FILES[mode]
    with path.open(encoding="utf-8") as stream:
        parser.read_file(stream, source=str(path))
    if "input" not in parser:
        raise ValueError(f"{path}: missing [input] section")
    cvars = {key: value.strip() for key, value in parser["input"].items()}
    env = {key: value.strip() for key, value in parser["env"].items()} if "env" in parser else {}
    if mode == "keyboard":
        cvars["mnk_mouse"] = "false"
        for key, value in list(cvars.items()):
            if key.startswith("keybind_"):
                kept = [token for token in (t.strip() for t in value.split(",")) if token and token not in _MOUSE_TOKENS]
                cvars[key] = ",".join(kept)
        cvars = {key: value for key, value in cvars.items() if not (key.startswith("keybind_") and not value)}
    return cvars, env


def cvar_options(mode, directory=HERE):
    cvars, _ = layout(mode, directory)
    return ["--" + name + "=" + value for name, value in cvars.items()]


def mode_environment(mode, base=None, directory=HERE):
    """Copy of `base` with the layout's variables; values already set by the user are kept."""
    _, env = layout(mode, directory)
    child = dict(base or {})
    for key, value in env.items():
        child.setdefault(key, value)
    return child


def detect_controllers(source=PROC_DEVICES):
    """Joystick devices from the kernel input list: [{name, joystick, event}]."""
    try:
        text = Path(source).read_text(encoding="utf-8", errors="replace")
    except OSError:
        return []
    found = []
    for block in text.split("\n\n"):
        name, handlers = "", []
        for line in block.splitlines():
            if line.startswith("N: Name="):
                name = line.split("=", 1)[1].strip().strip('"')
            elif line.startswith("H: Handlers="):
                handlers = line.split("=", 1)[1].split()
        joystick = next((h for h in handlers if h.startswith("js") and h[2:].isdigit()), None)
        if joystick:
            event = next((h for h in handlers if h.startswith("event") and h[5:].isdigit()), None)
            found.append({"name": name or joystick, "joystick": "/dev/input/" + joystick,
                          "event": "/dev/input/" + event if event else None})
    return found


def status(mode, controllers):
    if controllers:
        extra = f" и ещё {len(controllers) - 1}" if len(controllers) > 1 else ""
        return f"Найден контроллер: {controllers[0]['name']}{extra}"
    if mode == "gamepad":
        return "Контроллер не найден: подключите геймпад, затем нажмите «Обновить»"
    return "Контроллер не найден"


def snapshot(mode, source=PROC_DEVICES):
    controllers = detect_controllers(source)
    return {"mode": mode, "modes": [{"value": m, "label": LABELS[m]} for m in MODES],
            "controllers": controllers, "status": status(mode, controllers)}
