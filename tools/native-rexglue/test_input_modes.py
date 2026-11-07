#!/usr/bin/env python3
"""Input mode: layouts, controller detection, settings validation, bridge round trip and real probe dry-run."""
import importlib.util
import contextlib
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
import acv_input as I
import acv_settings as S
import acv_mangohud as M

spec = importlib.util.spec_from_file_location("trg_backend_input", ROOT / "tools/acv-trg-launcher/backend.py")
bridge = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bridge)

DEVICES = """I: Bus=0003 Vendor=046d Product=c52b Version=0111
N: Name="Logitech USB Receiver"
H: Handlers=sysrq kbd event3 leds

I: Bus=0003 Vendor=045e Product=028e Version=0110
N: Name="Microsoft X-Box 360 pad"
H: Handlers=event7 js0
B: PROP=0

I: Bus=0003 Vendor=054c Product=09cc Version=8111
N: Name="Wireless Controller"
H: Handlers=js1 event9
"""


class InputModeTest(unittest.TestCase):
    def test_layouts(self):
        mk, _ = I.layout("keyboard_mouse")
        self.assertEqual((mk["mnk_mode"], mk["mnk_mouse"], mk["keybind_right_trigger"], mk["keybind_rstick_press"]),
                         ("true", "true", "LMB,O", "MMB,C"))
        kb, env = I.layout("keyboard")
        self.assertEqual((kb["mnk_mode"], kb["mnk_mouse"], kb["keybind_right_trigger"], kb["keybind_left_trigger"],
                          kb["keybind_rstick_press"]), ("true", "false", "O", "U", "C"))
        self.assertFalse([v for k, v in kb.items() if k.startswith("keybind_") and {"LMB", "RMB", "MMB"} & set(v.split(","))])
        self.assertEqual(env, {})
        gp, env = I.layout("gamepad")
        self.assertEqual(gp, {"mnk_mode": "false", "mnk_mouse": "false"})
        self.assertEqual(env, {"SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS": "1"})
        self.assertEqual(I.cvar_options("gamepad"), ["--mnk_mode=false", "--mnk_mouse=false"])
        self.assertEqual(I.normalize_mode("keyboard-mouse"), "keyboard_mouse")
        for bad in ("pad", "", None, 1, "Keyboard"):
            with self.assertRaises(ValueError):
                I.normalize_mode(bad)

    def test_environment_keeps_user_values(self):
        child = I.mode_environment("gamepad", {"SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS": "0", "X": "1"})
        self.assertEqual(child["SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS"], "0")
        self.assertEqual(I.mode_environment("keyboard", {"X": "1"}), {"X": "1"})

    def test_controller_detection_and_status(self):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / "devices"
            source.write_text(DEVICES)
            found = I.detect_controllers(source)
            self.assertEqual([c["joystick"] for c in found], ["/dev/input/js0", "/dev/input/js1"])
            self.assertEqual(found[0]["event"], "/dev/input/event7")
            self.assertIn("Microsoft X-Box 360 pad", I.status("gamepad", found))
            self.assertIn("ещё 1", I.status("keyboard", found))
            self.assertEqual(I.detect_controllers(Path(tmp) / "missing"), [])
        self.assertIn("подключите", I.status("gamepad", []))
        self.assertNotIn("подключите", I.status("keyboard_mouse", []))

    def test_settings_validation(self):
        self.assertEqual(S.launcher_input_mode(), "keyboard_mouse")
        self.assertEqual(S.launcher_input_mode({"input_mode": "gamepad"}), "gamepad")
        self.assertEqual(S.launcher_input_mode({"input_mode": "gamepad"}, "keyboard"), "keyboard")
        for bad in ({"input_mode": "pad"}, {"input_mode": 1}, {"input_mode": None}, {"input_mode": True}):
            with self.assertRaises(S.SettingsError):
                S.launcher_input_mode(bad)
        with self.assertRaises(S.SettingsError):
            S.launcher_input_mode({}, "mouse")

    def test_bridge_round_trip_and_rejection(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            snap = bridge.snapshot(root)
            self.assertEqual(snap["settings"]["launcher"]["input_mode"], "keyboard_mouse")
            self.assertEqual([m["value"] for m in snap["input"]["modes"]], ["keyboard_mouse", "keyboard", "gamepad"])
            doc = snap["settings"]
            doc["launcher"]["input_mode"] = "gamepad"
            self.assertEqual(bridge.snapshot(root, doc)["input"]["mode"], "gamepad")
            self.assertFalse((root / "settings.json").exists())
            bridge.save(root, doc)
            self.assertEqual(bridge.snapshot(root)["settings"]["launcher"]["input_mode"], "gamepad")
            before = (root / "settings.json").read_bytes()
            doc["launcher"]["input_mode"] = "touch"
            with self.assertRaises(S.SettingsError):
                bridge.save(root, doc)
            self.assertEqual((root / "settings.json").read_bytes(), before)

    def test_probe_dry_run_uses_saved_mode_and_cli_override(self):
        script = HERE / "probe_linux.py"
        config = HERE / "acv.config"
        original_config = config.read_bytes()
        with tempfile.TemporaryDirectory(dir=ROOT / "tmp", prefix="input-probe-") as tmp:
            root = Path(tmp)
            settings = root / "settings.json"
            settings.write_text(json.dumps({"schema_version": 1, "launcher": {"input_mode": "gamepad"}}))
            cases = [([], "gamepad"), (["--controls", "keyboard"], "keyboard"), (["--controls", "keyboard-mouse"], "keyboard_mouse")]
            for index, (extra, mode) in enumerate(cases):
                output = root / str(index)
                args = [str(script), "--config", str(config), "--settings", str(settings), *extra]
                with patch.object(sys, "argv", args), patch.dict(os.environ, {"ACV_PROBE_DRYRUN": str(output)}), \
                     patch("subprocess.Popen") as launched:
                    with self.assertRaises(SystemExit) as exit:
                        runpy.run_path(str(script), run_name="__main__")
                    self.assertEqual(exit.exception.code, 0)
                    launched.assert_not_called()
                data = json.loads(output.read_text())
                command = data["command"]
                wanted = I.cvar_options(mode)
                for option in wanted:
                    self.assertIn(option, command)
                self.assertEqual(sum(a.startswith("--mnk_mode=") for a in command), 1)
                self.assertEqual("--mnk_mode=false" in command, mode == "gamepad")
                self.assertEqual("--mnk_mouse=true" in command, mode == "keyboard_mouse")
                self.assertEqual(data["env"].get("SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS") == "1", mode == "gamepad")
            self.assertEqual(config.read_bytes(), original_config)
            bad = root / "bad.json"
            bad.write_text(json.dumps({"schema_version": 1, "launcher": {"input_mode": "touch"}}))
            with patch.object(sys, "argv", [str(script), "--config", str(config), "--settings", str(bad)]), \
                 patch.dict(os.environ, {"ACV_PROBE_DRYRUN": str(root / "x")}), patch("subprocess.Popen") as launched:
                stderr = io.StringIO()
                with self.assertRaises(SystemExit) as exit, contextlib.redirect_stderr(stderr):
                    runpy.run_path(str(script), run_name="__main__")
                self.assertEqual(exit.exception.code, 2)
                self.assertIn('launcher.input_mode must be', stderr.getvalue())
                launched.assert_not_called()


if __name__ == "__main__":
    unittest.main()
