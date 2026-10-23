#!/usr/bin/env python3
"""Read-only link and canonical-entry checks for active ACV Markdown (fine excluded)."""
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
LINK = re.compile(r"\]\(([^\s)]+)\)")
SCHEME = re.compile(r"^[a-zA-Z][a-zA-Z0-9+.-]*:")


def check(root):
    files = set(root.glob("*.md")) | set(root.glob("*.MD"))
    files.update(p for p in (root / "docs").rglob("*.md")
                 if "fine" not in p.relative_to(root / "docs").parts)
    issues = []
    links = 0
    for source in sorted(files):
        for line, text in enumerate(source.read_text(encoding="utf-8").splitlines(), 1):
            for target in LINK.findall(text):
                if target.startswith("#") or SCHEME.match(target):
                    continue
                links += 1
                if not (source.parent / target.split("#", 1)[0]).exists():
                    issues.append({"file": str(source.relative_to(root)),
                                   "line": line, "missing_target": target})
    for name in ("docs/README.md", "docs/PLAN.md", "docs/X360_PROJECT_STATE.md"):
        if not (root / name).is_file():
            issues.append({"missing_entry": name})
    for name in ("X360_PROJECT_STATE.md", "TASK-XBOX.md", "docs/PLAN_B.md"):
        if (root / name).exists():
            issues.append({"obsolete_duplicate": name})
    return {"ok": not issues, "scope": "active Markdown paths; anchors and remote URLs not checked",
            "fine_scanned": False, "documents": len(files),
            "local_links_checked": links, "issues": issues}


if __name__ == "__main__":
    result = check(ROOT)
    print(json.dumps(result, ensure_ascii=False, indent=2))
    sys.exit(0 if result["ok"] else 1)
