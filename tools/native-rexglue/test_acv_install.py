import hashlib,json,sys,tempfile,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import acv_install
class Install(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name);self.manifest=self.root/"manifest.json"
  self.manifest.write_text(json.dumps({"files":{"default.xex":{"size":4,"sha256":hashlib.sha256(b"XEX2").hexdigest()},"payload":{"size":4,"sha256":hashlib.sha256(b"data").hexdigest()}}}))
 def tearDown(self):self.tmp.cleanup()
 def stage(self):
  d=self.root/"game.staging-test";d.mkdir();(d/"default.xex").write_bytes(b"XEX2");(d/"payload").write_bytes(b"data");return d
 def test_verify_and_publish(self):
  d=self.stage();self.assertEqual(acv_install.verify(d,manifest=self.manifest)["files"],2)
  dest=self.root/"game";acv_install.publish(d,dest,self.manifest)
  self.assertFalse(d.exists());self.assertEqual((dest/"payload").read_bytes(),b"data")
 def test_mismatch_preserves_existing(self):
  d=self.stage();dest=self.root/"game";dest.mkdir();(dest/"personal").write_bytes(b"old")
  with self.assertRaises(ValueError):acv_install.publish(d,dest,self.manifest)
  self.assertTrue(d.exists());self.assertEqual((dest/"personal").read_bytes(),b"old")
  (d/"payload").write_bytes(b"oops")
  with self.assertRaises(ValueError):acv_install.verify(d,manifest=self.manifest)
  with self.assertRaises(ValueError):acv_install.publish(d,self.root/"other",self.manifest)
 def test_missing_and_symlink(self):
  d=self.stage();(d/"payload").unlink()
  with self.assertRaises(ValueError):acv_install.verify(d,manifest=self.manifest)
  (d/"payload").symlink_to(d/"default.xex")
  with self.assertRaises(ValueError):acv_install.verify(d,manifest=self.manifest)
if __name__=="__main__":unittest.main()
