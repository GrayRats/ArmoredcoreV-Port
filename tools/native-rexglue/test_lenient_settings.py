import sys, unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent / "acv-trg-launcher"))
import acv_settings as S


def cvars(values, features=frozenset(), allow=True):
    warnings = []
    return S.graphics_to_cvars(values, allow, features, warnings), warnings


class LenientSettings(unittest.TestCase):
    def test_strict_mode_is_unchanged(self):
        for values in ({"gpu_api": "d3d12"}, {"color_depth": 10}, {"fps_limit": 30},
                       {"output_effect": "fsr"}):
            with self.assertRaises(S.SettingsError):
                S.graphics_to_cvars(values, True, frozenset())

    def test_unsupported_directx12_falls_back_to_vulkan(self):
        args, warnings = cvars({"gpu_api": "d3d12"})
        self.assertEqual(len(warnings), 1)
        self.assertFalse(any(a.startswith(("--gpu_api=", "--render_target_path_d3d12=")) for a in args))

    def test_unsupported_ten_bit_falls_back_to_eight(self):
        args, warnings = cvars({"color_depth": 10, "dither": False})
        self.assertEqual(len(warnings), 1)
        self.assertFalse(any(a.startswith("--present_output_bits=") for a in args))

    def test_unsupported_scaling_and_frame_limit_degrade(self):
        args, warnings = cvars({"output_effect": "fsr", "fps_limit": 30})
        self.assertEqual(len(warnings), 2)
        self.assertIn("--present_effect=bilinear", args)
        self.assertFalse(any(a.startswith("--frame_limit_fps=") for a in args))

    def test_unsupported_wide_view_is_ignored(self):
        args, warnings = cvars({"wide_view": "auto", "resolution": "2560x1080"})
        self.assertEqual(len(warnings), 1)
        self.assertIn("--present_letterbox=true", args)
        env_warnings = []
        self.assertEqual(S.graphics_to_env({"wide_view": "auto", "resolution": "2560x1080"}, frozenset(), env_warnings), {})
        self.assertEqual(len(env_warnings), 1)

    def test_supported_choices_produce_no_warnings(self):
        features = {"d3d12", "present_output_bits", "fidelityfx", "frame_limit_fps"}
        args, warnings = cvars({"gpu_api": "d3d12", "color_depth": 10, "dither": False,
                                "output_effect": "fsr", "fps_limit": 30}, features)
        self.assertEqual(warnings, [])
        self.assertIn("--gpu_api=d3d12", args)
        self.assertIn("--present_output_bits=10", args)
        self.assertIn("--present_effect=fsr", args)
        self.assertIn("--frame_limit_fps=30", args)

    def test_real_conflicts_still_raise(self):
        with self.assertRaises(S.SettingsError):
            S.graphics_to_cvars({"color_depth": 10, "dither": True}, True, {"present_output_bits"}, [])
        with self.assertRaises(S.SettingsError):
            S.graphics_to_cvars({"output_effect": "fsr"}, False, {"fidelityfx"}, [])  # experimental needs opt-in
        with self.assertRaises(S.SettingsError):
            S.graphics_to_cvars({"wide_view": "auto"}, True, {"wide_view"}, [])  # needs an explicit resolution

    def test_quality_preset_is_applied_without_fidelityfx_when_lenient(self):
        memory = S.host_memory_info()
        with self.assertRaises(S.SettingsError):
            S.apply_graphics_preset({}, "quality", set(), memory)
        result = S.apply_graphics_preset({}, "quality", set(), memory, lenient=True)
        self.assertEqual(result["graphics"]["output_effect"], "bilinear")
        self.assertFalse(result["launcher"].get("allow_experimental", False))


class BackendTolerance(unittest.TestCase):
    def test_rejected_saved_values_are_dropped_with_warnings(self):
        import backend
        warnings = []
        graphics = backend.tolerant_graphics({"vsync": False, "no_such_option": 1, "fps_limit": 17}, warnings)
        self.assertFalse(graphics["vsync"])
        self.assertEqual(graphics["fps_limit"], S.resolve_graphics()["fps_limit"])
        self.assertEqual(len(warnings), 2)


if __name__ == "__main__":
    unittest.main()
