#!/usr/bin/env python3
"""Exercise shared setting, real bridge and probe environment without launching ACV."""
import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import runpy
import sys
import tempfile
import unittest
from unittest.mock import patch

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
import acv_mangohud as M
import acv_settings as S

spec = importlib.util.spec_from_file_location("trg_backend_mangohud", ROOT / "tools/acv-trg-launcher/backend.py")
bridge = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bridge)
FOUND = {"available": True, "command": "/fixture/mangohud", "reason": "found"}
MISSING = {"available": False, "command": None, "reason": "MangoHud не найден"}


class MangoHudTest(unittest.TestCase):
    def test_guest_cadence_window_stale_and_safe_exec(self):
        import subprocess
        with tempfile.TemporaryDirectory(prefix='acv HUD $quote,') as tmp:
            source={'MANGOHUD_CONFIG':'font_size=20', 'MANGOHUD_CONFIGFILE':'/user/custom.conf'}
            child, stats=M.guest_overlay(source,Path(tmp))
            self.assertIn('waiting',stats.path.read_text())
            self.assertNotIn(str(stats.path),child['MANGOHUD_CONFIG'])
            self.assertEqual(child['MANGOHUD_CONFIGFILE'],source['MANGOHUD_CONFIGFILE'])
            self.assertTrue(child['MANGOHUD_CONFIG'].startswith('font_size=20,'))
            stats.consume('[acv-display] pacing samples=300 fps=25 mean_ms=40',now=100)
            self.assertIn('Guest 25.0/s 40.0ms',stats.path.read_text())
            self.assertEqual(stats.sample['window_seconds'],12)
            stats.consume('[acv-display] pacing samples=300 fps=nan mean_ms=40',now=101)
            self.assertEqual(stats.received,100)
            stats.refresh(now=120)
            self.assertIn('no fresh',stats.path.read_text())
            text=subprocess.check_output('cat -- "$ACV_MANGOHUD_STATS_FILE"',shell=True,env={**os.environ,**child},text=True)
            self.assertEqual(text,stats.path.read_text())
            stats.refresh(stopped=True)
            self.assertIn('stopped',stats.path.read_text())

    def test_shared_precedence_and_strict_types(self):
        self.assertFalse(S.launcher_mangohud())
        self.assertTrue(S.launcher_mangohud({"mangohud": True}))
        self.assertFalse(S.launcher_mangohud({"mangohud": True}, False))
        self.assertTrue(S.launcher_mangohud({"mangohud": False}, True))
        for invalid in [1, "true", None, [], {}]:
            with self.assertRaises(S.SettingsError):
                S.launcher_mangohud({"mangohud": invalid})

    def test_child_env_preserves_config_and_preloads(self):
        original = {"MANGOHUD": "0", "DISABLE_MANGOHUD": "1", "LD_PRELOAD": "/capture.so",
                    "VK_INSTANCE_LAYERS": "other_layer", "MANGOHUD_CONFIG": "fps,frame_timing",
                    "MANGOHUD_CONFIGFILE": "/user/MangoHud.conf"}
        before = dict(original)
        with patch.object(M, "availability", return_value=FOUND):
            child, info = M.game_environment(True, original)
        self.assertTrue(info["requested"])
        self.assertEqual(child["MANGOHUD"], "1")
        self.assertNotIn("DISABLE_MANGOHUD", child)
        for key in ("LD_PRELOAD", "VK_INSTANCE_LAYERS", "MANGOHUD_CONFIG", "MANGOHUD_CONFIGFILE"):
            self.assertEqual(child[key], original[key])
        self.assertEqual(original, before)
        self.assertEqual(info["stripped_inherited"], [])
        with patch.object(M, "availability", return_value=FOUND):
            child, info = M.game_environment(True, {"ENABLE_VULKAN_RENDERDOC_CAPTURE": "1"})
            self.assertEqual(child["ENABLE_VULKAN_RENDERDOC_CAPTURE"], "1")
            self.assertEqual(info["stripped_inherited"], [])
            child, _ = M.game_environment(False, {"ENABLE_VULKAN_RENDERDOC_CAPTURE": "1"})
            self.assertEqual(child["ENABLE_VULKAN_RENDERDOC_CAPTURE"], "1")
        with patch.object(M, "availability", return_value=MISSING):
            with self.assertRaisesRegex(ValueError, "MangoHud"):
                M.game_environment(True, original)
            child, _ = M.game_environment(False, {"MANGOHUD": "1"})
        self.assertEqual(child, {"MANGOHUD": "0", "DISABLE_MANGOHUD": "1"})

    def test_availability_uses_host_command(self):
        with patch.object(M.sys, "platform", "linux"), patch.object(M.shutil, "which", return_value=None):
            self.assertFalse(M.availability()["available"])
        with patch.object(M.sys, "platform", "linux"), patch.object(M.shutil, "which", return_value="/fixture/mangohud"):
            self.assertTrue(M.availability()["available"])

    def test_bridge_round_trip_preview_and_missing_install(self):
        with tempfile.TemporaryDirectory() as tmp, patch.object(M, "availability", return_value=FOUND):
            root = Path(tmp)
            doc = bridge.snapshot(root)["settings"]
            self.assertFalse(doc["launcher"]["mangohud"])
            doc["launcher"]["mangohud"] = True
            self.assertTrue(bridge.snapshot(root, doc)["settings"]["launcher"]["mangohud"])
            self.assertFalse((root / "settings.json").exists())
            bridge.save(root, doc)
            before = (root / "settings.json").read_bytes()
            self.assertTrue(bridge.snapshot(root)["settings"]["launcher"]["mangohud"])
            with patch.object(M, "availability", return_value=MISSING):
                self.assertFalse(bridge.snapshot(root)["mangohud"]["available"])
                with self.assertRaisesRegex(ValueError, "MangoHud"):
                    bridge.save(root, doc)
                self.assertEqual((root / "settings.json").read_bytes(), before)
                doc["launcher"]["mangohud"] = False
                bridge.save(root, doc)
            self.assertFalse(bridge.snapshot(root)["settings"]["launcher"]["mangohud"])

    def test_real_probe_saved_cli_override_and_missing_install(self):
        script = HERE / "probe_linux.py"
        config = HERE / "acv.config"
        original_config = config.read_bytes()
        with tempfile.TemporaryDirectory(dir=ROOT / "tmp", prefix="mangohud-probe-") as tmp:
            root = Path(tmp)
            settings = root / "settings.json"
            settings.write_text(json.dumps({"schema_version": 1, "launcher": {"mangohud": True}}))
            for index, (extra, found, code, requested) in enumerate([
                ([], FOUND, 0, True), (["--no-mangohud"], MISSING, 0, False),
                ([], MISSING, 2, None), (["--mangohud"], FOUND, 0, True)]):
                output = root / str(index)
                args = [str(script), "--config", str(config), "--settings", str(settings), *extra]
                with patch.object(sys, "argv", args), patch.object(M, "availability", return_value=found), \
                     patch.dict(os.environ, {"ACV_PROBE_DRYRUN": str(output), "MANGOHUD": "1"}), \
                     patch("subprocess.Popen") as launched, contextlib.redirect_stderr(io.StringIO()):
                    with self.assertRaises(SystemExit) as exit:
                        runpy.run_path(str(script), run_name="__main__")
                    self.assertEqual(exit.exception.code, code)
                    launched.assert_not_called()
                if requested is None:
                    self.assertFalse(output.exists())
                else:
                    data = json.loads(output.read_text())
                    self.assertEqual(data["env"]["MANGOHUD"], "1" if requested else "0")
                    self.assertTrue(data["command"][0].endswith("acv_linux_probe"))
            self.assertEqual(config.read_bytes(), original_config)


if __name__ == "__main__":
    unittest.main()
