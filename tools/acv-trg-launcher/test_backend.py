#!/usr/bin/env python3
"""Exercise the actual JSON boundary with isolated data; never touch game saves."""
import copy, hashlib, json, subprocess, sys, tempfile, unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BRIDGE=ROOT/"tools/acv-trg-launcher/backend.py"
CLI=ROOT/"tools/native-rexglue/acv_profile_cli.py"
class BridgeTest(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
        self.profile=self.call(CLI,["create","Тест","--gamertag","ARTEM","--pilot","Пилот"])
    def tearDown(self):self.tmp.cleanup()
    def call(self,script,args,doc=None,code=0):
        r=subprocess.run([sys.executable,"-I",str(script),"--profiles-root",str(self.root),*args],input=None if doc is None else json.dumps(doc),text=True,capture_output=True)
        self.assertEqual(r.returncode,code,r.stderr+r.stdout)
        return json.loads(r.stdout)
    def test_save_policy_and_reset(self):
        snap=self.call(BRIDGE,["snapshot"]);doc=snap["settings"]
        self.assertEqual(doc["launcher"]["profile"],self.profile["uuid"])
        self.assertFalse(next(o for o in snap["capabilities"]["fps_limit"]["options"] if o["value"]==165)["selectable"])
        doc["graphics"].update(resolution="2560x1080",vsync=False)
        self.call(BRIDGE,["save"],doc)
        before=(self.root/"settings.json").read_bytes()
        for change in [{"fps_limit":165},{"resolution":"$(touch bad)"},{"vsync":1},{"output_effect":"fsr3"}]:
            bad=copy.deepcopy(doc);bad["graphics"].update(change)
            self.assertIn("error",self.call(BRIDGE,["save"],bad,1))
            self.assertEqual((self.root/"settings.json").read_bytes(),before)
        for bad in [[],{"graphics":[]},{"launcher":{"build":[]}}, {"schema_version":True}]:
            self.assertIn("error",self.call(BRIDGE,["save"],bad,1))
        userdata=self.root/"profiles"/self.profile["uuid"]/"userdata";userdata.mkdir(exist_ok=True)
        save=userdata/"save.dat";save.write_bytes(b"unchanged game data")
        self.call(BRIDGE,["reset-graphics"])
        after=self.call(BRIDGE,["snapshot"])
        self.assertEqual(save.read_bytes(),b"unchanged game data")
        self.assertEqual(after["settings"]["launcher"]["profile"],self.profile["uuid"])
        self.assertEqual(after["settings"]["graphics"]["resolution"],"auto")
    def test_backup_restore(self):
        userdata=self.root/"profiles"/self.profile["uuid"]/"userdata";userdata.mkdir(exist_ok=True)
        f=userdata/"save.dat";f.write_bytes(b"first")
        backup=self.call(CLI,["backup",self.profile["uuid"]])["backup"]
        f.write_bytes(b"second")
        self.call(CLI,["restore",self.profile["uuid"],backup])
        self.assertEqual(f.read_bytes(),b"first")
if __name__=="__main__":unittest.main()
