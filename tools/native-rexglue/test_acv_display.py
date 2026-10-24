import sys, unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from acv_display import summarize

class DisplayTests(unittest.TestCase):
    def test_unknown_is_not_success(self):
        self.assertIsNone(summarize("ordinary startup")['applied'])
        report = summarize("[acv-display] applied width=1 height=1 format=999 mode=999 requested_bits=10")
        self.assertIsNone(report['applied']['bits'])
        self.assertEqual(report['applied']['present_mode'], 'unknown')

    def test_actual_depth_and_effect_with_fallback(self):
        report = summarize("""[acv-display] surface_format format=44 colorspace=0
[acv-display] surface_format format=44 colorspace=0
[acv-display] applied width=1600 height=675 format=44 colorspace=0 mode=2 requested_bits=10
[acv-display] output source_width=1280 source_height=720 x=200 y=0 width=1200 height=675 requested_effect=2 final_effect=7 passes=2 format=44
[acv-display] pacing samples=300 limit=60 fps=59.99 mean_ms=16.67 p95_ms=17.0
""")
        self.assertEqual(len(report['surface_formats']), 1)
        self.assertEqual(report['applied']['bits'], 8)
        self.assertTrue(report['applied']['depth_fallback'])
        self.assertEqual(report['output']['final_effect_name'], 'fsr_rcas')
        self.assertEqual(report['pacing'][0]['samples'], 300)
        self.assertEqual(report['output']['x'], 200)

if __name__ == '__main__': unittest.main()
