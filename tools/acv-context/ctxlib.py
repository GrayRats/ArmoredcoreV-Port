"""Local context retrieval for large Markdown/log corpora (SQLite FTS5, stdlib only).

Pieces:
  * Index    - heading-aware chunks of Markdown in an FTS5 table (BM25, incremental by mtime/size).
  * sanitize - log digest: strip volatile fields, collapse repeats, rank errors first, fit a budget.
  * tree     - compact metadata tree (no file contents), honoring .llmignore.
  * assemble - context pack with hard per-block token caps (architecture / code / logs).

Token counts are an estimate (characters / 3), not a tokenizer result.
"""
from __future__ import annotations
import sys as _sys
if _sys.version_info < (3, 15):
    raise SystemExit('requires Python 3.15+; run: uv run --no-project --python 3.15 python -I <script>')

import collections
import hashlib
import json
import os
import re
import sqlite3
import subprocess
import tempfile
from pathlib import Path

CHARS_PER_TOKEN = 3.0
DEFAULT_SPLIT = {"architecture": 0.15, "code": 0.60, "logs": 0.25}
SKIP_DIRS = {".git", "node_modules", "__pycache__", ".cache"}
DOC_SUFFIXES = {".md"}
STOP = {"the", "and", "for", "что", "как", "это", "для", "при", "или", "was", "are", "with", "from", "that"}


def est_tokens(text: str) -> int:
    return int(len(text) / CHARS_PER_TOKEN) + 1


def clip(text: str, tokens: int, marker: str = "\n[... cut to fit budget ...]") -> str:
    """Cut to at most `tokens` estimated tokens, marker included, so caps are hard limits."""
    limit = max(0, int(tokens * CHARS_PER_TOKEN))
    if len(text) <= limit:
        return text
    keep = max(0, limit - len(marker))
    cut = text[:keep]
    nl = cut.rfind("\n")
    if nl > keep * 0.6:
        cut = cut[:nl]
    return cut + marker


PLUGIN_DIR = Path(__file__).resolve().parent


def project_root() -> Path:
    return Path(os.environ.get("CLAUDE_PROJECT_DIR") or os.getcwd()).resolve()


def db_path(root: Path) -> Path:
    override = os.environ.get("ACV_CTX_DB")
    if override:
        return Path(override)
    data = os.environ.get("CLAUDE_PLUGIN_DATA")
    if data:
        return Path(data) / f"index-{hashlib.sha1(str(root).encode()).hexdigest()[:10]}.sqlite"
    return root / "tmp" / "acv-context" / "index.sqlite"


# ---------------------------------------------------------------- ignore rules
class Ignore:
    """`.llmignore` evaluated by git in a scratch repo (gitignore semantics, repo .gitignore not applied)."""

    def __init__(self, root: Path):
        self.root = root
        rules = root / ".llmignore"
        self.text = rules.read_text(encoding="utf-8") if rules.is_file() else ""

    def filter(self, rel_paths: list[str]) -> set[str]:
        """Return the subset of rel_paths NOT ignored."""
        if not self.text or not rel_paths:
            return set(rel_paths)
        env = dict(os.environ, GIT_CONFIG_GLOBAL="/dev/null", GIT_CONFIG_SYSTEM="/dev/null")
        with tempfile.TemporaryDirectory(prefix="ctx-ign-") as tmp:
            scratch = Path(tmp)
            subprocess.run(["git", "init", "-q"], cwd=scratch, check=True, env=env)
            (scratch / ".gitignore").write_text(self.text, encoding="utf-8")
            for rel in rel_paths:
                target = scratch / rel
                target.parent.mkdir(parents=True, exist_ok=True)
                target.touch()
            out = subprocess.run(["git", "ls-files", "-o", "--exclude-standard", "-z"], cwd=scratch,
                                 capture_output=True, text=True, env=env, check=True)
        wanted = set(rel_paths)
        return {p for p in out.stdout.split("\0") if p in wanted}


def iter_files(root: Path, suffixes: set[str], ignore: Ignore) -> list[str]:
    """Relative paths of files with the given suffixes, pruning ignored top-level trees early."""
    top = sorted(p.name for p in root.iterdir() if p.name not in SKIP_DIRS)
    keep_top = ignore.filter([f"{n}/__probe__" if (root / n).is_dir() else n for n in top])
    allowed_top = {p.split("/")[0] for p in keep_top}
    found: list[str] = []
    for name in sorted(allowed_top):
        base = root / name
        if base.is_file():
            if base.suffix.lower() in suffixes:
                found.append(name)
            continue
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames[:] = sorted(d for d in dirnames if d not in SKIP_DIRS)
            for f in filenames:
                if Path(f).suffix.lower() in suffixes:
                    found.append((Path(dirpath) / f).relative_to(root).as_posix())
    return sorted(ignore.filter(found))


# --------------------------------------------------------------------- chunking
HEADING = re.compile(r"^(#{1,6})\s+(.*\S)\s*$")


def chunk_markdown(text: str, max_chars: int = 1400) -> list[tuple[str, int, int, str]]:
    """(heading_path, first_line, last_line, body) chunks; splits long sections on blank lines."""
    lines = text.splitlines()
    chunks: list[tuple[str, int, int, str]] = []
    stack: list[tuple[int, str]] = []
    buf: list[str] = []
    start = 1
    in_fence = False

    def flush(end: int):
        body = "\n".join(buf).strip()
        if body:
            path = " > ".join(h for _, h in stack)
            if len(body) <= max_chars:
                chunks.append((path, start, end, body))
            else:
                part: list[str] = []
                first = start
                size = 0
                for offset, line in enumerate(buf):
                    part.append(line)
                    size += len(line) + 1
                    if size >= max_chars and not line.strip():
                        chunks.append((path, first, start + offset, "\n".join(part).strip()))
                        part, size, first = [], 0, start + offset + 1
                tail = "\n".join(part).strip()
                if tail:
                    chunks.append((path, first, end, tail))
        buf.clear()

    for number, line in enumerate(lines, 1):
        if line.lstrip().startswith("```"):
            in_fence = not in_fence
        m = None if in_fence else HEADING.match(line)
        if m:
            flush(number - 1)
            level = len(m.group(1))
            while stack and stack[-1][0] >= level:
                stack.pop()
            stack.append((level, m.group(2)))
            start = number
        buf.append(line)
    flush(len(lines))
    return chunks


# ------------------------------------------------------------------------ index
SCHEMA = """
CREATE TABLE IF NOT EXISTS docs(path TEXT PRIMARY KEY, mtime_ns INTEGER, size INTEGER);
CREATE TABLE IF NOT EXISTS chunks(id INTEGER PRIMARY KEY, path TEXT NOT NULL, heading TEXT, l0 INTEGER, l1 INTEGER, body TEXT);
CREATE INDEX IF NOT EXISTS chunks_path ON chunks(path);
CREATE VIRTUAL TABLE IF NOT EXISTS fts USING fts5(
  title, body, content='chunks', content_rowid='id', tokenize='porter unicode61 remove_diacritics 2');
CREATE TRIGGER IF NOT EXISTS chunks_ai AFTER INSERT ON chunks BEGIN
  INSERT INTO fts(rowid, title, body) VALUES (new.id, new.path || ' ' || coalesce(new.heading,''), new.body); END;
CREATE TRIGGER IF NOT EXISTS chunks_ad AFTER DELETE ON chunks BEGIN
  INSERT INTO fts(fts, rowid, title, body) VALUES ('delete', old.id, old.path || ' ' || coalesce(old.heading,''), old.body); END;
"""


class Index:
    def __init__(self, root: Path, db: Path | None = None):
        self.root = root
        self.db_file = db or db_path(root)
        self.db_file.parent.mkdir(parents=True, exist_ok=True)
        self.con = sqlite3.connect(self.db_file)
        self.con.executescript(SCHEMA)

    def refresh(self, full: bool = False) -> dict:
        ignore = Ignore(self.root)
        files = iter_files(self.root, DOC_SUFFIXES, ignore)
        where = {rel: self.root / rel for rel in files}
        known = {r[0]: (r[1], r[2]) for r in self.con.execute("SELECT path, mtime_ns, size FROM docs")}
        stats = {"files": len(files), "added": 0, "updated": 0, "removed": 0, "chunks": 0}
        with self.con:
            for gone in set(known) - set(files):
                self._drop(gone)
                stats["removed"] += 1
            for rel in files:
                st = where[rel].stat()
                if not full and known.get(rel) == (st.st_mtime_ns, st.st_size):
                    continue
                stats["updated" if rel in known else "added"] += 1
                self._drop(rel)
                text = where[rel].read_text(encoding="utf-8", errors="replace")
                for heading, l0, l1, body in chunk_markdown(text):
                    self.con.execute("INSERT INTO chunks(path, heading, l0, l1, body) VALUES (?,?,?,?,?)",
                                     (rel, heading, l0, l1, body))
                self.con.execute("INSERT OR REPLACE INTO docs VALUES (?,?,?)", (rel, st.st_mtime_ns, st.st_size))
        stats["chunks"] = self.con.execute("SELECT count(*) FROM chunks").fetchone()[0]
        return stats

    def _drop(self, rel: str):
        self.con.execute("DELETE FROM chunks WHERE path=?", (rel,))
        self.con.execute("DELETE FROM docs WHERE path=?", (rel,))

    @staticmethod
    def match_query(query: str) -> str:
        words = []
        for w in re.findall(r"\w+", query.lower(), re.UNICODE):
            if len(w) < 2 or w in STOP:
                continue
            if not w.isascii() and len(w) > 5:   # crude Russian stemming: cut inflection tail
                w = w[:-2]
            words.append('"%s"*' % w.replace('"', ""))
        return " OR ".join(dict.fromkeys(words))

    def search(self, query: str, k: int = 5, path_prefix: str = "", max_chars: int = 1000) -> list[dict]:
        q = self.match_query(query)
        if not q:
            return []
        sql = ("SELECT c.path, c.heading, c.l0, c.l1, c.body, bm25(fts, 6.0, 1.0) AS score "
               "FROM fts JOIN chunks c ON c.id = fts.rowid WHERE fts MATCH ? ")
        args: list = [q]
        if path_prefix:
            sql += "AND c.path LIKE ? "
            args.append(path_prefix.rstrip("%") + "%")
        sql += "ORDER BY score LIMIT ?"
        args.append(k * 4)
        hits, seen = [], set()
        for path, heading, l0, l1, body, score in self.con.execute(sql, args):
            key = (path, l0)
            if key in seen:
                continue
            seen.add(key)
            hits.append({"path": path, "heading": heading, "lines": f"{l0}-{l1}", "score": round(-score, 3),
                         "text": body if len(body) <= max_chars else body[:max_chars].rsplit("\n", 1)[0] + "\n[...]"})
            if len(hits) == k:
                break
        return hits


def format_hits(hits: list[dict], budget: int | None = None) -> str:
    out, used = [], 0
    for h in hits:
        block = f"### {h['path']}:{h['lines']}  {h['heading']}\n{h['text']}\n"
        cost = est_tokens(block)
        if budget is not None and used + cost > budget:
            room = budget - used
            if room < 80:
                break
            block = clip(block, room)
            cost = room
        out.append(block)
        used += cost
    return "\n".join(out)


# -------------------------------------------------------------------- log digest
ANSI = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")
TS = re.compile(r"^\[?\d{4}-\d\d-\d\d[ T]\d\d:\d\d:\d\d(?:[.,]\d+)?(?:Z|[+-]\d\d:?\d\d)?\]?\s*")
TID = re.compile(r"\[t\d+\]\s*")
HEX = re.compile(r"\b(?:0x)?[0-9a-fA-F]{8,}\b")
NUM = re.compile(r"\d+(?:\.\d+)?")
SEVERE = re.compile(r"\b(fatal|critical|panic|segfault|assert|abort|exception|crash|violation|sigsegv|sigabrt)", re.I)
ERROR = re.compile(r"\[error\]|\berror\b|\bfailed\b|\bfailure\b|\bdenied\b|\btimeout\b|\bnot found\b|\binvalid\b", re.I)
WARN = re.compile(r"\[warn(?:ing)?\]|\bwarn(?:ing)?\b|\bstub\b|\bunsupported\b|\bdeprecated\b", re.I)


LEVEL_TAG = re.compile(r"\[(trace|debug|info|warn(?:ing)?|error|err|critical|fatal)\]", re.I)


def _severity(line: str) -> int:
    """3 fatal, 2 error, 1 warning, 0 info. An explicit level tag wins over keywords in the text."""
    tag = LEVEL_TAG.search(line)
    if tag:
        level = tag.group(1).lower()
        if level in ("critical", "fatal"):
            return 3
        if level in ("error", "err"):
            return 3 if SEVERE.search(line) else 2
        if level.startswith("warn"):
            return 1
        return 1 if (SEVERE.search(line) or ERROR.search(line)) else 0
    return 3 if SEVERE.search(line) else 2 if ERROR.search(line) else 1 if WARN.search(line) else 0


def _norm_key(line: str) -> str:
    return NUM.sub("#", HEX.sub("H", line))


def digest_log(text: str, budget_tokens: int, tail: int = 8, head: int = 3, max_line: int = 220) -> dict:
    """Compress a log: strip timestamps/thread ids, collapse repeats (xN), errors first, fit budget."""
    raw_lines = [ANSI.sub("", l).rstrip() for l in text.splitlines() if l.strip()]
    groups: dict[str, dict] = {}
    order: list[str] = []
    for index, line in enumerate(raw_lines):
        clean = TID.sub("", TS.sub("", line, count=1)).strip()
        key = _norm_key(clean)
        g = groups.get(key)
        if g is None:
            sev = _severity(clean)
            g = groups[key] = {"line": clean[:max_line], "n": 0, "sev": sev, "first": index, "last": index}
            order.append(key)
        g["n"] += 1
        g["last"] = index
    head_keys = {_norm_key(TID.sub("", TS.sub("", l, count=1)).strip()) for l in raw_lines[:head]}
    tail_keys = {_norm_key(TID.sub("", TS.sub("", l, count=1)).strip()) for l in raw_lines[-tail:]}
    ranked = sorted(order, key=lambda k: (-groups[k]["sev"], -groups[k]["n"] if groups[k]["sev"] else 0,
                                          groups[k]["first"]))
    chosen = [k for k in ranked if groups[k]["sev"] > 0]
    extra = [k for k in order if k in head_keys | tail_keys and k not in chosen]
    rows, used = [], 0
    label = {3: "FATAL", 2: "ERR", 1: "WARN", 0: "info"}
    for k in chosen + extra:
        g = groups[k]
        row = f"[{label[g['sev']]}] {g['line']}" + (f"  x{g['n']}" if g["n"] > 1 else "")
        cost = est_tokens(row)
        if used + cost > budget_tokens:
            rows.append(f"[... {len(chosen) + len(extra) - len(rows)} more distinct lines omitted ...]")
            break
        rows.append(row)
        used += cost
    body = "\n".join(rows)
    return {"text": body, "lines_total": len(raw_lines), "distinct": len(groups),
            "severe": sum(1 for g in groups.values() if g["sev"] >= 2), "tokens_in": est_tokens(text),
            "tokens_out": est_tokens(body)}


def read_tail(path: Path, max_bytes: int = 4_000_000) -> str:
    size = path.stat().st_size
    with path.open("rb") as handle:
        if size > max_bytes:
            handle.seek(size - max_bytes)
            handle.readline()
        return handle.read().decode("utf-8", errors="replace")


# ------------------------------------------------------------------------ tree
def tree(root: Path, depth: int = 2, budget_tokens: int = 700) -> str:
    ignore = Ignore(root)
    names = sorted(p.name for p in root.iterdir() if p.name not in SKIP_DIRS)
    allowed = {p.split("/")[0] for p in ignore.filter([f"{n}/__probe__" if (root / n).is_dir() else n for n in names])}
    skipped = [n for n in names if n not in allowed]
    lines = [f"{root.name}/"]

    def walk(base: Path, level: int, prefix: str):
        entries = sorted(base.iterdir(), key=lambda p: (p.is_file(), p.name))
        dirs = [e for e in entries if e.is_dir() and e.name not in SKIP_DIRS]
        files = [e for e in entries if e.is_file()]
        for d in dirs:
            count = sum(len(f) for _, _, f in os.walk(d))
            lines.append(f"{prefix}{d.name}/ ({count} files)")
            if level < depth:
                walk(d, level + 1, prefix + "  ")
        shown = [f.name for f in files[:12]]
        if shown:
            lines.append(f"{prefix}" + ", ".join(shown) + (f" +{len(files) - 12}" if len(files) > 12 else ""))

    for name in sorted(allowed):
        path = root / name
        if path.is_dir():
            count = sum(len(f) for _, _, f in os.walk(path))
            lines.append(f"  {name}/ ({count} files)")
            if depth >= 2:
                walk(path, 2, "    ")
        else:
            lines.append(f"  {name}")
    if skipped:
        lines.append("  [.llmignore, contents not shown]: " + ", ".join(skipped))
    return clip("\n".join(lines), budget_tokens, "\n  [... tree cut to fit budget ...]")


# --------------------------------------------------------------------- assemble
def _window(text: str, terms: list[str], budget: int, context: int = 3) -> str:
    """Whole file if it fits; otherwise windows around term matches, else head+tail."""
    if est_tokens(text) <= budget:
        return text
    lines = text.splitlines()
    marks = [i for i, l in enumerate(lines) if any(t in l.lower() for t in terms)] if terms else []
    keep: set[int] = set()
    for i in marks:
        keep.update(range(max(0, i - context), min(len(lines), i + context + 1)))
        if est_tokens("\n".join(lines[j] for j in sorted(keep))) > budget:
            break
    if not keep:
        half = max(5, int(budget * CHARS_PER_TOKEN / 2 / 60))
        keep = set(range(min(half, len(lines)))) | set(range(max(0, len(lines) - half), len(lines)))
    out, prev = [], -2
    for i in sorted(keep):
        if i != prev + 1:
            out.append(f"[... line {i + 1} ...]")
        out.append(lines[i])
        prev = i
    return clip("\n".join(out), budget)


def _kb_sections(slug: str) -> list[tuple[str, int, int, str]]:
    """Sections of a knowledge-base document as (breadcrumb, line0, line1, text); empty if unavailable."""
    try:
        import kb as K  # imported lazily: kb imports this module
    except ImportError:
        return []
    if not K.DB_PATH.is_file():
        return []
    con = K.connect()
    try:
        rows = con.execute("SELECT path, line0, line1, text FROM section WHERE slug=? ORDER BY ord", (slug,)).fetchall()
    except sqlite3.Error:
        rows = []
    finally:
        con.close()
    return [(r["path"], r["line0"], r["line1"], r["text"]) for r in rows]


def assemble(root: Path, query: str = "", files: list[str] | None = None, logs: list[str] | None = None,
             total_tokens: int = 12000, state_slug: str = "X360_PROJECT_STATE",
             split: dict | None = None, index: Index | None = None) -> dict:
    split = split or DEFAULT_SPLIT
    caps = {k: int(total_tokens * v) for k, v in split.items()}
    terms = [w for w in re.findall(r"\w+", query.lower(), re.UNICODE) if len(w) > 2 and w not in STOP]
    sections, used = [], {}

    # Architecture: metadata tree + project state (query-relevant sections first when over budget)
    arch_tree = tree(root, depth=2, budget_tokens=max(150, caps["architecture"] // 3))
    arch = [f"### Project tree (metadata only)\n{arch_tree}"]
    chunks = _kb_sections(state_slug)
    if chunks:
        remaining = caps["architecture"] - est_tokens(arch[0])
        scored = sorted(range(len(chunks)), key=lambda i: (
            -sum(chunks[i][3].lower().count(t) + chunks[i][0].lower().count(t) * 3 for t in terms), i))
        take, spent = [], 0
        for i in scored if terms else range(len(chunks)):
            cost = est_tokens(chunks[i][3]) + 12
            if spent + cost > remaining:
                continue
            take.append(i)
            spent += cost
        body = "\n\n".join(f"#### {chunks[i][0]} (lines {chunks[i][1]}-{chunks[i][2]})\n{chunks[i][3]}"
                           for i in sorted(take))
        arch.append(f"### kb:{state_slug}\n{body}" if body else f"### kb:{state_slug}\n[no section fits the budget]")
    text = "\n\n".join(arch)
    sections.append(("Architecture", clip(text, caps["architecture"])))

    # Code / working files + relevant notes from the FTS index
    code_parts, code_cap = [], caps["code"]
    file_cap = int(code_cap * (0.7 if query else 1.0))
    files = files or []
    for rel in files:
        path = (root / rel).resolve()
        if root not in path.parents or not path.is_file() or ".git" in path.parts:
            code_parts.append(f"### {rel}\n[unavailable: outside project or missing]")
            continue
        share = max(200, file_cap // max(1, len(files)))
        body = _window(path.read_text(encoding="utf-8", errors="replace"), terms, share)
        code_parts.append(f"### {rel}\n{body}")
    if query and index is not None:
        hits = index.search(query, k=8)
        room = code_cap - sum(est_tokens(p) for p in code_parts)
        if hits and room > 150:
            code_parts.append("### Relevant notes (FTS)\n" + format_hits(hits, room))
    sections.append(("Code / working files", clip("\n\n".join(code_parts) or "[none requested]", code_cap)))

    # Logs / errors
    log_parts, log_cap = [], caps["logs"]
    for rel in logs or []:
        path = (root / rel).resolve()
        if root not in path.parents or not path.is_file() or ".git" in path.parts:
            log_parts.append(f"### {rel}\n[unavailable: outside project or missing]")
            continue
        d = digest_log(read_tail(path), max(100, log_cap // max(1, len(logs or []))))
        log_parts.append(f"### {rel}  ({d['lines_total']} lines, {d['distinct']} distinct, "
                         f"{d['severe']} warn/error groups, {d['tokens_in']}->{d['tokens_out']} tok)\n{d['text']}")
    sections.append(("Logs / errors", clip("\n\n".join(log_parts) or "[none requested]", log_cap)))

    out = [f"# Context pack (budget {total_tokens} tok est.; architecture<={caps['architecture']}, "
           f"code<={caps['code']}, logs<={caps['logs']})"]
    for (title, body), key in zip(sections, ("architecture", "code", "logs")):
        used[key] = est_tokens(body)
        out.append(f"## {title}  [{used[key]}/{caps[key]} tok]\n{body}")
    return {"text": "\n\n".join(out), "used": used, "caps": caps, "total": sum(used.values())}
