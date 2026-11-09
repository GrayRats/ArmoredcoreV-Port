import sys, tempfile, unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import pacing_report as R

LOG = """[2026-11-10 02:00:00.000] [info] [core] [t1] start
[2026-11-10 02:00:10.000] [info] [core] [t2] [acv-display] pacing samples=300 limit=60 fps=30.0 mean_ms=33.3 p95_ms=35.0 p99_ms=36.0 min_ms=16.0 max_ms=900.0
[2026-11-10 02:02:10.000] [info] [core] [t2] [acv-display] pacing samples=300 limit=60 fps=60.0 mean_ms=16.7 p95_ms=17.0 p99_ms=18.0 min_ms=15.0 max_ms=30.0
[2026-11-10 02:02:20.000] [info] [core] [t2] [acv-display] pacing samples=300 limit=60 fps=40.0 mean_ms=25.0 p95_ms=40.0 p99_ms=80.0 min_ms=15.0 max_ms=600.0
"""


class PacingReport(unittest.TestCase):
    def test_loading_phase_is_skipped_and_stalls_are_counted(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "runtime.log"
            path.write_text(LOG, encoding="utf-8")
            everything = R.summarize(path, 0)
            gameplay = R.summarize(path, 120)
        self.assertEqual(everything["windows"], 3)
        self.assertEqual(everything["stalls_500ms"], 2)
        self.assertEqual(gameplay["windows"], 2)
        self.assertEqual(gameplay["seconds"], 10)
        self.assertAlmostEqual(gameplay["fps_mean"], 50.0)
        self.assertEqual(gameplay["p99_worst"], 80.0)
        self.assertEqual((gameplay["max_worst"], gameplay["stalls_100ms"], gameplay["stalls_500ms"]), (600.0, 1, 1))

    def test_log_without_pacing_gives_nothing(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "empty.log"
            path.write_text("[2026-11-10 02:00:00.000] [info] x\n", encoding="utf-8")
            self.assertIsNone(R.summarize(path, 0))


if __name__ == "__main__":
    unittest.main()
