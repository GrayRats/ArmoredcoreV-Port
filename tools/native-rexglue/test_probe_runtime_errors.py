"""Execute the actual runtime-log follower against a fatal record."""
import ast,collections,io,re,sys,tempfile,threading,types,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import acv_settings
class RuntimeErrors(unittest.TestCase):
 def test_fatal_record_is_counted(self):
  source=Path(__file__).with_name("probe_linux.py").read_text()
  node=next(n for n in ast.parse(source).body if isinstance(n,ast.FunctionDef) and n.name=="follow_runtime")
  with tempfile.TemporaryDirectory() as d:
   output=Path(d);(output/"runtime.log").write_text("[kernel] [critical] Unknown fatal test diagnostic\n")
   stop=threading.Event();stop.set()
   namespace=dict(output=output,runtime_stop=stop,runtime_errors=[],runtime_error_counts=collections.Counter(),fatal_error_count=0,fault_count=0,abort_reason=None,kill_timer=None,acv_settings=acv_settings,re=re,threading=threading,sys=types.SimpleNamespace(stdout=types.SimpleNamespace(buffer=io.BytesIO())))
   exec(compile(ast.Module(body=[node],type_ignores=[]),"probe-follower","exec"),namespace)
   namespace["follow_runtime"]()
   self.assertEqual(namespace["fatal_error_count"],1)
   self.assertEqual(sum(namespace["runtime_error_counts"].values()),1)
if __name__=="__main__":unittest.main()
