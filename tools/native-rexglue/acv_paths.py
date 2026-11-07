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
