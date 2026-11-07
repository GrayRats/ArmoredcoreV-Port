"""acv_launch_spec.build mirrors probe_linux.py for the Windows launcher (no game is started)."""
import sys, tempfile, unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import acv_launch_spec as L


class LaunchSpecTest(unittest.TestCase):
    def setUp(self):
        self.root = tempfile.TemporaryDirectory()
        self.addCleanup(self.root.cleanup)
        self.spec = L.build(Path(self.root.name), run_id="test")

    def test_input_cvars_enable_keyboard_and_mouse(self):
        self.assertIn("--mnk_mode=true", self.spec["args"])  # SDK default is false

    def test_readback_defaults_to_full_and_launcher_choice_wins(self):
        modes = [a for a in self.spec["args"] if a.startswith("--readback_resolve=")]
        self.assertEqual(modes, ["--readback_resolve=full"])
        self.assertIn("--protect_zero=false", self.spec["args"])
        (Path(self.root.name) / "settings.json").write_text(
            '{"schema_version": 1, "graphics": {"readback_resolve": "fast"}, "launcher": {}}')
        fast = L.build(Path(self.root.name), run_id="fast")
        self.assertEqual([a for a in fast["args"] if a.startswith("--readback_resolve=")], ["--readback_resolve=fast"])

    def test_no_repeated_option_keys(self):
        keys = [a.split("=", 1)[0] for a in self.spec["args"] if a.startswith("--")]
        self.assertEqual(len(keys), len(set(keys)))

    def test_names_mounts_and_log(self):
        env = self.spec["env"]
        self.assertEqual(env["ACV_PILOT_NAME"], "ARTEM")
        self.assertEqual(env["ACV_GAMERTAG"], "ARTEM")
        self.assertTrue(Path(env["ACV_SYSTEM_ROOT"]).is_dir())
        self.assertIn("--log_file=" + self.spec["log_file"], self.spec["args"])
        self.assertIn("--gpu_plugin=xenos", self.spec["args"])

    def test_reserved_sdk_key_is_rejected(self):
        bad = Path(self.root.name) / "bad.config"
        bad.write_text("[sdk]\nlog_file = x\n")
        with self.assertRaises(Exception):
            L.read_config(bad)


if __name__ == "__main__":
    unittest.main()
