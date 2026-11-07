#!/usr/bin/env python3
"""Read-only checks for active ACV documentation.

Checks three things:
  1. local Markdown links in the remaining document files (fine/ archives excluded);
  2. the knowledge base (tools/acv-context/knowledge.sqlite3): hash chain, derived layers, kb: references;
  3. the retired docs folder must not come back, and required kb documents must exist.
"""
import sys as _sys
if _sys.version_info < (3, 15):
    raise SystemExit('requires Python 3.15+; run: uv run --no-project --python 3.15 python -I <script>')
import json
from pathlib import Path
import re
import sqlite3
import sys

ROOT = Path(__file__).resolve().parents[1]
KB_PATH = ROOT / "tools" / "acv-context" / "knowledge.sqlite3"
LINK = re.compile(r"\]\(([^\s)]+)\)")
SCHEME = re.compile(r"^[a-zA-Z][a-zA-Z0-9+.-]*:")
REQUIRED_KB = ("AGENTS", "PLAN", "PLAN_GUI", "X360_PROJECT_STATE", "TOOLS", "PLAN_WINDOWS", "TASK_SQL")
DOCS_FOLDER = ROOT / "tools" / "acv-context" / "docs"   # must not come back: documents live in the knowledge base


def check_markdown(root):
    files = set(root.glob("*.md")) | set(root.glob("*.MD"))
    issues, links = [], 0
    for source in sorted(files):
        for line, text in enumerate(source.read_text(encoding="utf-8").splitlines(), 1):
            for target in LINK.findall(text):
                if target.startswith("#") or SCHEME.match(target):
                    continue
                links += 1
                if not (source.parent / target.split("#", 1)[0]).exists():
                    issues.append({"file": str(source.relative_to(root)), "line": line, "missing_target": target})
    return files, links, issues


def check_kb(kb_path):
    issues = []
    if not kb_path.is_file():
        return [{"kb_missing": str(kb_path)}]
    sys.path.insert(0, str(kb_path.parent))
    import kb as K  # noqa: E402
    con = K.connect(kb_path)
    for problem in K.verify(con):
        issues.append({"kb_verify": problem})
    for src, ref, ok in K.links(con):
        if not ok:
            issues.append({"kb_link": f"{src} -> kb:{ref}"})
    present = {r["slug"] for r in con.execute("SELECT slug FROM doc")}
    for slug in REQUIRED_KB:
        if slug not in present:
            issues.append({"kb_missing_document": slug})
    con.close()
    return issues


def check(root):
    files, links, issues = check_markdown(root)
    issues += check_kb(root / "tools" / "acv-context" / "knowledge.sqlite3")
    if DOCS_FOLDER.exists():
        issues.append({"docs_folder_present": str(DOCS_FOLDER.relative_to(root)),
                       "fix": "documents are stored in the knowledge base; move files into kb.py put"})
    return {"ok": not issues,
            "scope": "remaining Markdown files, knowledge base (kb: references, hashes), docs-folder guard; anchors and remote URLs not checked",
            "fine_scanned": False, "documents": len(files), "local_links_checked": links, "issues": issues}


if __name__ == "__main__":
    result = check(ROOT)
    print(json.dumps(result, ensure_ascii=False, indent=2))
    sys.exit(0 if result["ok"] else 1)
