#!/usr/bin/env python3
"""CLI for the local context tools. Examples:

  ctx.py index [--full] [--quiet]
  ctx.py search "readback resolve" -k 5 --max-tokens 1500
  ctx.py log logs/acv-native-linux/<run>/runtime.log --max-tokens 800
  ctx.py tree --depth 2
  ctx.py assemble "FPS readback" --files tools/native-rexglue/probe_linux.py --logs logs/x/runtime.log --total 12000
  ctx.py stats
"""
from __future__ import annotations
import sys as _sys
if _sys.version_info < (3, 15):
    raise SystemExit('requires Python 3.15+; run: uv run --no-project --python 3.15 python -I <script>')

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ctxlib as C  # noqa: E402


def main(argv=None) -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--root", type=Path, default=None)
    sub = p.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("index"); s.add_argument("--full", action="store_true"); s.add_argument("--quiet", action="store_true")
    s = sub.add_parser("search"); s.add_argument("query"); s.add_argument("-k", type=int, default=5)
    s.add_argument("--max-tokens", type=int, default=1500); s.add_argument("--path", default="")
    s = sub.add_parser("log"); s.add_argument("path"); s.add_argument("--max-tokens", type=int, default=800)
    s = sub.add_parser("tree"); s.add_argument("--depth", type=int, default=2); s.add_argument("--max-tokens", type=int, default=700)
    s = sub.add_parser("assemble"); s.add_argument("query", nargs="?", default="")
    s.add_argument("--files", nargs="*", default=[]); s.add_argument("--logs", nargs="*", default=[])
    s.add_argument("--total", type=int, default=12000); s.add_argument("--state", default="X360_PROJECT_STATE", help="knowledge-base slug for the architecture block")
    sub.add_parser("stats")
    a = p.parse_args(argv)
    root = (a.root or C.project_root()).resolve()

    if a.cmd == "index":
        stats = C.Index(root).refresh(full=a.full)
        if not a.quiet:
            print(stats)
        return 0
    idx = C.Index(root)
    if a.cmd == "search":
        if not idx.con.execute("SELECT 1 FROM docs LIMIT 1").fetchone():
            idx.refresh()
        print(C.format_hits(idx.search(a.query, a.k, a.path), a.max_tokens) or "no matches")
    elif a.cmd == "log":
        path = (root / a.path).resolve()
        if root not in path.parents:
            print("path outside project", file=sys.stderr); return 2
        d = C.digest_log(C.read_tail(path), a.max_tokens)
        print(f"# {a.path}: {d['lines_total']} lines, {d['distinct']} distinct, {d['tokens_in']}->{d['tokens_out']} tok est.")
        print(d["text"])
    elif a.cmd == "tree":
        print(C.tree(root, a.depth, a.max_tokens))
    elif a.cmd == "assemble":
        if not idx.con.execute("SELECT 1 FROM docs LIMIT 1").fetchone():
            idx.refresh()
        r = C.assemble(root, a.query, a.files, a.logs, a.total, a.state, index=idx)
        print(r["text"]); print(f"\n[used {r['total']}/{a.total} tok est.: {r['used']}]", file=sys.stderr)
    elif a.cmd == "stats":
        n_docs = idx.con.execute("SELECT count(*) FROM docs").fetchone()[0]
        n_chunks, chars = idx.con.execute("SELECT count(*), coalesce(sum(length(body)),0) FROM chunks").fetchone()
        print(f"db={idx.db_file}\ndocs={n_docs} chunks={n_chunks} corpus~{int(chars / C.CHARS_PER_TOKEN)} tok est.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
