#!/usr/bin/env python3
"""Sampling evidence must not merge duplicate names or infer concurrency from placement."""
import json, tempfile, unittest
from pathlib import Path
from analyze_cpu_parallelism import summarize

class ParallelismTests(unittest.TestCase):
    def run_rows(self,rows):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"samples.jsonl"
            p.write_text("".join(json.dumps(r)+"\n" for r in rows))
            return summarize(p)
    def test_duplicate_names_and_migration(self):
        rows=[
          {"time":10,"game_age_s":120,"threads":{"1":{"name":"worker","last_cpu":0,"allowed_cpus":"0-11"},"2":{"name":"worker","last_cpu":5,"allowed_cpus":"5"}}},
          {"time":11,"game_age_s":121,"threads":{"1":{"name":"worker","cpu_percent":80,"last_cpu":6,"allowed_cpus":"0-11"},"2":{"name":"worker","cpu_percent":20,"last_cpu":5,"allowed_cpus":"5"}}},
          {"time":12,"game_age_s":122,"threads":{"1":{"name":"worker","cpu_percent":40,"last_cpu":6,"allowed_cpus":"0-11"}}}]
        result=self.run_rows(rows)
        self.assertEqual(len(result["threads"]),2)
        self.assertEqual(result["threads"][0]["mean_core_percent"],60)
        self.assertEqual(result["threads"][0]["placements"],{"0":1,"6":2})
        self.assertEqual(result["threads"][1]["cpu_samples"],1)
        self.assertEqual(result["threads"][1]["allowed_cpus"],["5"])
    def test_reject_bad_gate_or_clock(self):
        for rows in [
          [{"time":1,"game_age_s":89,"threads":{}}],
          [{"time":2,"game_age_s":120,"threads":{}},{"time":1,"game_age_s":121,"threads":{}}]]:
            with self.assertRaises(ValueError):self.run_rows(rows)
if __name__=="__main__":unittest.main()
