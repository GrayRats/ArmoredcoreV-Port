"""Paths shared by development tools and a relocated local game package."""
import os
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
PACKAGED = (ROOT / "acv-package.json").is_file()
EXECUTABLE = ROOT / ("bin/game/acv_linux_probe" if PACKAGED else
                     "tmp/acv-rexglue-linux-indexed/out/build/linux-release/acv_linux_probe")
GAME = ROOT / ("data/game" if PACKAGED else "tmp/iso_root")
DATA = ROOT / ("data" if PACKAGED else "tmp")
PROFILES = Path(os.environ.get("ACV_PROFILES_ROOT", str(
    Path(os.environ.get("XDG_DATA_HOME", str(Path.home() / ".local/share"))) / "DEArmoredcore/acv"
    if PACKAGED else ROOT / "tmp/acv-user-data")))
LOGS = (Path(os.environ.get("XDG_STATE_HOME", str(Path.home() / ".local/state"))) /
        "DEArmoredcore/acv/logs" if PACKAGED else ROOT / "logs")
