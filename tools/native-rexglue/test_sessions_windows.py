import sys, tempfile, unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import acv_sessions

LOG = """[2026-11-10 00:50:59.860] [info] [sys] [t1] GPU plugin 'xenos' loaded (rexgpu-xenos.dll)
[2026-11-10 00:51:00.202] [info] [core] [t1] [acv-display] monitor index=1 width=2560 height=1080 refresh_hz=165 scale=1
[2026-11-10 00:51:00.324] [info] [core] [t1] [acv-display] display_mode monitor=1 width=2560 height=1080 refresh_hz=165
[2026-11-10 00:51:00.324] [info] [core] [t1] [acv-display] display_mode monitor=1 width=1920 height=1080 refresh_hz=60
[2026-11-10 00:52:00.000] [info] [core] [t2] [acv-display] pacing samples=300 limit=60 fps=30.0 mean_ms=33.3 p95_ms=35.0 p99_ms=36.0 min_ms=16.0 max_ms=40.0
"""


class WindowsRuntimeLogs(unittest.TestCase):
    def test_runtime_log_gives_monitors_modes_and_pacing(self):
        with tempfile.TemporaryDirectory() as temp:
            logs = Path(temp) / "launcher-logs"
            logs.mkdir()
            runtime = Path(temp) / "logs"
            runtime.mkdir()
            (runtime / "runtime-20261109T195059801914Z.log").write_text("old\n", encoding="utf-8")
            newest = runtime / "runtime-20261109T200212599914Z.log"
            newest.write_text(LOG, encoding="utf-8")
            report = acv_sessions.latest(logs, runtime)
        self.assertTrue(report["available"])
        self.assertTrue(report["evidence"].endswith(newest.name))
        self.assertEqual(report["monitors"][0]["width"], 2560)
        self.assertEqual({(m["width"], m["height"]) for m in report["display_modes"]}, {(2560, 1080), (1920, 1080)})
        self.assertEqual(report["pacing_last"]["fps"], 30.0)

    def test_without_logs_the_launcher_still_reports_no_runs(self):
        with tempfile.TemporaryDirectory() as temp:
            self.assertFalse(acv_sessions.latest(Path(temp), Path(temp) / "missing")["available"])
            self.assertFalse(acv_sessions.latest(Path(temp))["available"])


if __name__ == "__main__":
    unittest.main()
