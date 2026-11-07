import sys as _sys
if _sys.version_info < (3, 15):
    raise SystemExit('requires Python 3.15+; run: uv run --no-project --python 3.15 python -I <script>')
import json
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import kb as K  # noqa: E402

DOC = """# Plan

intro line

## Readback
readback_resolve full stalls the queue.

| Item | Status | Acceptance |
|---|---|---|
| GUI-1 | open | matched scene |
| GUI-2 | accepted | | 

## Cache pipeline
pipeline cache file is written atomically
"""


class Store(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.con = K.init(Path(self.tmp.name) / "kb.sqlite3")

    def tearDown(self):
        self.con.close()
        self.tmp.cleanup()

    def test_versions_keep_every_byte(self):
        v2 = DOC.replace("full stalls", "some is cheaper")
        self.assertEqual(K.put(self.con, "PLAN", DOC, reason="t", kind="plan"), "created")
        self.assertEqual(K.put(self.con, "PLAN", DOC, reason="again"), "unchanged")
        self.assertEqual(K.put(self.con, "PLAN", v2, reason="edit"), "updated")
        self.assertEqual(K.get(self.con, "PLAN", version=1, fmt="md"), DOC)
        self.assertEqual(K.get(self.con, "PLAN", fmt="md"), v2)
        self.assertEqual(K.verify(self.con), [])

    def test_sections_have_stable_addresses(self):
        K.put(self.con, "PLAN", DOC, reason="t", kind="plan")
        anchors = [r["anchor"] for r in self.con.execute("SELECT anchor FROM section WHERE slug='PLAN' ORDER BY ord")]
        self.assertEqual(anchors, ["plan", "readback", "cache-pipeline"])  # no preamble: doc starts with a heading
        self.assertIn("stalls", K.get(self.con, "PLAN#readback", fmt="md"))
        self.assertIn("kb:PLAN#readback", K.get(self.con, "PLAN#readback"))
        with self.assertRaises(SystemExit):
            K.get(self.con, "PLAN#missing")

    def test_table_rows_are_json_records(self):
        K.put(self.con, "PLAN", DOC, reason="t", kind="plan")
        rows = [json.loads(x) for x in K.rows(self.con, "PLAN", None, None)]
        self.assertEqual(rows[0]["ref"], "kb:PLAN#readback/t1/r1")
        self.assertEqual(rows[0]["Item"], "GUI-1")
        self.assertEqual(rows[0]["Status"], "open")
        self.assertEqual(len(rows), 2)
        only = [json.loads(x) for x in K.rows(self.con, "PLAN", 1, "Status=accepted")]
        self.assertEqual([r["Item"] for r in only], ["GUI-2"])
        self.assertEqual(only[0]["Acceptance"], "")  # short row padded

    def test_search_returns_sections_and_rows(self):
        K.put(self.con, "PLAN", DOC, reason="t", kind="plan")
        refs = [h["ref"] for h in K.search(self.con, "pipeline cache", 5)]
        self.assertIn("kb:PLAN#cache-pipeline", refs)
        self.assertIn("kb:PLAN#readback/t1/r1", [h["ref"] for h in K.search(self.con, "GUI-1", 5)])

    def test_links_resolve_anchors(self):
        K.put(self.con, "PLAN", DOC, reason="t", kind="plan")
        K.put(self.con, "NOTE", "see kb:PLAN#readback and kb:PLAN#nope and kb:GONE\n", reason="t", kind="guide")
        status = {ref: ok for _, ref, ok in K.links(self.con)}
        self.assertTrue(status["PLAN#readback"])
        self.assertFalse(status["PLAN#nope"])
        self.assertFalse(status["GONE"])

    def test_tampering_and_stale_layers_are_reported(self):
        K.put(self.con, "PLAN", DOC, reason="t", kind="plan")
        self.con.execute("UPDATE doc SET body='x' WHERE slug='PLAN'")
        self.assertTrue(any("does not match" in p for p in K.verify(self.con)))
        self.con.execute("UPDATE doc SET body=? WHERE slug='PLAN'", (DOC,))
        self.con.execute("DELETE FROM section WHERE slug='PLAN' AND anchor='cache-pipeline'")
        self.assertTrue(any("derived layers stale" in p for p in K.verify(self.con)))

    def test_unknown_kind_rejected(self):
        with self.assertRaises(SystemExit):
            K.put(self.con, "Z", "x", reason="t", kind="bogus")

    def test_export_json_lines(self):
        import contextlib, io
        K.put(self.con, "PLAN", DOC, reason="t", kind="plan")
        self.con.close()
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            K.main(["--db", str(Path(self.tmp.name) / "kb.sqlite3"), "export-json", "PLAN"])
        lines = [json.loads(x) for x in buf.getvalue().splitlines()]
        self.assertEqual(lines[0]["type"], "doc")
        self.assertTrue(any(x["type"] == "record" and x["data"]["Item"] == "GUI-1" for x in lines))
        self.assertEqual(lines[0]["card"]["slug"], "PLAN")
        self.con = K.init(Path(self.tmp.name) / "kb.sqlite3")


class LlmFormat(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.con = K.init(Path(self.tmp.name) / "kb.sqlite3")

    def tearDown(self):
        self.con.close()
        self.tmp.cleanup()

    def test_card_summarizes_and_counts(self):
        K.put(self.con, "PLAN", DOC, reason="t", kind="plan")
        c = K.card(self.con, "PLAN")
        self.assertEqual(c["title"], "Plan")
        self.assertEqual(c["records"], {"table": 2, "item": 0, "dated": 0})
        self.assertEqual(c["summary"], "pipeline cache file is written atomically")  # first paragraph of at least 40 chars
        self.assertEqual(set(c["keywords"]), {"cache", "pipeline"})  # words repeated at least twice
        self.assertEqual(c["sections"], 3)

    def test_card_collects_dates_and_refs(self):
        body = "# J\n\n2026-11-04: first entry see kb:PLAN#readback\n\n2026-11-05: second entry\n"
        K.put(self.con, "J", body, reason="t", kind="state")
        c = K.card(self.con, "J")
        self.assertEqual((c["date_from"], c["date_to"]), ("2026-11-04", "2026-11-05"))
        self.assertIn("PLAN#readback", c["refs"])

    def test_index_hides_archived_unless_asked(self):
        K.put(self.con, "LIVE", "# Live\n\nlive text\n", reason="t", kind="guide")
        K.put(self.con, "OLD", "# Old\n\nold text about pipelines\n", reason="t", kind="archive", status="archived")
        self.assertIn("kb:LIVE", K.index_text(self.con))
        self.assertNotIn("kb:OLD", K.index_text(self.con))
        self.assertIn("kb:OLD", K.index_text(self.con, include_archived=True))
        self.assertEqual(K.search(self.con, "pipelines"), [])
        self.assertTrue(K.search(self.con, "pipelines", include_archived=True))

    def test_attachment_round_trip_and_tamper(self):
        data = bytes(range(256)) * 4
        with self.con:
            self.con.execute("INSERT INTO attachment(name,title,mime,size,sha256,legacy_path,reason,created,data)"
                             " VALUES (?,?,?,?,?,?,?,?,?)",
                             ("blob", "t", "application/octet-stream", len(data), K.sha_bytes(data), "x", "r",
                              K.now(), K.sqlite3.Binary(data)))
        self.assertEqual(K.verify(self.con), [])
        self.con.execute("UPDATE attachment SET data=? WHERE name='blob'", (K.sqlite3.Binary(b"x"),))
        self.assertTrue(any("attachment blob" in x for x in K.verify(self.con)))


if __name__ == "__main__":
    unittest.main()
