"""Grouping and shard planning of acv_shard_dvdbnd (no game data needed)."""
import sys, unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import acv_shard_dvdbnd as S


def entry(path, offset, size):
    return {"path": path, "offset": offset, "size": size}


class ShardPlanTest(unittest.TestCase):
    def test_groups(self):
        self.assertEqual(S.group_of("model/map/m0300/m0300_htdcx.bnd"), "map-m0300")
        self.assertEqual(S.group_of("model/chr/a.flver"), "model-chr")
        self.assertEqual(S.group_of("model/x.bin"), "model-root")
        self.assertEqual(S.group_of("lang/jp/menu/trade.drb"), "lang-jp")
        self.assertEqual(S.group_of("sound/bgm.fsb"), "sound")
        self.assertEqual(S.group_of("param/a.param"), "core")
        self.assertEqual(S.group_of("_unknown/0000abcd"), "unknown")
        self.assertEqual(S.group_of(None), "unknown")

    def test_plan_keeps_archive_order_and_splits_by_size(self):
        entries = [entry("sound/b", 9000, 3000), entry("sound/a", 100, 3000), entry("sound/c", 20000, 3000)]
        shards = S.plan(entries, max_bytes=2 * 4096)  # each entry pads to 4096 -> two per shard
        self.assertEqual(list(shards), ["sound-1", "sound-2"])
        self.assertEqual([e["path"] for e in shards["sound-1"]], ["sound/a", "sound/b"])
        self.assertEqual([e["path"] for e in shards["sound-2"]], ["sound/c"])

    def test_gaps_are_found_and_grouped(self):
        entries = [entry("a", 16, 100), entry("b", 116, 0), entry("c", 300, 50)]
        self.assertEqual(S.find_gaps(entries, 400), [(0, 16), (116, 184), (350, 50)])
        self.assertEqual(S.group_of("_gap/0"), "gaps")

    def test_small_group_keeps_plain_name(self):
        shards = S.plan([entry("param/a", 0, 10)], max_bytes=1 << 20)
        self.assertEqual(list(shards), ["core"])

    def test_path_hash_matches_loose_unpack(self):
        self.assertEqual(S.path_hash("/a"), ord("/") * 37 + ord("a"))

    def test_binary_index_round_trip(self):
        import tempfile
        index = {"align": 2048, "source": {"bdt_size": 10000},
                 "shards": [{"name": "core", "file": "core.pak", "size": 4096}, {"name": "sound", "file": "sound.pak", "size": 2048}],
                 "entries": [{"orig_offset": 5000, "size": 100, "shard": "sound", "offset": 0},
                             {"orig_offset": 16, "size": 3000, "shard": "core", "offset": 2048}]}
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "shards.idx"
            S.write_idx(index, path)
            parsed = S.read_idx(path)
        self.assertEqual(parsed["virtual_size"], 10000)
        self.assertEqual([s["file"] for s in parsed["shards"]], ["core.pak", "sound.pak"])
        self.assertEqual([(e["virtual_offset"], e["shard"], e["shard_offset"]) for e in parsed["extents"]],
                         [(16, 0, 2048), (5000, 1, 0)])


if __name__ == "__main__":
    unittest.main()
