#!/usr/bin/env python3
"""Minimal MCP stdio server (newline-delimited JSON-RPC) exposing the local context tools.

Reads only inside CLAUDE_PROJECT_DIR (or cwd). No network, no writes except the index database.
"""
from __future__ import annotations
import sys as _sys
if _sys.version_info < (3, 15):
    raise SystemExit('requires Python 3.15+; run: uv run --no-project --python 3.15 python -I <script>')

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ctxlib as C  # noqa: E402
import kb as K  # noqa: E402

ROOT = C.project_root()
_index: C.Index | None = None

TOOLS = [
    {"name": "ctx_search", "description": "Full-text (BM25) search over the project's Markdown notes. Returns only the matching "
     "chunks with path:lines citations. Use instead of reading whole .md files.",
     "inputSchema": {"type": "object", "properties": {"query": {"type": "string"}, "k": {"type": "integer", "default": 5},
                                                      "max_tokens": {"type": "integer", "default": 1500},
                                                      "path_prefix": {"type": "string", "default": ""}}, "required": ["query"]}},
    {"name": "ctx_log_digest", "description": "Compressed digest of a log file inside the project: timestamps stripped, repeats "
     "collapsed (xN), errors/warnings first. Use instead of reading a raw log.",
     "inputSchema": {"type": "object", "properties": {"path": {"type": "string"}, "max_tokens": {"type": "integer", "default": 800}},
                     "required": ["path"]}},
    {"name": "ctx_tree", "description": "Compact project file tree without contents (honors .llmignore).",
     "inputSchema": {"type": "object", "properties": {"depth": {"type": "integer", "default": 2},
                                                      "max_tokens": {"type": "integer", "default": 700}}}},
    {"name": "ctx_assemble", "description": "Build a context pack with hard token caps: architecture 15%, code/working files 60%, "
     "logs 25% of total_tokens.",
     "inputSchema": {"type": "object", "properties": {"query": {"type": "string", "default": ""},
                                                      "files": {"type": "array", "items": {"type": "string"}},
                                                      "logs": {"type": "array", "items": {"type": "string"}},
                                                      "total_tokens": {"type": "integer", "default": 12000},
                                                      "state_slug": {"type": "string", "default": "X360_PROJECT_STATE"}}}},
    {"name": "ctx_reindex", "description": "Refresh the Markdown index (incremental unless full=true).",
     "inputSchema": {"type": "object", "properties": {"full": {"type": "boolean", "default": False}}}},
    {"name": "kb_outline", "description": "Outline of a project knowledge-base document: section addresses kb:SLUG#anchor, "
     "sizes and table/record counts. Call this before kb_get to pick the right section.",
     "inputSchema": {"type": "object", "properties": {"slug": {"type": "string", "description": "e.g. PLAN_GUI, X360_PROJECT_STATE, AGENTS"}},
                     "required": ["slug"]}},
    {"name": "kb_get", "description": "Read one knowledge-base section (kb:SLUG#anchor) or a whole document (kb:SLUG). "
     "Returns the text with its address.",
     "inputSchema": {"type": "object", "properties": {"target": {"type": "string", "description": "SLUG or SLUG#anchor"}},
                     "required": ["target"]}},
    {"name": "kb_rows", "description": "Structured records of a document as JSON lines: kind=item (numbered queue items), "
     "table (table rows keyed by header) or dated (dated journal entries). Use limit for the newest entries.",
     "inputSchema": {"type": "object", "properties": {"slug": {"type": "string"},
                                                      "kind": {"type": "string", "enum": ["item", "table", "dated"]},
                                                      "where": {"type": "string", "description": "COL=VALUE filter"},
                                                      "limit": {"type": "integer"}},
                     "required": ["slug"]}},
    {"name": "kb_card", "description": "Routing card of a knowledge-base document: summary, keywords, counts, date range, "
     "outgoing kb: references. Read this to decide whether to open the document.",
     "inputSchema": {"type": "object", "properties": {"slug": {"type": "string"}}, "required": ["slug"]}},
    {"name": "kb_index", "description": "llms.txt-style index of the knowledge base: one routing card per active document, "
     "grouped by kind. Start here when the document is not known.",
     "inputSchema": {"type": "object", "properties": {"kind": {"type": "string"}, "include_archived": {"type": "boolean"}}}},
    {"name": "kb_search", "description": "Full-text search (BM25) over knowledge-base sections and table rows. "
     "Returns addresses and short text, not whole documents.",
     "inputSchema": {"type": "object", "properties": {"query": {"type": "string"}, "k": {"type": "integer", "default": 8}},
                     "required": ["query"]}},
]


def index() -> C.Index:
    global _index
    if _index is None:
        _index = C.Index(ROOT)
        if not _index.con.execute("SELECT 1 FROM docs LIMIT 1").fetchone():
            _index.refresh()
    return _index


def safe(rel: str) -> Path:
    path = (ROOT / rel).resolve()
    if ROOT not in path.parents or ".git" in path.parts or not path.is_file():
        raise ValueError(f"path not allowed or missing: {rel}")
    return path


def call(name: str, a: dict) -> str:
    if name == "ctx_search":
        return C.format_hits(index().search(a["query"], int(a.get("k", 5)), a.get("path_prefix", "")),
                             int(a.get("max_tokens", 1500))) or "no matches"
    if name == "ctx_log_digest":
        d = C.digest_log(C.read_tail(safe(a["path"])), int(a.get("max_tokens", 800)))
        return (f"# {a['path']}: {d['lines_total']} lines, {d['distinct']} distinct, "
                f"{d['tokens_in']}->{d['tokens_out']} tok est.\n{d['text']}")
    if name == "ctx_tree":
        return C.tree(ROOT, int(a.get("depth", 2)), int(a.get("max_tokens", 700)))
    if name == "ctx_assemble":
        return C.assemble(ROOT, a.get("query", ""), a.get("files", []), a.get("logs", []),
                          int(a.get("total_tokens", 12000)), a.get("state_slug", "X360_PROJECT_STATE"),
                          index=index())["text"]
    if name == "ctx_reindex":
        return json.dumps(index().refresh(full=bool(a.get("full", False))))
    if name.startswith("kb_"):
        con = K.connect(K.DB_PATH)
        try:
            if name == "kb_outline":
                return K.outline(con, a["slug"])
            if name == "kb_get":
                return K.get(con, a["target"], None, "llm")
            if name == "kb_rows":
                return "\n".join(K.rows(con, a["slug"], None, a.get("where"), a.get("kind"),
                                        int(a["limit"]) if a.get("limit") else None))
            if name == "kb_card":
                return json.dumps(K.card(con, a["slug"]), ensure_ascii=False, indent=2)
            if name == "kb_index":
                return K.index_text(con, bool(a.get("include_archived", False)), a.get("kind") or None)
            if name == "kb_search":
                hits = K.search(con, a["query"], int(a.get("k", 8)))
                return "\n\n".join(f"### {h['ref']}  [{h['where']}]\n{h['text'][:800]}" for h in hits) or "no matches"
        finally:
            con.close()
    raise ValueError(f"unknown tool {name}")


def handle(msg: dict):
    method, mid = msg.get("method"), msg.get("id")
    if method == "initialize":
        version = (msg.get("params") or {}).get("protocolVersion", "2024-11-05")
        return {"protocolVersion": version, "capabilities": {"tools": {}},
                "serverInfo": {"name": "acv-context", "version": "0.1.0"}}
    if method == "ping":
        return {}
    if method == "tools/list":
        return {"tools": TOOLS}
    if method == "tools/call":
        params = msg.get("params") or {}
        try:
            text = call(params.get("name", ""), params.get("arguments") or {})
            return {"content": [{"type": "text", "text": text}]}
        except Exception as error:  # report as tool error, keep serving
            return {"content": [{"type": "text", "text": f"error: {error}"}], "isError": True}
    if mid is None:
        return None  # notification
    raise KeyError(method)


def main() -> int:
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            msg = json.loads(line)
        except json.JSONDecodeError:
            continue
        try:
            result = handle(msg)
            reply = None if msg.get("id") is None or result is None else {"jsonrpc": "2.0", "id": msg["id"], "result": result}
        except KeyError:
            reply = {"jsonrpc": "2.0", "id": msg.get("id"), "error": {"code": -32601, "message": "method not found"}}
        if reply is not None:
            sys.stdout.write(json.dumps(reply, ensure_ascii=False) + "\n")
            sys.stdout.flush()
    return 0


if __name__ == "__main__":
    sys.exit(main())
