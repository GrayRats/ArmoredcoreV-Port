import sys, tempfile, unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import acv_paths as P


class ResolveBuild(unittest.TestCase):
    def setUp(self):
        self._temp = tempfile.TemporaryDirectory()
        self.addCleanup(self._temp.cleanup)
        base = Path(self._temp.name)
        self.root = base / "ACV-Windows-release-2.2"
        self.exe = self.root / "bin/game/acv_windows_probe.exe"
        self.old = base / "ACV-2.2/bin/game/acv_windows_probe.exe"
        self.other = base / "custom/tool.exe"
        for path in (self.exe, self.old, self.other, self.root / "bin/game/second.exe"):
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"x")

    def resolve(self, saved, packaged=True):
        return P.resolve_build(saved, root=self.root, executable=self.exe, packaged=packaged)

    def test_empty_follows_package(self):
        self.assertEqual(self.resolve(""), (str(self.exe), "package", None))
        self.assertEqual(self.resolve(None), (str(self.exe), "package", None))

    def test_own_executable_is_package(self):
        self.assertEqual(self.resolve(str(self.exe)), (str(self.exe), "package", None))

    def test_stale_path_from_another_folder_is_replaced_with_warning(self):
        path, status, warning = self.resolve(str(self.old))
        self.assertEqual((path, status), (str(self.exe), "relocated"))
        self.assertIn("другую папку", warning)

    def test_build_chosen_outside_the_package_stays_pinned(self):
        path, status, warning = self.resolve(str(self.other))
        self.assertEqual((path, status), (str(self.other), "external"))
        self.assertIn("внешняя", warning)

    def test_missing_build_falls_back_to_package(self):
        path, status, warning = self.resolve(str(self.other.parent / "gone.exe"))
        self.assertEqual((path, status), (str(self.exe), "missing"))
        self.assertIn("не найдена", warning)

    def test_other_file_inside_the_package_is_kept(self):
        second = self.root / "bin/game/second.exe"
        self.assertEqual(self.resolve(str(second)), (str(second), "package", None))

    def test_development_tree_uses_the_saved_path(self):
        self.assertEqual(self.resolve(str(self.old), packaged=False), (str(self.old), "dev", None))
        self.assertEqual(self.resolve("", packaged=False), (str(self.exe), "dev", None))


if __name__ == "__main__":
    unittest.main()
