#!/usr/bin/env python3
import os, sys, tempfile, unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import acv_shm as m


class ShmTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.shm = Path(self.tmp.name) / "shm"; self.shm.mkdir()
        self.proc = Path(self.tmp.name) / "proc"; (self.proc / "123").mkdir(parents=True)

    def tearDown(self):
        self.tmp.cleanup()

    def make(self, name, size=4096):
        p = self.shm / name
        p.write_bytes(b"x" * size)
        return str(p)

    def test_only_unmapped_matching_files_are_orphans(self):
        orphan = self.make("xenia_memory_1")
        live = self.make("xenia_memory_2")
        other = self.make("something_else")
        (self.proc / "123" / "maps").write_text(f"7f00-7f01 rw-s 0 00:1 5 {live}\n")
        found = [i["path"] for i in m.orphans(str(self.shm), str(self.proc))]
        self.assertEqual(found, [orphan])
        self.assertTrue(os.path.exists(other))

    def test_clean_removes_only_orphans(self):
        orphan = self.make("xenia_memory_1")
        live = self.make("xenia_memory_2")
        (self.proc / "123" / "maps").write_text(f"a-b rw-s 0 00:1 5 {live} (deleted)\n")
        removed = m.clean(str(self.shm), str(self.proc))
        self.assertEqual([r["path"] for r in removed], [orphan])
        self.assertFalse(os.path.exists(orphan)); self.assertTrue(os.path.exists(live))

    def test_status_counts(self):
        self.make("xenia_memory_1", 8192)
        s = m.status(str(self.shm), str(self.proc))
        self.assertEqual(s["orphans"], 1); self.assertGreaterEqual(s["orphan_bytes"], 8192)


if __name__ == "__main__":
    unittest.main()
