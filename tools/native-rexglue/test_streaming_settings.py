#!/usr/bin/env python3
"""Memory/streaming settings and recovery controls; no effect or resolution runtime tests."""
import copy,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"tools/native-rexglue"))
sys.path.insert(0,str(ROOT/"tools/acv-trg-launcher"))
import acv_settings as S
import backend
class StreamingSettings(unittest.TestCase):
    def test_budget_and_auto_read_ahead(self):
        values=S.resolve_graphics()
        self.assertEqual((values["texture_cache_soft_mb"],values["texture_cache_hard_mb"],values["texture_cache_retention_s"]),(1024,1536,180))
        args=S.graphics_to_cvars({},features={"asset_read_ahead_mb"})
        for flag in ["--texture_cache_memory_limit_soft=1024","--texture_cache_memory_limit_hard=1536",
                     "--texture_cache_memory_limit_soft_lifetime=180","--asset_read_ahead_mb=16"]:
            self.assertIn(flag,args)
        self.assertFalse(any(x.startswith("--asset_read_ahead_mb=") for x in S.graphics_to_cvars({})))
        with self.assertRaises(S.SettingsError):S.graphics_to_cvars({"asset_read_ahead_mb":32})
    def test_invalid_budget_and_types(self):
        for values in [{"texture_cache_soft_mb":2048,"texture_cache_hard_mb":768},
                       {"texture_cache_hard_mb":True},{"asset_read_ahead_mb":-1},
                       {"texture_cache_retention_s":0}]:
            with self.assertRaises(S.SettingsError):S.resolve_graphics(values)
        self.assertEqual(S.resolve_graphics({"texture_cache_hard_mb":3072},{"texture_cache_soft_mb":2048})["texture_cache_soft_mb"],2048)
    def test_presets_and_saved_atomicity(self):
        for values in S.GRAPHICS_PRESETS.values():S.graphics_to_cvars(values,features={"asset_read_ahead_mb"})
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            doc={"schema_version":1,"graphics":{},"launcher":{}}
            backend.save(root,doc)
            before=(root/"settings.json").read_bytes()
            bad=copy.deepcopy(doc);bad["graphics"]={"texture_cache_soft_mb":2048,"texture_cache_hard_mb":768}
            with self.assertRaises(S.SettingsError):backend.save(root,bad)
            self.assertEqual((root/"settings.json").read_bytes(),before)
    def test_budget_controls_can_be_edited_together_before_saving(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            backend.save(root,{"schema_version":1,"graphics":{},"launcher":{}})
            caps=backend.snapshot(root)["capabilities"]
            for key in ("texture_cache_soft_mb","texture_cache_hard_mb"):
                self.assertTrue(all(o["selectable"] for o in caps[key]["options"]),key)
            backend.save(root,{"schema_version":1,"graphics":{"texture_cache_soft_mb":2048,"texture_cache_hard_mb":3072},"launcher":{}})
            saved=S.load_json(root/"settings.json","settings")["graphics"]
            self.assertEqual((saved["texture_cache_soft_mb"],saved["texture_cache_hard_mb"]),(2048,3072))
    def test_unrelated_bad_experimental_value_does_not_lock_all_controls(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            S.save_json_atomic(root/"settings.json",{"schema_version":1,"graphics":{"render_scale":2},"launcher":{"allow_experimental":False}})
            result=backend.snapshot(root)
            options=result["capabilities"]
            self.assertTrue(next(o for o in options["window_mode"]["options"] if o["value"]=="windowed")["selectable"])
            self.assertTrue(next(o for o in options["render_scale"]["options"] if o["value"]==1)["selectable"])
            self.assertFalse(next(o for o in options["render_scale"]["options"] if o["value"]==2)["selectable"])
if __name__=="__main__":unittest.main()
