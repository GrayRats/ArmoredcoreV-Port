#!/usr/bin/env python3
"""Tests for the stage A settings/profile contract."""
import hashlib, sys, tempfile, unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import acv_settings as s


class GraphicsTests(unittest.TestCase):
    def test_precedence(self):
        r = s.resolve_graphics({"fps_limit": 120, "vsync": False}, {"fps_limit": 144}, {"vsync": True})
        self.assertEqual((r["fps_limit"], r["vsync"], r["render_scale"]), (144, True, 1))

    def test_rejects_unsupported(self):
        for bad in ({"fps_limit": 90}, {"vsync": 1}, {"render_scale": True}, {"resolution": "big"},
                    {"nope": 1}, {"window_mode": "x"}):
            with self.assertRaises(s.SettingsError, msg=bad):
                s.resolve_graphics(bad)

    def test_accepts_values(self):
        s.validate_graphics({"resolution": "2560x1080", "anisotropy": 16, "fps_limit": 165})


class CvarTests(unittest.TestCase):
    def test_defaults_and_mapping(self):
        a = s.graphics_to_cvars({})
        self.assertIn("--anisotropic_override=-1", a)
        self.assertIn("--fullscreen=false", a)
        self.assertNotIn("--resolution", " ".join(x.split("=")[0] for x in a if x == "--resolution"))
        a = s.graphics_to_cvars({"anisotropy": 16, "resolution": "2560x1080", "window_mode": "fullscreen",
                                 "antialiasing": "fxaa"}, allow_experimental=True)
        for want in ("--anisotropic_override=5", "--resolution=2560x1080", "--fullscreen=true",
                     "--swap_post_effect=fxaa"):
            self.assertIn(want, a)

    def test_unavailable_and_experimental_rejected(self):
        for bad in ({"fps_limit": 144}, {"color_encoding": "ycbcr420"}, {"color_depth": 10}):
            with self.assertRaises(s.SettingsError):
                s.graphics_to_cvars(bad, allow_experimental=True)
        with self.assertRaises(s.SettingsError):
            s.graphics_to_cvars({"render_scale": 2})

    def test_capabilities_cover_schema(self):
        caps = s.capabilities()
        self.assertEqual(set(caps), set(s.GRAPHICS_SCHEMA))
        statuses = {o["status"] for o in caps["fps_limit"]["options"]}
        self.assertEqual(statuses, {"available", "unavailable"})


class ProfileTests(unittest.TestCase):
    def test_identity_separate_and_rename(self):
        p = s.new_profile("Artem", "ARTEM", "PILOT")
        r = s.rename_profile(p, "Other")
        self.assertEqual((r.uuid, r.xuid, r.gamertag, r.pilot_name), (p.uuid, p.xuid, p.gamertag, p.pilot_name))
        self.assertEqual(r.display_name, "Other")
        self.assertNotEqual(p.uuid, p.xuid)

    def test_validation(self):
        p = s.new_profile("A")
        for field, value in (("gamertag", "x" * 16), ("xuid", "abc"), ("uuid", "nope"), ("pilot_name", " ")):
            q = s.Profile(**{**p.__dict__, field: value})
            with self.assertRaises(s.SettingsError, msg=field):
                q.validate()

    def test_roundtrip_and_version(self):
        with tempfile.TemporaryDirectory() as d:
            p = s.new_profile("Artem")
            s.save_profile(d, p)
            self.assertEqual(s.load_profile(d, p.uuid), p)
            path = Path(d) / p.data_dir / "profile.json"
            path.write_text(path.read_text().replace('"schema_version": 1', '"schema_version": 99'))
            with self.assertRaises(s.SettingsError):
                s.load_profile(d, p.uuid)

    def test_atomic_save_keeps_old_file_on_failure(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "x.json"
            s.save_json_atomic(path, {"a": 1})
            with self.assertRaises(TypeError):
                s.save_json_atomic(path, {"a": object()})
            self.assertIn('"a": 1', path.read_text())
            self.assertEqual([f.name for f in Path(d).iterdir()], ["x.json"])


class MigrationTests(unittest.TestCase):
    def test_legacy_config_untouched(self):
        cfg = Path(__file__).resolve().parent / "acv.config"
        before = hashlib.sha256(cfg.read_bytes()).hexdigest()
        p = s.migrate_legacy_config(cfg)
        self.assertEqual((p.gamertag, p.pilot_name), ("ARTEM", "ARTEM"))
        self.assertEqual(hashlib.sha256(cfg.read_bytes()).hexdigest(), before)
        self.assertEqual(s.runtime_env(p), {"ACV_PILOT_NAME": "ARTEM", "ACV_GAMERTAG": "ARTEM"})


if __name__ == "__main__":
    unittest.main()
