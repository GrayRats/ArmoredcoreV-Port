"""Paths shared by development tools and a relocated local game package."""
import os
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
PACKAGED = (ROOT / "acv-package.json").is_file()
if os.name == "nt":
    EXECUTABLE = ROOT / ("bin/game/acv_windows_probe.exe" if PACKAGED else
                         "tmp/acv-rexglue-win-indexed/out/build/win-amd64/acv_windows_probe.exe")
else:
    EXECUTABLE = ROOT / ("bin/game/acv_linux_probe" if PACKAGED else
                         "tmp/acv-rexglue-linux-indexed/out/build/linux-release/acv_linux_probe")
GAME = ROOT / ("data/game" if PACKAGED else "tmp/iso_root")
DATA = ROOT / ("data" if PACKAGED else "tmp")
if os.name == "nt":
    local_app_data = Path(os.environ.get("LOCALAPPDATA", str(Path.home() / "AppData/Local")))
    PROFILES = Path(os.environ.get("ACV_PROFILES_ROOT", str(
        local_app_data / "DEArmoredcore/acv" if PACKAGED else ROOT / "tmp/acv-user-data")))
    LOGS = local_app_data / "DEArmoredcore/acv/logs" if PACKAGED else ROOT / "logs"
else:
    PROFILES = Path(os.environ.get("ACV_PROFILES_ROOT", str(
        Path(os.environ.get("XDG_DATA_HOME", str(Path.home() / ".local/share"))) / "DEArmoredcore/acv"
        if PACKAGED else ROOT / "tmp/acv-user-data")))
    LOGS = (Path(os.environ.get("XDG_STATE_HOME", str(Path.home() / ".local/state"))) /
            "DEArmoredcore/acv/logs" if PACKAGED else ROOT / "logs")


def _same_file(a, b):
    try:
        return os.path.normcase(str(Path(a).resolve())) == os.path.normcase(str(Path(b).resolve()))
    except OSError:
        return False


def _inside(path, folder):
    try:
        Path(os.path.normcase(str(Path(path).resolve()))).relative_to(os.path.normcase(str(Path(folder).resolve())))
        return True
    except (ValueError, OSError):
        return False


def resolve_build(saved_build, root=None, executable=None, packaged=None):
    """Executable to use for the saved launcher.build: (path, status, warning).

    status: "dev" (not a package; the saved path is used as is), "package" (this package's own build),
    "relocated" (the saved path named this package's executable in another folder, e.g. a moved or older
    release: the build of this package is used), "external" (a different build the user chose) and "missing"
    (the saved file does not exist: this package's build is used). The warning is None unless the saved
    value was not honoured or points outside the package. Saved preferences are never rewritten here."""
    root = ROOT if root is None else Path(root)
    executable = EXECUTABLE if executable is None else Path(executable)
    packaged = PACKAGED if packaged is None else packaged
    if not saved_build:
        return str(executable), ("package" if packaged else "dev"), None
    saved = Path(saved_build)
    if not packaged:
        return str(saved), "dev", None
    if _same_file(saved, executable):
        return str(executable), "package", None
    if packaged_build_selection(saved_build) != str(saved):  # Linux sibling-release switch
        return str(executable), "package", None
    if _inside(saved, root):
        return str(saved), "package", None
    if saved.name.lower() == executable.name.lower():
        return str(executable), "relocated", (
            f"Сохранённый путь сборки указывал в другую папку ({saved.parent}); используется сборка этого пакета")
    if not saved.is_file():
        return str(executable), "missing", f"Сохранённая сборка не найдена ({saved}); используется сборка этого пакета"
    return str(saved), "external", f"Используется внешняя сборка игры: {saved}"


def packaged_build_selection(saved_build):
    """Follow a sibling release switch without rewriting saved preferences."""
    saved = Path(saved_build)
    if (PACKAGED and ROOT.name.startswith("ACV-Linux-release-") and
            saved.name == "acv_linux_probe" and saved.parent.name == "game" and
            saved.parent.parent.name == "bin" and
            saved.parents[2].parent == ROOT.parent and
            saved.parents[2].name.startswith("ACV-Linux-release-")):
        return str(EXECUTABLE)
    return str(saved)
