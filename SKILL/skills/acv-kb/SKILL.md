---
name: acv-kb
description: Use to read, search, or change ACV project documents stored in the SQLite knowledge base (tools/acv-context/knowledge.sqlite3) instead of Markdown files.
---

# ACV knowledge base

Ten project documents live in SQLite, not in files: `AGENTS`, `TOOLS`, `PLAN_GUI`, `PLAN_WINDOWS`, `TASK_SQL`, `PLAN`, `X360_PROJECT_STATE`, `ACV_TIMING`, `SQLITE_LOGGING`, `VULKAN_OPTIMIZATION_OPTIONS`. List them with `python3 tools/acv-context/kb.py list`.

Run every command with Python 3.15: `uv run --no-project --python 3.15 python -I tools/acv-context/kb.py ...`
(or set `alias kb='uv run --no-project --python 3.15 python -I tools/acv-context/kb.py'`).

Routing first: `kb.py index` (llms.txt-style list of active documents with summaries and keywords), then
`kb.py card SLUG` (routing card as JSON). Archived documents are hidden unless `--all` is given
(`kb.py index --all --kind archive`). Binary files are attachments: `kb.py attachments`, `kb.py attachment-out NAME --out PATH`.

Addresses: `kb:SLUG` (document), `kb:SLUG#anchor` (section), `kb:SLUG#anchor/tN/rM` (table row or list item). Commands run from the repository root.

## Use the MCP server first

The project server `acv-context` (declared in `.mcp.json`) is the primary access path. Call its tools instead of the CLI:
`mcp__acv-context__kb_index` (map), `kb_card`, `kb_outline`, `kb_get`, `kb_rows`, `kb_search`. They are read-only.
The CLI below is the fallback and the only way to write (`put --reason`).

## Read cheapest first

1. `python3 tools/acv-context/kb.py outline SLUG` — section addresses and sizes in estimated tokens.
2. `python3 tools/acv-context/kb.py get SLUG#ANCHOR` — one section with its address.
3. Structured records:
   - queue items: `kb.py rows PLAN_GUI --kind item`
   - table rows: `kb.py rows SLUG --kind table --table N --where COL=VALUE`
   - journal entries: `kb.py rows X360_PROJECT_STATE --kind dated --limit 5` (newest first)
4. `kb.py search "query"` when the section is not known; it returns addresses, not whole documents.

Plugin `acv-context` exposes the same reads as MCP tools `kb_index`, `kb_card`, `kb_outline`, `kb_get`, `kb_rows`, `kb_search`. They are read-only; changes go through the CLI.

## Change

- Never write to the database with SQL. Export, edit, store:
  `kb.py get SLUG --format md > tmp/kb/SLUG.md`, edit the file, then
  `kb.py put SLUG --file tmp/kb/SLUG.md --reason "what changed and why"`.
- Each `put` creates a new version. Earlier bytes stay: `kb.py history SLUG`, `kb.py get SLUG --version N`, `kb.py diff SLUG A B`.
- Journal entries (`YYYY-MM-DD: ...` paragraphs) are added at the top of the journal section and are never rewritten. A correction is a new entry that names the entry it corrects.
- Documents refer to each other only with `kb:` addresses. Project files are written as repository-root paths (`tools/...`, `logs/...`). Address placeholders in prose are written as `kb:<slug>`.

## Verify before finishing

1. `python3 tools/acv-context/kb.py verify` — hash chain, versions, derived layers; must print `problems=0`.
2. `python3 tools/acv-context/kb.py links` — every `kb:` reference resolves; must print `unresolved=0`.
3. `python3 tools/check_active_docs.py` — knowledge-base checks plus the remaining Markdown files.

## Rules

- Record only facts with a source: a path, guest VA, artifact hash, or saved operation. Separate hypotheses from verified results.
- There is one queue: the items of `kb:PLAN_GUI`. Do not add a second task list anywhere.
- Do not rewrite or delete history. If a statement is wrong, store a corrected version with a reason, and keep the old one in history.
