import sys as _sys
if _sys.version_info < (3, 15):
    raise SystemExit('requires Python 3.15+; run: uv run --no-project --python 3.15 python -I <script>')
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import ctxlib as C  # noqa: E402


def make_project(tmp: Path) -> Path:
    (tmp / "docs").mkdir()
    (tmp / "docs" / "PLAN.md").write_text(
        "# Plan\nintro text\n\n## Readback\nreadback_resolve full stalls the GPU queue.\n\n"
        "## Звук\nЗаписи звука читаются из архива dvdbnd.\n", encoding="utf-8")
    (tmp / "docs" / "STATE.md").write_text(
        "# State\n\n## Architecture\n" + "arch line about the renderer\n" * 400 + "\n## Readback notes\nreadback is slow\n",
        encoding="utf-8")
    (tmp / "src").mkdir()
    (tmp / "src" / "big.py").write_text("\n".join(f"line {i} filler" for i in range(3000)) + "\nneedle readback here\n")
    (tmp / "run.log").write_text(
        "\n".join(f"[2026-11-05 21:00:{i % 60:02d}.100] [info] [gpu] [t1] pipeline {i} created" for i in range(500))
        + "\n[2026-11-05 21:01:00.000] [warning] [krnl] [t2] open FAILED: path='x' -> 0xc000000f\n" * 30
        + "[2026-11-05 21:02:00.000] [error] [sys] [t3] Unhandled violation at 0x1234abcd\n")
    return tmp


class Core(unittest.TestCase):
    def test_chunks_keep_heading_path_and_lines(self):
        chunks = C.chunk_markdown("# A\nx\n## B\ny\n```\n# not heading\n```\n## C\nz\n")
        paths = [c[0] for c in chunks]
        self.assertIn("A > B", paths)
        self.assertIn("A > C", paths)
        self.assertFalse(any("not heading" in p for p in paths))
        b = next(c for c in chunks if c[0] == "A > B")
        self.assertEqual((b[1], b[2]), (3, 7))

    def test_search_incremental_and_russian(self):
        with tempfile.TemporaryDirectory() as t:
            root = make_project(Path(t))
            idx = C.Index(root, root / "idx.sqlite")
            self.assertEqual(idx.refresh()["added"], 2)
            hit = idx.search("readback resolve")[0]
            self.assertEqual(hit["path"], "docs/PLAN.md")
            self.assertTrue(idx.search("звука архив"))  # inflected Russian query matches
            self.assertEqual(idx.refresh()["updated"], 0)
            (root / "docs" / "PLAN.md").write_text("# Plan\n\n## Other\nnothing\n")
            os.utime(root / "docs" / "PLAN.md", ns=(1, 1))
            self.assertEqual(idx.refresh()["updated"], 1)
            self.assertFalse([h for h in idx.search("stalls") if h["path"] == "docs/PLAN.md"])
            (root / "docs" / "PLAN.md").unlink()
            self.assertEqual(idx.refresh()["removed"], 1)

    def test_llmignore_respected(self):
        with tempfile.TemporaryDirectory() as t:
            root = make_project(Path(t))
            (root / "secret").mkdir()
            (root / "secret" / "N.md").write_text("# Hidden\nreadback hidden text\n")
            (root / ".llmignore").write_text("/secret/\n")
            idx = C.Index(root, root / "idx.sqlite")
            idx.refresh()
            self.assertFalse([h for h in idx.search("hidden") if h["path"].startswith("secret")])

    def test_log_digest_collapses_and_fits_budget(self):
        text = (Path(__file__).parent).joinpath("test_ctxlib.py").read_text() and (
            "\n".join(f"[2026-11-05 21:00:00.100] [info] [gpu] [t1] pipeline {i} created" for i in range(300))
            + "\n[2026-11-05 21:01:00.000] [warning] [k] open FAILED: path='x' -> 0xc000000f" * 20
            + "\n[2026-11-05 21:02:00.000] [error] [s] violation at 0x1234abcd\n")
        d = C.digest_log(text, 200)
        self.assertLess(d["tokens_out"], 210)
        lines = d["text"].splitlines()
        self.assertTrue(lines[0].startswith(("[FATAL]", "[ERR]")))
        self.assertTrue(any("x20" in l for l in lines))
        self.assertLess(d["tokens_out"] * 10, d["tokens_in"])
        self.assertNotIn("2026-11-05", d["text"])

    def test_assemble_hard_caps(self):
        with tempfile.TemporaryDirectory() as t:
            root = make_project(Path(t))
            idx = C.Index(root, root / "idx.sqlite")
            idx.refresh()
            import kb as K
            db = root / "kb.sqlite3"
            con = K.init(db)
            K.put(con, "STATE", (root / "docs" / "STATE.md").read_text(encoding="utf-8"), reason="test", kind="state")
            con.close()
            old_db = K.DB_PATH
            K.DB_PATH = db
            try:
                r = C.assemble(root, "readback", ["src/big.py", "../etc/passwd"], ["run.log"], 2000, "STATE", index=idx)
            finally:
                K.DB_PATH = old_db
            for key, cap in r["caps"].items():
                self.assertLessEqual(r["used"][key], cap + 3, key)
            self.assertEqual(r["caps"], {"architecture": 300, "code": 1200, "logs": 500})
            self.assertIn("needle readback here", r["text"])      # window around the query term
            self.assertIn("unavailable", r["text"])              # path escape refused
            self.assertIn("Readback notes", r["text"])           # query-relevant state section kept (from kb)


class Mcp(unittest.TestCase):
    def rpc(self, root, messages):
        env = dict(os.environ, CLAUDE_PROJECT_DIR=str(root), ACV_CTX_DB=str(root / "mcp.sqlite"))
        proc = subprocess.run([sys.executable, "-I", str(HERE / "mcp_server.py")], input="\n".join(map(json.dumps, messages)) + "\n",
                              capture_output=True, text=True, env=env, timeout=60)
        return [json.loads(l) for l in proc.stdout.splitlines()]

    def test_handshake_and_tools(self):
        with tempfile.TemporaryDirectory() as t:
            root = make_project(Path(t))
            out = self.rpc(root, [
                {"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {"protocolVersion": "2025-03-26"}},
                {"jsonrpc": "2.0", "method": "notifications/initialized"},
                {"jsonrpc": "2.0", "id": 2, "method": "tools/list"},
                {"jsonrpc": "2.0", "id": 3, "method": "tools/call", "params": {"name": "ctx_search", "arguments": {"query": "stalls"}}},
                {"jsonrpc": "2.0", "id": 4, "method": "tools/call", "params": {"name": "ctx_log_digest", "arguments": {"path": "../x"}}},
                {"jsonrpc": "2.0", "id": 5, "method": "nope"},
            ])
            by_id = {m["id"]: m for m in out}
            self.assertEqual(len(out), 5)  # the notification gets no reply
            self.assertEqual(by_id[1]["result"]["protocolVersion"], "2025-03-26")
            self.assertEqual({x["name"] for x in by_id[2]["result"]["tools"]},
                             {"ctx_search", "ctx_log_digest", "ctx_tree", "ctx_assemble", "ctx_reindex",
                              "kb_outline", "kb_get", "kb_rows", "kb_search", "kb_card", "kb_index"})
            self.assertIn("docs/PLAN.md", by_id[3]["result"]["content"][0]["text"])
            self.assertTrue(by_id[4]["result"]["isError"])
            self.assertEqual(by_id[5]["error"]["code"], -32601)


if __name__ == "__main__":
    unittest.main()
