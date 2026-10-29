---
name: acv-docs
description: Use when updating active ACV plans, status, evidence, or technical documents.
---

# ACV documentation workflow

Follow the repository [documentation ownership and cleanup rules](../../../docs/AGENTS.md). Route readers through `docs/README.md`; do not create a competing task queue.

- `PLAN_GUI.MD` owns current implementation priorities and acceptance.
- `docs/PLAN.md` and `docs/PLAN.RU.MD` own compact status in English and Russian.
- `docs/PROGRESS.md` owns verified milestones and limits.
- `docs/X360_PROJECT_STATE.md` is a short resume pointer.
- Technical documents own detailed claims and evidence.

Keep claims tied to a source, guest VA, artifact, or saved operation. Distinguish hypotheses from verified results. Preserve historical bytes with hashes when compressing material. After edits, run `tools/check_active_docs.py` and `git diff --check` through the project logger.
