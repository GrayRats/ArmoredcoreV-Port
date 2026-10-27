import json,sys,tempfile,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import acv_sessions
class Sessions(unittest.TestCase):
 def test_xdg_live_and_complete(self):
  with tempfile.TemporaryDirectory() as d:
   logs=Path(d)
   self.assertFalse(acv_sessions.latest(logs)["available"])
   run=logs/"acv-native-linux/20261025T000000Z";run.mkdir(parents=True)
   (run/"runtime.log").write_text("[acv-display] applied width=2560 height=1080 format=44 colorspace=0 mode=0 requested_bits=10\n"+"x"*1500000+"\n[acv-display] pacing samples=300 limit=60 fps=59.8 mean_ms=16.7 p95_ms=17.0\n")
   live=acv_sessions.latest(logs)
   self.assertFalse(live["completed"])
   self.assertEqual(live["applied"]["bits"],8)
   self.assertTrue(live["applied"]["depth_fallback"])
   self.assertEqual(live["pacing_last"]["fps"],59.8)
   self.assertIsNotNone(live["idle_seconds"])
   (run/"launch.json").write_text(json.dumps({"child_returncode":0,"display":{"applied":{"width":1920},"pacing":[]}}))
   done=acv_sessions.latest(logs)
   self.assertTrue(done["completed"]);self.assertEqual(done["applied"]["width"],1920)
if __name__=="__main__":unittest.main()
