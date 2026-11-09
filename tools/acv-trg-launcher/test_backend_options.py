import sys, tempfile, unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import backend
import acv_settings as S

DISPLAY = {"monitors": [{"index": 1, "width": 2560, "height": 1080}, {"index": 2, "width": 1920, "height": 1080}]}


class ExplainedOptions(unittest.TestCase):
    def test_monitor_resolution_is_derived_for_the_selected_monitor(self):
        self.assertEqual(backend.derived_resolution({"monitor": 2}, DISPLAY), "1920x1080")
        self.assertEqual(backend.derived_resolution({"monitor": 0}, DISPLAY), "2560x1080")  # unknown -> first
        self.assertIsNone(backend.derived_resolution({}, {}))
        self.assertIsNone(backend.derived_resolution({}, {"monitors": [{"width": 0, "height": 0}]}))

    def test_wide_view_gets_the_monitor_resolution_only_when_needed(self):
        warnings = []
        graphics = {"wide_view": "auto", "resolution": "auto", "monitor": 0}
        backend.fill_wide_resolution(graphics, DISPLAY, warnings)
        self.assertEqual(graphics["resolution"], "2560x1080")
        self.assertEqual(len(warnings), 1)
        S.graphics_to_cvars(dict(graphics, wide_view="auto"), True, {"wide_view"})  # now valid
        for untouched in ({"wide_view": "off", "resolution": "auto"}, {"wide_view": "auto", "resolution": "1920x1080"}):
            before, warnings = dict(untouched), []
            backend.fill_wide_resolution(untouched, DISPLAY, warnings)
            self.assertEqual((untouched, warnings), (before, []))
        graphics, warnings = {"wide_view": "auto", "resolution": "auto"}, []
        backend.fill_wide_resolution(graphics, {}, warnings)  # no observed monitor: the explicit-resolution error stays
        self.assertEqual((graphics["resolution"], warnings), ("auto", []))

    def test_reasons_say_what_to_do(self):
        experimental = backend.explain(S.SettingsError("wide_view='auto' is experimental: x"), "static note")
        self.assertIn("«Экспериментальные настройки»", experimental)
        self.assertIn("static note", experimental)
        self.assertIn("разрешение", backend.explain(S.SettingsError("wide_view=auto needs an explicit resolution"), None))
        self.assertIn("FidelityFX", backend.explain(S.SettingsError("output_effect='fsr' needs a FidelityFX-enabled runtime"), None))
        self.assertEqual(backend.explain(S.SettingsError("x is unavailable: y"), "static"), "static")
        self.assertEqual(backend.explain(S.SettingsError("something else"), None), "something else")

    def test_snapshot_explains_experimental_options(self):
        with tempfile.TemporaryDirectory() as temp:
            snapshot = backend.snapshot(Path(temp))
        options = {(key, option["value"]): option for key, cap in snapshot["capabilities"].items() for option in cap["options"] or []}
        for key in (("wide_view", "auto"), ("color_depth", 10), ("antialiasing", "fxaa"), ("render_scale", 2)):
            self.assertFalse(options[key]["selectable"], key)
            self.assertIn("«Экспериментальные настройки»", options[key]["reason"], key)


if __name__ == "__main__":
    unittest.main()
