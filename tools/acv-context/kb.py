#!/usr/bin/env python3
"""Project knowledge base in SQLite, shaped for LLM retrieval (stdlib only).

Storage: tools/acv-context/knowledge.sqlite3 (WAL, foreign keys on). Read it through this CLI or the
acv-context MCP tools (kb_outline, kb_get, kb_rows, kb_search, kb_card, kb_index); do not write SQL by hand.

Layers, all derived from the stored bytes of each document:
  doc          current text and metadata: kind, status (active|archived|superseded), owner, SHA-256, version
  doc_version  every version with its bytes and SHA-256 (nothing is overwritten)
  card         one routing card per document: summary, keywords, counts, date range, outgoing kb: references
  section      one row per heading block: address kb:SLUG#anchor, breadcrumb, line range, text, token estimate
  trow         one record per structured line, JSON data, address kb:SLUG#anchor/tN/rM; kind =
               table  Markdown table row keyed by header        item   numbered item with bold title
               dated  paragraph starting with a date (journal)
  attachment   binary files (archives, images) stored as BLOB with SHA-256 and provenance

Addresses: kb:SLUG, kb:SLUG#anchor, kb:SLUG#anchor/tN/rM. Archived documents are excluded from search and
from the index unless --all is given.

  kb.py init
  kb.py import SLUG LEGACY_PATH --kind K [--title T] [--status S] [--owner O]
  kb.py put SLUG --file PATH --reason TEXT           new version (no-op if bytes are identical)
  kb.py get SLUG[#ANCHOR] [--version N] [--format llm|md]
  kb.py outline SLUG                                 sections with addresses and token estimates
  kb.py card SLUG                                    routing card as JSON
  kb.py index [--all] [--kind K]                     llms.txt-style index of the whole base
  kb.py rows SLUG [--kind table|item|dated] [--table N] [--where COL=VAL] [--limit N]
  kb.py search QUERY [-k 8] [--all]                  sections and records, BM25 ranked
  kb.py list [--kind K] [--status S]
  kb.py history SLUG | diff SLUG A B
  kb.py attach NAME --file PATH --title T --legacy PATH [--mime M] [--reason R]
  kb.py attachments | attachment-out NAME --out PATH
  kb.py verify | links | reindex [SLUG] | export-json [SLUG]
"""
from __future__ import annotations
import sys as _sys
if _sys.version_info < (3, 15):
    raise SystemExit('requires Python 3.15+; run: uv run --no-project --python 3.15 python -I <script>')

import argparse
import collections
import datetime as dt
import difflib
import hashlib
import json
import mimetypes
import re
import sqlite3
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import ctxlib as C  # noqa: E402  (FTS query helper)

DB_PATH = HERE / "knowledge.sqlite3"
KINDS = {"plan", "state", "guide", "tool", "log", "agent", "evidence", "reference", "progress",
         "technical", "index", "data", "archive"}
STATUSES = {"active", "superseded", "archived"}
REF = re.compile(r"kb:([A-Za-z0-9_\-]+)(?:#([^\s)\]`]+))?")  # slugs never contain dots
HEADING = re.compile(r"^(#{1,6})\s+(.*\S)\s*$")
SEP = re.compile(r"^\|?\s*:?-{3,}")
WORD = re.compile(r"[A-Za-zА-Яа-яЁё][A-Za-zА-Яа-яЁё0-9_\-]{3,}")
STOP = set("""the and for with that this from are was were not but can will have has its into when then than
also only more most some such each both over under only after before into onto about within without
это как для что при или его она они был была были быть есть если тоже также только более через
между после перед когда где который которые которая которых чтобы или нет без над под при над
""".split())

SCHEMA = """
CREATE TABLE IF NOT EXISTS doc(
  slug TEXT PRIMARY KEY,
  title TEXT NOT NULL,
  kind TEXT NOT NULL,
  status TEXT NOT NULL DEFAULT 'active',
  owner TEXT NOT NULL DEFAULT '',
  body TEXT NOT NULL,
  sha256 TEXT NOT NULL,
  version INTEGER NOT NULL,
  legacy_path TEXT NOT NULL DEFAULT '',
  updated TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS doc_version(
  id INTEGER PRIMARY KEY,
  slug TEXT NOT NULL REFERENCES doc(slug) ON DELETE CASCADE,
  version INTEGER NOT NULL,
  sha256 TEXT NOT NULL,
  body TEXT NOT NULL,
  created TEXT NOT NULL,
  reason TEXT NOT NULL,
  UNIQUE(slug, version)
);
CREATE TABLE IF NOT EXISTS card(
  slug TEXT PRIMARY KEY REFERENCES doc(slug) ON DELETE CASCADE,
  summary TEXT NOT NULL,
  keywords TEXT NOT NULL,
  sections INTEGER NOT NULL,
  records TEXT NOT NULL,
  tokens INTEGER NOT NULL,
  date_from TEXT NOT NULL,
  date_to TEXT NOT NULL,
  refs TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS section(
  id INTEGER PRIMARY KEY,
  slug TEXT NOT NULL REFERENCES doc(slug) ON DELETE CASCADE,
  ord INTEGER NOT NULL,
  level INTEGER NOT NULL,
  heading TEXT NOT NULL,
  path TEXT NOT NULL,
  anchor TEXT NOT NULL,
  line0 INTEGER NOT NULL,
  line1 INTEGER NOT NULL,
  text TEXT NOT NULL,
  UNIQUE(slug, anchor)
);
CREATE TABLE IF NOT EXISTS trow(
  id INTEGER PRIMARY KEY,
  slug TEXT NOT NULL REFERENCES doc(slug) ON DELETE CASCADE,
  anchor TEXT NOT NULL,
  table_no INTEGER NOT NULL,
  row_no INTEGER NOT NULL,
  line INTEGER NOT NULL,
  data TEXT NOT NULL,
  text TEXT NOT NULL,
  kind TEXT NOT NULL DEFAULT 'table'
);
CREATE TABLE IF NOT EXISTS attachment(
  name TEXT PRIMARY KEY,
  title TEXT NOT NULL,
  mime TEXT NOT NULL,
  size INTEGER NOT NULL,
  sha256 TEXT NOT NULL,
  legacy_path TEXT NOT NULL,
  reason TEXT NOT NULL,
  created TEXT NOT NULL,
  data BLOB NOT NULL
);
CREATE INDEX IF NOT EXISTS section_slug ON section(slug);
CREATE INDEX IF NOT EXISTS trow_slug ON trow(slug, table_no);
CREATE VIRTUAL TABLE IF NOT EXISTS section_fts USING fts5(
  heading, path, text, content='section', content_rowid='id',
  tokenize='porter unicode61 remove_diacritics 2');
CREATE VIRTUAL TABLE IF NOT EXISTS trow_fts USING fts5(
  text, content='trow', content_rowid='id',
  tokenize='porter unicode61 remove_diacritics 2');
CREATE TRIGGER IF NOT EXISTS section_ai AFTER INSERT ON section BEGIN
  INSERT INTO section_fts(rowid, heading, path, text) VALUES (new.id, new.heading, new.path, new.text); END;
CREATE TRIGGER IF NOT EXISTS section_ad AFTER DELETE ON section BEGIN
  INSERT INTO section_fts(section_fts, rowid, heading, path, text) VALUES ('delete', old.id, old.heading, old.path, old.text); END;
CREATE TRIGGER IF NOT EXISTS trow_ai AFTER INSERT ON trow BEGIN
  INSERT INTO trow_fts(rowid, text) VALUES (new.id, new.text); END;
CREATE TRIGGER IF NOT EXISTS trow_ad AFTER DELETE ON trow BEGIN
  INSERT INTO trow_fts(trow_fts, rowid, text) VALUES ('delete', old.id, old.text); END;
"""


def now() -> str:
    return dt.datetime.now(dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def sha(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def sha_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def connect(path: Path = DB_PATH) -> sqlite3.Connection:
    con = sqlite3.connect(path)
    con.row_factory = sqlite3.Row
    con.execute("PRAGMA foreign_keys=ON")
    con.execute("PRAGMA busy_timeout=5000")
    con.execute("PRAGMA journal_mode=WAL")
    return con


def init(path: Path = DB_PATH) -> sqlite3.Connection:
    con = connect(path)
    con.executescript(SCHEMA)
    return con


# ----------------------------------------------------------------- derivation
def _anchor(text: str, used: set[str]) -> str:
    base = re.sub(r"[^\w\- ]", "", text.lower()).strip().replace(" ", "-") or "section"
    anchor, n = base, 2
    while anchor in used:
        anchor = f"{base}-{n}"
        n += 1
    used.add(anchor)
    return anchor


def _cells(line: str) -> list[str]:
    inner = line.strip()
    if inner.startswith("|"):
        inner = inner[1:]
    if inner.endswith("|"):
        inner = inner[:-1]
    return [c.strip() for c in inner.split("|")]


def derive(body: str) -> tuple[list[dict], list[dict]]:
    """Split a Markdown body into sections and records (tables, numbered items, dated entries)."""
    lines = body.splitlines()
    heads: list[tuple[int, int, str]] = []
    in_fence = False
    for i, line in enumerate(lines):
        if line.lstrip().startswith("```"):
            in_fence = not in_fence
            continue
        if in_fence:
            continue
        m = HEADING.match(line)
        if m:
            heads.append((i, len(m.group(1)), m.group(2)))
    starts = [(0, 0, "preamble")] + [(i, lvl, txt) for i, lvl, txt in heads]
    if heads and heads[0][0] == 0:
        starts = starts[1:]
    used: set[str] = set()
    sections: list[dict] = []
    stack: list[tuple[int, str]] = []
    for idx, (start, level, title) in enumerate(starts):
        end = starts[idx + 1][0] if idx + 1 < len(starts) else len(lines)
        if level:
            while stack and stack[-1][0] >= level:
                stack.pop()
            stack.append((level, title))
        path = " > ".join(t for _, t in stack) if level else "(preamble)"
        text = "\n".join(lines[start:end]).strip()
        if not text:
            continue
        sections.append({"ord": len(sections) + 1, "level": level, "heading": title if level else "(preamble)",
                         "path": path, "anchor": _anchor(title if level else "preamble", used),
                         "line0": start + 1, "line1": end, "text": text})

    rows: list[dict] = []
    # Markdown tables
    table_no, current, i = 0, "preamble", 0
    while i < len(lines):
        for s in sections:
            if s["line0"] == i + 1:
                current = s["anchor"]
        line = lines[i]
        if line.strip().startswith("|") and i + 1 < len(lines) and SEP.match(lines[i + 1].strip()):
            table_no += 1
            names: list[str] = []
            for h in _cells(line):
                name = h or f"col{len(names) + 1}"
                while name in names:
                    name += "_"
                names.append(name)
            row_no = 0
            i += 2
            while i < len(lines) and lines[i].strip().startswith("|"):
                cells = _cells(lines[i])
                row_no += 1
                data = {names[k] if k < len(names) else f"extra{k}": (cells[k] if k < len(cells) else "")
                        for k in range(max(len(names), len(cells)))}
                rows.append({"anchor": current, "table_no": table_no, "row_no": row_no, "line": i + 1, "data": data,
                             "text": " | ".join(f"{k}: {v}" for k, v in data.items() if v), "kind": "table"})
                i += 1
            continue
        i += 1

    # numbered items with a bold title, then dated paragraphs
    ITEM = re.compile(r"^\s{0,3}(\d+)\.\s+\*\*(.+?)\*\*\s*(.*)$")
    DATED = re.compile(r"^(\d{4}-\d{2}-\d{2})(?::|\s)\s*(.*)$")
    for mode in ("item", "dated"):
        i, count, current = 0, 0, "preamble"
        while i < len(lines):
            for s in sections:
                if s["line0"] == i + 1:
                    current = s["anchor"]
                    count = 0
            line = lines[i]
            if mode == "item":
                m = ITEM.match(line)
                if m:
                    parts = [m.group(3).strip()]
                    j = i + 1
                    while j < len(lines) and lines[j].strip() and not ITEM.match(lines[j]) \
                            and not HEADING.match(lines[j]) and not lines[j].strip().startswith("|"):
                        parts.append(lines[j].strip())
                        j += 1
                    text = " ".join(x for x in parts if x)
                    count += 1
                    rows.append({"anchor": current, "table_no": 0, "row_no": count, "line": i + 1,
                                 "data": {"n": m.group(1), "title": m.group(2).strip(), "text": text},
                                 "text": f"{m.group(2).strip()}: {text}", "kind": "item"})
                    i = j
                    continue
            else:
                m = DATED.match(line)
                if m and (i == 0 or not lines[i - 1].strip()):
                    parts = [m.group(2).strip()]
                    j = i + 1
                    while j < len(lines) and lines[j].strip() and not HEADING.match(lines[j]) \
                            and not DATED.match(lines[j]):
                        parts.append(lines[j].strip())
                        j += 1
                    text = " ".join(x for x in parts if x)
                    count += 1
                    rows.append({"anchor": current, "table_no": 0, "row_no": count, "line": i + 1,
                                 "data": {"date": m.group(1), "text": text}, "text": f"{m.group(1)}: {text}",
                                 "kind": "dated"})
                    i = j
                    continue
            i += 1
    return sections, rows


def _card(slug: str, body: str, sections: list[dict], rows: list[dict]) -> dict:
    """Routing card: enough for an LLM to decide whether to open the document."""
    summary = ""
    for block in re.split(r"\n\s*\n", body):
        text = " ".join(x.strip() for x in block.splitlines()
                        if x.strip() and not x.lstrip().startswith(("#", "|", "```", "<!--", "- ", "* ")))
        text = re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", text)
        text = re.sub(r"[`*_>]", "", text).strip()
        if len(text) >= 40:
            summary = text[:360]
            break
    counts = collections.Counter(r["kind"] for r in rows)
    freq = collections.Counter(w.lower() for w in WORD.findall(body) if w.lower() not in STOP)
    keywords = [w for w, n in freq.most_common(40) if n >= 2][:15]
    dates = sorted(r["data"]["date"] for r in rows if r["kind"] == "dated")
    refs = sorted({f"{s}#{a}" if a else s for s, a in REF.findall(body)})
    return {"summary": summary, "keywords": keywords, "sections": len(sections),
            "records": {k: counts.get(k, 0) for k in ("table", "item", "dated")},
            "tokens": len(body) // 3 + 1, "date_from": dates[0] if dates else "",
            "date_to": dates[-1] if dates else "", "refs": refs}


def _rebuild(con: sqlite3.Connection, slug: str, body: str) -> tuple[int, int]:
    con.execute("DELETE FROM trow WHERE slug=?", (slug,))
    con.execute("DELETE FROM section WHERE slug=?", (slug,))
    sections, rows = derive(body)
    for s in sections:
        con.execute("INSERT INTO section(slug,ord,level,heading,path,anchor,line0,line1,text)"
                    " VALUES (?,?,?,?,?,?,?,?,?)",
                    (slug, s["ord"], s["level"], s["heading"], s["path"], s["anchor"], s["line0"], s["line1"], s["text"]))
    for r in rows:
        con.execute("INSERT INTO trow(slug,anchor,table_no,row_no,line,data,text,kind) VALUES (?,?,?,?,?,?,?,?)",
                    (slug, r["anchor"], r["table_no"], r["row_no"], r["line"],
                     json.dumps(r["data"], ensure_ascii=False), r["text"], r["kind"]))
    c = _card(slug, body, sections, rows)
    con.execute("INSERT OR REPLACE INTO card(slug,summary,keywords,sections,records,tokens,date_from,date_to,refs)"
                " VALUES (?,?,?,?,?,?,?,?,?)",
                (slug, c["summary"], json.dumps(c["keywords"], ensure_ascii=False), c["sections"],
                 json.dumps(c["records"]), c["tokens"], c["date_from"], c["date_to"], json.dumps(c["refs"])))
    return len(sections), len(rows)


# ------------------------------------------------------------------ operations
def put(con: sqlite3.Connection, slug: str, body: str, *, reason: str, kind: str | None = None,
        title: str | None = None, status: str | None = None, owner: str | None = None,
        legacy_path: str | None = None) -> str:
    digest = sha(body)
    row = con.execute("SELECT * FROM doc WHERE slug=?", (slug,)).fetchone()
    with con:
        if row is None:
            if kind not in KINDS:
                raise SystemExit(f"kind must be one of {sorted(KINDS)}")
            if status and status not in STATUSES:
                raise SystemExit(f"status must be one of {sorted(STATUSES)}")
            heading = next((m.group(2) for m in map(HEADING.match, body.splitlines()) if m and len(m.group(1)) == 1), slug)
            con.execute("INSERT INTO doc(slug,title,kind,status,owner,body,sha256,version,legacy_path,updated)"
                        " VALUES (?,?,?,?,?,?,?,?,?,?)",
                        (slug, title or heading, kind, status or "active", owner or "", body, digest, 1,
                         legacy_path or "", now()))
            con.execute("INSERT INTO doc_version(slug,version,sha256,body,created,reason) VALUES (?,?,?,?,?,?)",
                        (slug, 1, digest, body, now(), reason))
            _rebuild(con, slug, body)
            return "created"
        if row["sha256"] == digest:
            return "unchanged"
        version = row["version"] + 1
        con.execute("INSERT INTO doc_version(slug,version,sha256,body,created,reason) VALUES (?,?,?,?,?,?)",
                    (slug, version, digest, body, now(), reason))
        con.execute("UPDATE doc SET body=?, sha256=?, version=?, updated=?, title=COALESCE(?,title),"
                    " kind=COALESCE(?,kind), status=COALESCE(?,status), owner=COALESCE(?,owner),"
                    " legacy_path=COALESCE(NULLIF(?,''),legacy_path) WHERE slug=?",
                    (body, digest, version, now(), title, kind, status, owner, legacy_path, slug))
        _rebuild(con, slug, body)
        return "updated"


def _doc(con, slug):
    row = con.execute("SELECT * FROM doc WHERE slug=?", (slug,)).fetchone()
    if row is None:
        raise SystemExit(f"no document {slug!r}; run `kb.py index --all`")
    return row


def _section(con, slug, anchor):
    row = con.execute("SELECT * FROM section WHERE slug=? AND anchor=?", (slug, anchor)).fetchone()
    if row is None:
        known = [r["anchor"] for r in con.execute("SELECT anchor FROM section WHERE slug=? ORDER BY ord", (slug,))]
        raise SystemExit(f"no section {slug}#{anchor}; known: {', '.join(known[:40])}")
    return row


def get(con, target: str, version: int | None = None, fmt: str = "llm") -> str:
    slug, _, anchor = target.partition("#")
    row = _doc(con, slug)
    if version is not None:
        if anchor:
            raise SystemExit("--version with a section address is not supported; use `history` and `diff`")
        found = con.execute("SELECT body FROM doc_version WHERE slug=? AND version=?", (slug, version)).fetchone()
        if found is None:
            raise SystemExit(f"{slug} has no version {version}")
        return found["body"]
    if not anchor:
        if fmt == "md":
            return row["body"]
        s = con.execute("SELECT path, anchor, line0, line1, text FROM section WHERE slug=? ORDER BY ord",
                        (slug,)).fetchall()
        head = (f"# kb:{slug}  kind={row['kind']} status={row['status']} owner={row['owner'] or '-'} "
                f"v{row['version']} updated={row['updated']}\n")
        return head + "\n\n".join(f"<!-- kb:{slug}#{r['anchor']} lines {r['line0']}-{r['line1']} -->\n{r['text']}"
                                  for r in s)
    s = _section(con, slug, anchor)
    if fmt == "md":
        return s["text"]
    return f"<!-- kb:{slug}#{anchor}  path: {s['path']}  lines {s['line0']}-{s['line1']} -->\n{s['text']}"


def outline(con, slug: str) -> str:
    _doc(con, slug)
    out = []
    for r in con.execute("SELECT anchor, level, path, line0, line1, length(text) AS n FROM section"
                         " WHERE slug=? ORDER BY ord", (slug,)):
        out.append(f"kb:{slug}#{r['anchor']:<48} L{r['line0']}-{r['line1']:<5} ~{r['n'] // 3:>6} tok  {r['path']}")
    tables = con.execute("SELECT anchor, table_no, kind, count(*) AS n FROM trow WHERE slug=?"
                         " GROUP BY anchor, table_no, kind ORDER BY anchor, table_no", (slug,)).fetchall()
    if tables:
        out.append("")
        out.extend(f"records: kb:{slug}#{t['anchor']} {t['kind']} t{t['table_no']} n={t['n']}" for t in tables)
    return "\n".join(out)


def card(con, slug: str) -> dict:
    _doc(con, slug)
    c = con.execute("SELECT * FROM card WHERE slug=?", (slug,)).fetchone()
    d = con.execute("SELECT slug, title, kind, status, version, updated FROM doc WHERE slug=?", (slug,)).fetchone()
    return {"slug": slug, "title": d["title"], "kind": d["kind"], "status": d["status"], "version": d["version"],
            "updated": d["updated"], "summary": c["summary"], "keywords": json.loads(c["keywords"]),
            "sections": c["sections"], "records": json.loads(c["records"]), "tokens": c["tokens"],
            "date_from": c["date_from"], "date_to": c["date_to"], "refs": json.loads(c["refs"]),
            "address": f"kb:{slug}"}


def index_text(con, include_archived: bool = False, kind: str | None = None) -> str:
    """llms.txt-style index: one line per document with its routing card."""
    q = "SELECT d.slug, d.title, d.kind, d.status, d.version, c.summary, c.keywords, c.tokens, c.sections, c.records" \
        " FROM doc d JOIN card c ON c.slug = d.slug WHERE 1=1"
    args: list = []
    if not include_archived:
        q += " AND d.status <> 'archived'"
    if kind:
        q += " AND d.kind = ?"
        args.append(kind)
    rows = con.execute(q + " ORDER BY d.kind, d.slug", args).fetchall()
    total = con.execute("SELECT count(*) FROM doc").fetchone()[0]
    archived = con.execute("SELECT count(*) FROM doc WHERE status='archived'").fetchone()[0]
    out = ["# ACV project knowledge base",
           f"> {len(rows)} documents shown of {total} ({archived} archived, excluded unless --all). "
           "Read a section: `python3 tools/acv-context/kb.py get SLUG#ANCHOR`; search: `kb.py search \"query\"`.", ""]
    current = None
    for r in rows:
        if r["kind"] != current:
            current = r["kind"]
            out += [f"## {current}", ""]
        keywords = ", ".join(json.loads(r["keywords"])[:8])
        records = json.loads(r["records"])
        rec = ", ".join(f"{k} {v}" for k, v in records.items() if v)
        out.append(f"- [{r['slug']}](kb:{r['slug']}) — {r['title']} [{r['status']}, v{r['version']}, "
                   f"~{r['tokens']} tok, {r['sections']} sections{', ' + rec if rec else ''}]")
        if r["summary"]:
            out.append(f"  {r['summary'][:300]}")
        if keywords:
            out.append(f"  keywords: {keywords}")
    return "\n".join(out)


def rows(con, slug: str, table: int | None = None, where: str | None = None,
         kind: str | None = None, limit: int | None = None) -> list[str]:
    _doc(con, slug)
    q = "SELECT anchor, table_no, row_no, kind, data FROM trow WHERE slug=?"
    args: list = [slug]
    if table is not None:
        q += " AND table_no=?"
        args.append(table)
    if kind:
        q += " AND kind=?"
        args.append(kind)
    q += " ORDER BY id"
    key = val = None
    if where:
        key, _, val = where.partition("=")
    out = []
    for r in con.execute(q, args):
        data = json.loads(r["data"])
        if key is not None and data.get(key) != val:
            continue
        out.append(json.dumps({"ref": f"kb:{slug}#{r['anchor']}/t{r['table_no']}/r{r['row_no']}",
                               "kind": r["kind"], **data}, ensure_ascii=False))
        if limit and len(out) >= limit:
            break
    return out


def search(con, query: str, k: int = 8, include_archived: bool = False) -> list[dict]:
    match = C.Index.match_query(query)
    if not match:
        return []
    status_filter = "" if include_archived else " AND d.status <> 'archived'"
    hits = []
    for r in con.execute(
            "SELECT s.slug, s.anchor, s.path, s.line0, s.line1, s.text, bm25(section_fts, 8.0, 3.0, 1.0) AS score"
            " FROM section_fts JOIN section s ON s.id = section_fts.rowid JOIN doc d ON d.slug = s.slug"
            f" WHERE section_fts MATCH ?{status_filter} ORDER BY score LIMIT ?", (match, k)):
        hits.append({"ref": f"kb:{r['slug']}#{r['anchor']}", "where": f"{r['path']} (L{r['line0']}-{r['line1']})",
                     "text": r["text"], "score": -r["score"]})
    for r in con.execute(
            "SELECT t.slug, t.anchor, t.table_no, t.row_no, t.text, bm25(trow_fts) AS score"
            " FROM trow_fts JOIN trow t ON t.id = trow_fts.rowid JOIN doc d ON d.slug = t.slug"
            f" WHERE trow_fts MATCH ?{status_filter} ORDER BY score LIMIT ?", (match, k)):
        hits.append({"ref": f"kb:{r['slug']}#{r['anchor']}/t{r['table_no']}/r{r['row_no']}",
                     "where": "record", "text": r["text"], "score": -r["score"]})
    hits.sort(key=lambda h: -h["score"])
    return hits[:k]


def verify(con) -> list[str]:
    problems = []
    for row in con.execute("SELECT * FROM doc"):
        slug = row["slug"]
        if sha(row["body"]) != row["sha256"]:
            problems.append(f"{slug}: body does not match stored sha256")
        versions = con.execute("SELECT * FROM doc_version WHERE slug=? ORDER BY version", (slug,)).fetchall()
        if not versions or versions[-1]["version"] != row["version"]:
            problems.append(f"{slug}: latest version record missing")
            continue
        if versions[-1]["sha256"] != row["sha256"]:
            problems.append(f"{slug}: latest version sha differs from document")
        for v in versions:
            if sha(v["body"]) != v["sha256"]:
                problems.append(f"{slug} v{v['version']}: stored bytes do not match their sha256")
        numbers = [v["version"] for v in versions]
        if numbers != list(range(1, len(numbers) + 1)):
            problems.append(f"{slug}: version numbers are not contiguous {numbers}")
        sections, table_rows = derive(row["body"])
        have_s = con.execute("SELECT count(*) FROM section WHERE slug=?", (slug,)).fetchone()[0]
        have_r = con.execute("SELECT count(*) FROM trow WHERE slug=?", (slug,)).fetchone()[0]
        if (have_s, have_r) != (len(sections), len(table_rows)):
            problems.append(f"{slug}: derived layers stale (sections {have_s}/{len(sections)}, "
                            f"rows {have_r}/{len(table_rows)}); run kb.py reindex {slug}")
        if con.execute("SELECT count(*) FROM card WHERE slug=?", (slug,)).fetchone()[0] != 1:
            problems.append(f"{slug}: routing card missing; run kb.py reindex {slug}")
    for row in con.execute("SELECT name, sha256, size, data FROM attachment"):
        if sha_bytes(row["data"]) != row["sha256"] or len(row["data"]) != row["size"]:
            problems.append(f"attachment {row['name']}: bytes do not match sha256/size")
    return problems


def links(con) -> list[tuple[str, str, bool]]:
    docs = {r["slug"] for r in con.execute("SELECT slug FROM doc")}
    anchors = {(r["slug"], r["anchor"]) for r in con.execute("SELECT slug, anchor FROM section")}
    out = []
    for row in con.execute("SELECT slug, body FROM doc ORDER BY slug"):
        for slug, anchor in sorted(set(REF.findall(row["body"]))):
            ok = slug in docs and (not anchor or (slug, anchor.split("/")[0]) in anchors)
            out.append((row["slug"], f"{slug}#{anchor}" if anchor else slug, ok))
    return out


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--db", type=Path, default=DB_PATH)
    sub = p.add_subparsers(dest="cmd", required=True)
    sub.add_parser("init")
    s = sub.add_parser("import"); s.add_argument("slug"); s.add_argument("legacy_path", type=Path)
    s.add_argument("--kind", required=True); s.add_argument("--title"); s.add_argument("--status"); s.add_argument("--owner")
    s = sub.add_parser("put"); s.add_argument("slug"); s.add_argument("--file", required=True, type=Path)
    s.add_argument("--reason", required=True); s.add_argument("--status"); s.add_argument("--owner")
    s = sub.add_parser("get"); s.add_argument("target"); s.add_argument("--version", type=int)
    s.add_argument("--format", choices=["llm", "md"], default="llm")
    s = sub.add_parser("outline"); s.add_argument("slug")
    s = sub.add_parser("card"); s.add_argument("slug")
    s = sub.add_parser("index"); s.add_argument("--all", action="store_true"); s.add_argument("--kind")
    s = sub.add_parser("rows"); s.add_argument("slug"); s.add_argument("--table", type=int)
    s.add_argument("--where"); s.add_argument("--kind", choices=["table", "item", "dated"]); s.add_argument("--limit", type=int)
    s = sub.add_parser("search"); s.add_argument("query"); s.add_argument("-k", type=int, default=8)
    s.add_argument("--all", action="store_true")
    s = sub.add_parser("list"); s.add_argument("--kind"); s.add_argument("--status")
    s = sub.add_parser("history"); s.add_argument("slug")
    s = sub.add_parser("diff"); s.add_argument("slug"); s.add_argument("a", type=int); s.add_argument("b", type=int)
    s = sub.add_parser("attach"); s.add_argument("name"); s.add_argument("--file", required=True, type=Path)
    s.add_argument("--title", required=True); s.add_argument("--legacy", required=True)
    s.add_argument("--mime"); s.add_argument("--reason", required=True)
    sub.add_parser("attachments")
    s = sub.add_parser("attachment-out"); s.add_argument("name"); s.add_argument("--out", required=True, type=Path)
    sub.add_parser("verify")
    sub.add_parser("links")
    s = sub.add_parser("reindex"); s.add_argument("slug", nargs="?")
    s = sub.add_parser("export-json"); s.add_argument("slug", nargs="?")
    return p


def main(argv=None) -> int:
    a = build_parser().parse_args(argv)
    if a.cmd == "init":
        init(a.db).close()
        print(f"initialized {a.db}")
        return 0
    writes = {"import", "put", "reindex", "attach"}
    con = init(a.db) if a.cmd in writes else connect(a.db)
    if a.cmd == "import":
        body = a.legacy_path.read_bytes().decode("utf-8")
        result = put(con, a.slug, body, reason=f"import from {a.legacy_path.as_posix()}", kind=a.kind,
                     title=a.title, status=a.status, owner=a.owner, legacy_path=a.legacy_path.as_posix())
        print(f"{a.slug}: {result} sha256={sha(body)[:12]}")
    elif a.cmd == "put":
        body = a.file.read_bytes().decode("utf-8")
        print(f"{a.slug}: {put(con, a.slug, body, reason=a.reason, status=a.status, owner=a.owner)}")
    elif a.cmd == "attach":
        data = a.file.read_bytes()
        mime = a.mime or mimetypes.guess_type(a.file.name)[0] or "application/octet-stream"
        with con:
            con.execute("INSERT OR REPLACE INTO attachment(name,title,mime,size,sha256,legacy_path,reason,created,data)"
                        " VALUES (?,?,?,?,?,?,?,?,?)",
                        (a.name, a.title, mime, len(data), sha_bytes(data), a.legacy, a.reason, now(), sqlite3.Binary(data)))
        print(f"{a.name}: stored {len(data)} bytes sha256={sha_bytes(data)[:12]}")
    elif a.cmd == "attachments":
        for r in con.execute("SELECT name, mime, size, sha256, legacy_path, title FROM attachment ORDER BY name"):
            print(f"{r['name']:<36} {r['mime']:<24} {r['size']:>10} B  {r['sha256'][:12]}  {r['title']}  <- {r['legacy_path']}")
    elif a.cmd == "attachment-out":
        r = con.execute("SELECT data, sha256 FROM attachment WHERE name=?", (a.name,)).fetchone()
        if r is None:
            raise SystemExit(f"no attachment {a.name!r}")
        if sha_bytes(r["data"]) != r["sha256"]:
            raise SystemExit("stored bytes do not match sha256")
        a.out.write_bytes(r["data"])
        print(f"wrote {a.out} ({len(r['data'])} bytes)")
    elif a.cmd == "get":
        print(get(con, a.target, a.version, a.format))
    elif a.cmd == "outline":
        print(outline(con, a.slug))
    elif a.cmd == "card":
        print(json.dumps(card(con, a.slug), ensure_ascii=False, indent=2))
    elif a.cmd == "index":
        print(index_text(con, a.all, a.kind))
    elif a.cmd == "rows":
        print("\n".join(rows(con, a.slug, a.table, a.where, a.kind, a.limit)))
    elif a.cmd == "search":
        hits = search(con, a.query, a.k, a.all)
        if not hits:
            print("no matches")
        for h in hits:
            print(f"### {h['ref']}  [{h['where']}]\n{h['text'][:800]}\n")
    elif a.cmd == "list":
        q = "SELECT slug, kind, status, version, updated, title FROM doc WHERE 1=1"
        args = []
        if a.kind:
            q += " AND kind=?"; args.append(a.kind)
        if a.status:
            q += " AND status=?"; args.append(a.status)
        for r in con.execute(q + " ORDER BY kind, slug", args):
            print(f"{r['slug']:<30} {r['kind']:<10} {r['status']:<9} v{r['version']:<3} {r['updated']}  {r['title']}")
    elif a.cmd == "history":
        _doc(con, a.slug)
        for r in con.execute("SELECT version, sha256, created, reason, length(body) AS n FROM doc_version"
                             " WHERE slug=? ORDER BY version", (a.slug,)):
            print(f"v{r['version']:<3} {r['sha256'][:12]}  {r['created']}  {r['n']:>7} bytes  {r['reason']}")
    elif a.cmd == "diff":
        old = get(con, a.slug, a.a, "md").splitlines(keepends=True)
        new = get(con, a.slug, a.b, "md").splitlines(keepends=True)
        sys.stdout.writelines(difflib.unified_diff(old, new, f"{a.slug} v{a.a}", f"{a.slug} v{a.b}"))
    elif a.cmd == "verify":
        problems = verify(con)
        docs = con.execute("SELECT count(*) FROM doc").fetchone()[0]
        versions = con.execute("SELECT count(*) FROM doc_version").fetchone()[0]
        secs = con.execute("SELECT count(*) FROM section").fetchone()[0]
        trs = con.execute("SELECT count(*) FROM trow").fetchone()[0]
        atts = con.execute("SELECT count(*) FROM attachment").fetchone()[0]
        print(f"documents={docs} versions={versions} sections={secs} records={trs} attachments={atts} "
              f"problems={len(problems)}")
        for p in problems:
            print("  " + p)
        return 1 if problems else 0
    elif a.cmd == "links":
        bad = 0
        for src, ref, ok in links(con):
            bad += 0 if ok else 1
            print(f"{'ok     ' if ok else 'MISSING'} {src} -> kb:{ref}")
        print(f"unresolved={bad}")
        return 1 if bad else 0
    elif a.cmd == "reindex":
        slugs = [a.slug] if a.slug else [r["slug"] for r in con.execute("SELECT slug FROM doc")]
        with con:
            for slug in slugs:
                row = _doc(con, slug)
                s, r = _rebuild(con, slug, row["body"])
                print(f"{slug}: sections={s} records={r}")
    elif a.cmd == "export-json":
        q = "SELECT slug FROM doc" + (" WHERE slug=?" if a.slug else "") + " ORDER BY slug"
        for slug_row in con.execute(q, (a.slug,) if a.slug else ()).fetchall():
            slug = slug_row["slug"]
            d = con.execute("SELECT * FROM doc WHERE slug=?", (slug,)).fetchone()
            print(json.dumps({"type": "doc", "slug": slug, "kind": d["kind"], "status": d["status"],
                              "version": d["version"], "sha256": d["sha256"], "card": card(con, slug)},
                             ensure_ascii=False))
            for s in con.execute("SELECT anchor, path, line0, line1, text FROM section WHERE slug=? ORDER BY ord", (slug,)):
                print(json.dumps({"type": "section", "slug": slug, **dict(s)}, ensure_ascii=False))
            for t in con.execute("SELECT anchor, table_no, row_no, kind, data FROM trow WHERE slug=? ORDER BY id", (slug,)):
                print(json.dumps({"type": "record", "slug": slug, "kind": t["kind"], "anchor": t["anchor"],
                                  "table": t["table_no"], "row": t["row_no"], "data": json.loads(t["data"])},
                                 ensure_ascii=False))
    con.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
