import json, os, sys, tempfile, unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import acv_settings as S


class CapabilitiesManifest(unittest.TestCase):
    def setUp(self):
        self._temp = tempfile.TemporaryDirectory()
        self.addCleanup(self._temp.cleanup)
        self.dir = Path(self._temp.name)
        self.gpu = self.dir / "rexgpu-xenos.dll"
        self.runtime = self.dir / "librexruntime.dll"
        self.gpu.write_bytes(b"..frame_limit_fps..D3D12GraphicsSystem..")
        self.runtime.write_bytes(b"..present_output_bits..")

    def test_without_manifest_the_binaries_are_scanned(self):
        features, source = S.runtime_features_info(self.dir)
        self.assertEqual(source, "scan")
        self.assertEqual(features, {"frame_limit_fps", "D3D12GraphicsSystem", "d3d12", "present_output_bits"})

    def test_written_manifest_is_used_and_matches_the_scan(self):
        doc = S.write_capabilities(self.dir)
        self.assertEqual(set(doc["files"]), {"rexgpu-xenos.dll", "librexruntime.dll"})
        features, source = S.runtime_features_info(self.dir)
        self.assertEqual(source, "manifest")
        self.assertEqual(features, S.scan_runtime_features(self.dir))

    def test_manifest_is_trusted_over_the_scan_while_binaries_match(self):
        S.write_capabilities(self.dir)
        path = self.dir / S.CAPABILITIES_FILE
        doc = json.loads(path.read_text(encoding="utf-8"))
        doc["features"] = ["from_manifest"]
        path.write_text(json.dumps(doc), encoding="utf-8")
        self.assertEqual(S.runtime_features_info(self.dir), ({"from_manifest"}, "manifest"))

    def test_copied_binary_with_new_mtime_but_same_content_still_matches(self):
        S.write_capabilities(self.dir)
        stat = self.gpu.stat()
        os.utime(self.gpu, ns=(stat.st_atime_ns, stat.st_mtime_ns + 10_000_000_000))
        self.assertEqual(S.runtime_features_info(self.dir)[1], "manifest")

    def test_changed_binary_falls_back_to_the_scan(self):
        S.write_capabilities(self.dir)
        stat = self.gpu.stat()
        self.gpu.write_bytes(b"..frame_limit_fps..xxxxxxxxxxxxxxxxxxxxx")  # same size, no D3D12
        self.assertEqual(len(self.gpu.read_bytes()), stat.st_size)
        os.utime(self.gpu, ns=(stat.st_atime_ns, stat.st_mtime_ns + 10_000_000_000))
        features, source = S.runtime_features_info(self.dir)
        self.assertEqual(source, "scan")
        self.assertNotIn("d3d12", features)

    def test_added_or_resized_binary_invalidates_the_manifest(self):
        S.write_capabilities(self.dir)
        self.runtime.write_bytes(b"..present_output_bits.. longer")
        self.assertEqual(S.runtime_features_info(self.dir)[1], "scan")
        S.write_capabilities(self.dir)
        (self.dir / "acv_windows_probe.exe").write_bytes(b"MZ")
        self.assertEqual(S.runtime_features_info(self.dir)[1], "scan")

    def test_malformed_manifest_is_ignored(self):
        (self.dir / S.CAPABILITIES_FILE).write_text("{not json", encoding="utf-8")
        self.assertEqual(S.runtime_features_info(self.dir)[1], "scan")
        (self.dir / S.CAPABILITIES_FILE).write_text(json.dumps({"schema_version": 2}), encoding="utf-8")
        self.assertEqual(S.runtime_features_info(self.dir)[1], "scan")


if __name__ == "__main__":
    unittest.main()
