#!/usr/bin/env python3
"""Evaluate `.llmignore` (gitignore syntax) for LLM agents.

    llmignore.py check PATH...   print `ignored <rule>` or `allowed` per path; exit 1 if any ignored
    llmignore.py list            ignored top-level entries with sizes (what agents must not scan)
    llmignore.py sync            regenerate `.ignore` so ripgrep/fd (Grep/Glob tools) skip the same paths
    llmignore.py tracked         tracked git files that match .llmignore (rules that are too broad)

Matching is delegated to git itself in a scratch repository whose only ignore file is a copy of
`.llmignore`, so repository `.gitignore` rules never leak into the result. Paths need not exist.
"""
from __future__ import annotations
import sys as _sys
if _sys.version_info < (3, 15):
    raise SystemExit('requires Python 3.15+; run: uv run --no-project --python 3.15 python -I <script>')

import argparse
import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RULES = ROOT / ".llmignore"
HEADER = ("# GENERATED from .llmignore by tools/llmignore.py sync. Do not edit.\n"
          "# ripgrep and fd read this file automatically.\n")


def _git(cwd: Path, *args: str, stdin: str | None = None) -> subprocess.CompletedProcess:
    env = dict(os.environ, GIT_CONFIG_GLOBAL="/dev/null", GIT_CONFIG_SYSTEM="/dev/null")
    return subprocess.run(["git", *args], cwd=cwd, input=stdin, text=True,
                          capture_output=True, env=env, check=False)


class Matcher:
    """Scratch git repo holding only the .llmignore rules."""

    def __init__(self, rules: Path = RULES):
        if not rules.is_file():
            raise SystemExit(f"missing {rules}")
        self._tmp = tempfile.TemporaryDirectory(prefix="llmignore-")
        self.dir = Path(self._tmp.name)
        _git(self.dir, "init", "-q")
        (self.dir / ".gitignore").write_text(rules.read_text(encoding="utf-8"), encoding="utf-8")

    def check(self, paths: list[str]) -> dict[str, str | None]:
        """Map each path to its matching rule line, or None when allowed."""
        result: dict[str, str | None] = {}
        for raw in paths:
            path = Path(raw)
            rel = (path.resolve().relative_to(ROOT) if path.is_absolute() or path.exists()
                   else path).as_posix()
            # Directory-only rules need a directory to exist in the scratch repo.
            probe = rel.rstrip("/")
            parts = probe.split("/")
            for i in range(1, len(parts)):
                (self.dir / "/".join(parts[:i])).mkdir(exist_ok=True)
            if raw.endswith("/") or (ROOT / rel).is_dir():
                (self.dir / probe).mkdir(parents=True, exist_ok=True)
            else:
                target = self.dir / probe
                target.parent.mkdir(parents=True, exist_ok=True)
                target.touch()
            out = _git(self.dir, "check-ignore", "-v", "--no-index", "--", probe)
            if out.returncode == 0 and out.stdout.strip():
                head = out.stdout.split("\t", 1)[0]
                result[rel] = head.split(":", 2)[2] if head.count(":") >= 2 else head
            else:
                result[rel] = None
        return result


def cmd_check(args) -> int:
    matches = Matcher().check(args.paths)
    for path, rule in matches.items():
        print(f"ignored  {path}  ({rule})" if rule else f"allowed  {path}")
    return 1 if any(matches.values()) else 0


def _size(path: Path) -> int:
    if path.is_file():
        return path.stat().st_size
    total = 0
    for dirpath, _, files in os.walk(path):
        for name in files:
            try:
                total += (Path(dirpath) / name).lstat().st_size
            except OSError:
                pass
    return total


def cmd_list(_args) -> int:
    matcher = Matcher()
    entries = sorted(p.name for p in ROOT.iterdir() if p.name != ".git")
    first = matcher.check(entries)
    rows = [(name, rule) for name, rule in first.items() if rule]
    for name, rule in rows:
        print(f"{_size(ROOT / name) / 1e6:12.1f} MB  {name}  ({rule})")
    print(f"{len(rows)} of {len(entries)} top-level entries ignored; "
          "nested rules (extensions, build dirs, third-party trees) apply below the top level")
    return 0


def cmd_sync(_args) -> int:
    body = "\n".join(line for line in RULES.read_text(encoding="utf-8").splitlines()
                     if line.strip() and not line.lstrip().startswith("#"))
    (ROOT / ".ignore").write_text(HEADER + body + "\n", encoding="utf-8")
    print(f"wrote {ROOT / '.ignore'}")
    return 0


def cmd_tracked(_args) -> int:
    tracked = _git(ROOT, "ls-files").stdout.splitlines()
    matcher = Matcher()
    hits = [(p, r) for p, r in matcher.check(tracked).items() if r]
    for path, rule in hits:
        print(f"{path}  ({rule})")
    print(f"{len(hits)} of {len(tracked)} tracked files match .llmignore")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    check = sub.add_parser("check")
    check.add_argument("paths", nargs="+")
    check.set_defaults(func=cmd_check)
    for name, func in (("list", cmd_list), ("sync", cmd_sync), ("tracked", cmd_tracked)):
        sub.add_parser(name).set_defaults(func=func)
    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
