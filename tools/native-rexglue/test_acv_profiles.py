#!/usr/bin/env python3
"""Tests for the persistent profile store (stage B)."""
import multiprocessing, sys, tempfile, unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import acv_profiles as p
import acv_settings as s


def _hold(root, uid, ready, release):
    with p.ProfileStore(root).lock(uid):
        ready.set()
        release.wait(10)


class StoreTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.store = p.ProfileStore(self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def write(self, profile, name, data):
        f = self.store.userdata(profile.uuid) / name
        f.parent.mkdir(parents=True, exist_ok=True)
        f.write_bytes(data)

    def test_isolation_and_rename(self):
        a, b = self.store.create("A"), self.store.create("B")
        self.assertNotEqual(self.store.userdata(a.uuid), self.store.userdata(b.uuid))
        self.write(a, "save.bin", b"1")
        self.assertFalse((self.store.userdata(b.uuid) / "save.bin").exists())
        r = self.store.rename(a.uuid, "Renamed")
        self.assertEqual((r.uuid, r.xuid), (a.uuid, a.xuid))
        self.assertEqual((self.store.userdata(a.uuid) / "save.bin").read_bytes(), b"1")
        self.assertEqual({x.uuid for x in self.store.list()}, {a.uuid, b.uuid})

    def test_rejects_bad_uuid(self):
        for bad in ("../x", "A" * 36, "", None):
            with self.assertRaises(s.SettingsError):
                self.store.userdata(bad)

    def test_backup_restore_keeps_original(self):
        a = self.store.create("A")
        self.write(a, "d/save.bin", b"good")
        backup = self.store.backup(a.uuid)
        self.assertTrue(self.store.verify_backup(backup))
        self.write(a, "d/save.bin", b"corrupt")
        quarantined = self.store.restore(a.uuid, backup)
        self.assertEqual((self.store.userdata(a.uuid) / "d/save.bin").read_bytes(), b"good")
        self.assertEqual((quarantined / "d/save.bin").read_bytes(), b"corrupt")

    def test_damaged_backup_refused(self):
        a = self.store.create("A")
        self.write(a, "save.bin", b"good")
        backup = self.store.backup(a.uuid)
        (backup / "userdata/save.bin").write_bytes(b"bad")
        with self.assertRaises(p.BackupError):
            self.store.restore(a.uuid, backup)
        self.assertEqual((self.store.userdata(a.uuid) / "save.bin").read_bytes(), b"good")

    def test_lock_blocks_second_writer_and_restore(self):
        a = self.store.create("A")
        self.write(a, "save.bin", b"x")
        backup = self.store.backup(a.uuid)
        ready, release = multiprocessing.Event(), multiprocessing.Event()
        proc = multiprocessing.Process(target=_hold, args=(self.tmp.name, a.uuid, ready, release))
        proc.start()
        try:
            self.assertTrue(ready.wait(10))
            with self.assertRaises(p.ProfileBusy):
                self.store.lock(a.uuid).acquire()
            with self.assertRaises(p.ProfileBusy):
                self.store.restore(a.uuid, backup)
        finally:
            release.set()
            proc.join()
        with self.store.lock(a.uuid):  # released after the owner exits
            pass

    def test_lock_released_when_owner_killed(self):
        a = self.store.create("A")
        ready, release = multiprocessing.Event(), multiprocessing.Event()
        proc = multiprocessing.Process(target=_hold, args=(self.tmp.name, a.uuid, ready, release))
        proc.start()
        self.assertTrue(ready.wait(10))
        proc.kill()
        proc.join()
        with self.store.lock(a.uuid):
            pass

    def test_import(self):
        a = self.store.create("A")
        with tempfile.TemporaryDirectory() as src:
            (Path(src) / "B13E").mkdir()
            (Path(src) / "B13E/save").write_bytes(b"s")
            self.store.import_saves(a.uuid, src)
        self.assertEqual((self.store.userdata(a.uuid) / "B13E/save").read_bytes(), b"s")
        with tempfile.TemporaryDirectory() as empty, self.assertRaises(p.BackupError):
            self.store.import_saves(a.uuid, empty)


if __name__ == "__main__":
    unittest.main()
