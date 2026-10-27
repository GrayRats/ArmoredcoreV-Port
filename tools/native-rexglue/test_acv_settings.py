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
        for want in ("--anisotropic_override=5", "--window_width=2560", "--window_height=1080", "--fullscreen=true",
                     "--swap_post_effect=fxaa"):
            self.assertIn(want, a)

    def test_host_resolution_preserves_guest_mode_and_no_crop(self):
        for resolution in ("1920x1200", "2560x1080", "3440x1440"):
            args = s.graphics_to_cvars({"resolution": resolution})
            self.assertIn("--resolution=1280x720", args)
            self.assertIn("--present_letterbox=true", args)
            self.assertIn("--present_safe_area_x=100", args)
            self.assertIn("--present_safe_area_y=100", args)

    def test_vsync_selects_fifo(self):
        for enabled in (True, False):
            args = s.graphics_to_cvars({"vsync": enabled})
            for mode in ("immediate", "mailbox"):
                self.assertIn(f"--vulkan_allow_present_mode_{mode}={str(not enabled).lower()}", args)
            self.assertIn("--vulkan_allow_present_mode_fifo_relaxed=false", args)

    def test_ten_bit_requires_patched_runtime_and_no_8bit_dither(self):
        args = s.graphics_to_cvars({"color_depth": 10}, True, {"present_output_bits"})
        self.assertIn("--present_output_bits=10", args)
        with self.assertRaises(s.SettingsError):
            s.graphics_to_cvars({"color_depth": 10, "dither": True}, True, {"present_output_bits"})

    def test_fsr_sharpness_reaches_rcas(self):
        args = s.graphics_to_cvars({"output_effect": "fsr", "fsr_sharpness": 1.5}, True, {"fidelityfx"})
        self.assertIn("--present_fsr_sharpness_reduction=1.5", args)

    def test_limiter_only_with_runtime_support(self):
        self.assertFalse(any(a.startswith("--frame_limit_fps") for a in s.graphics_to_cvars({})))
        self.assertIn("--frame_limit_fps=60", s.graphics_to_cvars({"vsync": False}, features={"frame_limit_fps"}))

    def test_known_harmless_error_is_not_fatal(self):
        harmless = "[2026-10-24 17:09:21.196] [error] [fs] [t17429] ResolvePath(system:\\) failed - device not found"
        self.assertFalse(s.is_fatal_runtime_error(harmless))
        self.assertTrue(s.is_fatal_runtime_error("[error] [gpu] vkQueueSubmit failed: VK_ERROR_DEVICE_LOST"))

    def test_dedupe_keeps_last_value(self):
        self.assertEqual(s.dedupe_options(["--a=1", "--user_language=1", "--b", "--user_language=2", "x"]),
                         ["--a=1", "--b", "--user_language=2", "x"])

    def test_runtime_features_detected_from_libraries(self):
        import tempfile
        from pathlib import Path
        with tempfile.TemporaryDirectory() as d:
            self.assertEqual(s.runtime_features(d), set())
            (Path(d) / "librexruntime.so").write_bytes(b"\0cas_additional_sharpness\0ACV_XUID\0")
            (Path(d) / "librexgpu-xenos.so").write_bytes(b"frame_limit_fps")
            self.assertEqual(s.runtime_features(d), {"ACV_XUID", "cas_additional_sharpness", "fidelityfx", "frame_limit_fps"})

    def test_output_effect_needs_fidelityfx_runtime(self):
        with self.assertRaises(s.SettingsError):
            s.graphics_to_cvars({"output_effect": "cas"}, allow_experimental=True)
        a = s.graphics_to_cvars({"output_effect": "fsr"}, True, {"fidelityfx"})
        self.assertIn("--present_effect=fsr", a)
        with self.assertRaises(s.SettingsError):
            s.graphics_to_cvars({"output_effect": "fsr2"}, True, {"fidelityfx"})
        a = s.graphics_to_cvars({"output_effect": "cas", "sharpness": 0.5}, True, {"fidelityfx"})
        self.assertIn("--present_cas_additional_sharpness=0.5", a)
        self.assertIn("--present_effect=bilinear", s.graphics_to_cvars({}, features={"fidelityfx"}))

    def test_unavailable_and_experimental_rejected(self):
        for bad in ({"fps_limit": 144}, {"color_encoding": "ycbcr420"}, {"color_depth": 10}):
            with self.assertRaises(s.SettingsError):
                s.graphics_to_cvars(bad, allow_experimental=True)
        with self.assertRaises(s.SettingsError):
            s.graphics_to_cvars({"render_scale": 2})

    def test_wide_view_maps_resolution_to_hooks_and_letterbox(self):
        wide = {"wide_view": "auto", "resolution": "2560x1080"}
        env = s.graphics_to_env(wide, {"wide_view"})
        self.assertAlmostEqual(float(env["ACV_EXPERIMENTAL_SCENE_ASPECT"]), 64 / 27)
        self.assertEqual(env["ACV_EXPERIMENTAL_SCENE_ASPECT"], env["ACV_EXPERIMENTAL_HUD_ASPECT"])
        args = s.dedupe_options(s.graphics_to_cvars(wide, True, {"wide_view"}))
        self.assertIn("--present_letterbox=false", args)
        self.assertNotIn("--present_letterbox=true", args)
        # narrower than 16:9 (16:10) is handled by the same hooks
        self.assertAlmostEqual(float(s.graphics_to_env({"wide_view": "auto", "resolution": "1920x1200"},
                                                       {"wide_view"})["ACV_EXPERIMENTAL_SCENE_ASPECT"]), 1.6)
        with self.assertRaises(s.SettingsError):  # 1:1 is outside the supported range
            s.graphics_to_env({"wide_view": "auto", "resolution": "1000x1000"}, {"wide_view"})
        # 16:9 output, off, and the "auto" resolution never enable the hooks
        self.assertEqual(s.graphics_to_env({"wide_view": "auto", "resolution": "1920x1080"}, {"wide_view"}), {})
        self.assertEqual(s.graphics_to_env({}, {"wide_view"}), {})
        for bad, why in (({"wide_view": "auto"}, "auto resolution"),
                         ({"wide_view": "auto", "resolution": "2560x1080"}, "no host hooks")):
            with self.assertRaises(s.SettingsError, msg=why):
                s.graphics_to_cvars(bad, True, frozenset())
        with self.assertRaises(s.SettingsError):  # experimental needs the flag
            s.graphics_to_cvars(wide, False, {"wide_view"})

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
        if not cfg.exists():  # acv.config is git-ignored local user config; absent in a clean checkout
            self.skipTest("local acv.config not present")
        before = hashlib.sha256(cfg.read_bytes()).hexdigest()
        p = s.migrate_legacy_config(cfg)
        self.assertEqual((p.gamertag, p.pilot_name), ("ARTEM", "ARTEM"))
        self.assertEqual(hashlib.sha256(cfg.read_bytes()).hexdigest(), before)
        self.assertEqual(s.runtime_env(p), {"ACV_PILOT_NAME": "ARTEM", "ACV_GAMERTAG": "ARTEM"})


if __name__ == "__main__":
    unittest.main()
