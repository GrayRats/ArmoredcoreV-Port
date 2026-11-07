---
name: acv-docs
description: Use when updating ACV plans, status, evidence, or technical documents. Project documents are in the knowledge base; use the acv-kb skill for the commands.
---

# ACV documentation workflow

Project documents are stored in the SQLite knowledge base (see the `acv-kb` skill). Read sections with `kb.py outline` and `kb.py get`, not whole documents.

## Ownership

| Document | Owns |
|---|---|
| `kb:PLAN_GUI` | Implementation queue, priorities, dependencies and acceptance criteria. The only task list. |
| `kb:PLAN` | Compact status and decisions. |
| `kb:X360_PROJECT_STATE` | Short resume pointer: the summary section and the dated journal. |
| `kb:TOOLS` | Tool selection, availability checks and usage boundaries. |
| `kb:AGENTS` | Working rules for agents and the evidence policy. |
| `kb:PLAN_WINDOWS` | Windows port queue and gates. |
| `kb:TASK_SQL`, `kb:SQLITE_LOGGING` | Persistent execution log and its limits. |
| `kb:ACV_TIMING`, `kb:VULKAN_OPTIMIZATION_OPTIONS` | Technical evidence and option analysis. |
| archived documents | `kb.py index --all --kind archive`; not used for routine work |

## Workflow

1. Find the current item in `kb:PLAN_GUI` (`kb.py rows PLAN_GUI --kind item`) and its acceptance criterion.
2. Read only the sections you need (`kb.py outline`, then `kb.py get SLUG#ANCHOR`).
3. Record the result as a dated journal entry in `kb:X360_PROJECT_STATE`, and update the queue item and `kb:PLAN` if status changed. Store every change with `kb.py put --reason`.
4. Put detailed evidence (commands, hashes, logs) in the technical document or in `logs/`, and link it with a repository path.
5. Run `kb.py verify`, `kb.py links`, `tools/check_active_docs.py`, and `git diff --check`.

## Rules

- Keep claims tied to a source, guest VA, artifact, or saved operation. Separate hypotheses from verified results.
- Preserve history: store corrections as new versions; do not edit bytes in place.
- Do not create a competing queue or a second status document.
