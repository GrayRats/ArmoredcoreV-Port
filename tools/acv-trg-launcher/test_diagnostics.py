import json, subprocess, sys, tempfile, unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import backend


class Diagnostics(unittest.TestCase):
    def test_report_has_the_support_sections_and_is_written_to_logs(self):
        with tempfile.TemporaryDirectory() as temp:
            result = backend.diagnostics(Path(temp))
        text = result["text"]
        for section in ("[build]", "[warnings]", "[settings]", "[launch arguments]", "[runtime log:", "[launcher journal:"):
            self.assertIn(section, text)
        self.assertIn("features (", text)
        self.assertIn("os: ", text)
        if result["file"]:
            path = Path(result["file"])
            self.assertTrue(path.is_file())
            path.unlink()

    def test_tail_is_bounded_and_missing_files_are_empty(self):
        with tempfile.TemporaryDirectory() as temp:
            log = Path(temp) / "x.log"
            log.write_text("\n".join(f"line {i}" for i in range(2000)), encoding="utf-8")
            tail = backend._tail(log, 200)
            self.assertLess(len(tail), 260)
            self.assertTrue(tail.startswith("[...]"))
            self.assertIn("line 1999", tail)
            self.assertEqual(backend._tail(Path(temp) / "missing.log"), "")

    def test_snapshot_reports_platform_and_sources(self):
        with tempfile.TemporaryDirectory() as temp:
            snapshot = backend.snapshot(Path(temp))
        self.assertIn(snapshot["platform"], ("windows", "linux"))
        self.assertIn(snapshot["features_source"], ("manifest", "scan"))
        self.assertIn("warnings", snapshot)


if __name__ == "__main__":
    unittest.main()
