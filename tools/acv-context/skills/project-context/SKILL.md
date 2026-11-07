---
name: project-context
description: Retrieve project knowledge cheaply. Use ctx_search / ctx_log_digest / ctx_assemble instead of reading whole Markdown files or raw logs in this repository.
---

# Project context tools (MCP server `acv-context`)

Before opening a large `.md` file or a log, use the tools:

- `ctx_search(query, k=5, max_tokens=1500)` — BM25 over heading-aware Markdown chunks; answers carry `path:lines`.
  Open the cited file range only if the chunk is not enough.
- `ctx_log_digest(path, max_tokens=800)` — a log with timestamps stripped, repeats collapsed (`xN`), errors first.
  Never `Read` a raw `runtime.log`; digest it, then grep the raw file for one specific line if needed.
- `ctx_tree(depth=2)` — compact tree without contents; `.llmignore` paths are listed as hidden.
- `ctx_assemble(query, files, logs, total_tokens)` — one pack with hard caps: architecture 15 %,
  code/working files 60 %, logs 25 %. Unused budget is not carried over.
- `kb_outline(slug)`, `kb_get("SLUG#anchor")`, `kb_rows(slug, kind="item"|"table"|"dated", limit=N)`, `kb_search(query)` —
  project documents stored in SQLite (`tools/acv-context/knowledge.sqlite3`). Read a section, not a whole document;
  records are JSON lines. Read-only; changes go through `tools/acv-context/kb.py put --reason`.
- `ctx_reindex()` — after bulk edits to Markdown (a SessionStart hook already refreshes incrementally).

Token figures are estimates (characters / 3), not tokenizer output. The index covers Markdown only,
excluding paths matched by `.llmignore`; search misses mean "not in indexed notes", not "not in the repo".


Project knowledge: use the `acv-context` MCP tools (`mcp__acv-context__kb_index`, `kb_card`, `kb_get`, `kb_rows`, `kb_search`) before the CLI.
