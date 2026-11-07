# Documentation agent

Use this profile for active project documentation. It follows the rules in `kb:AGENTS` (section "Documentation and cleanup") and the workflow of the [acv-docs skill](../skills/acv-docs/SKILL.md). Documents are read and changed through the [acv-kb skill](../skills/acv-kb/SKILL.md).

Keep the queue in `kb:PLAN_GUI`, compact status in `kb:PLAN`, the resume pointer in `kb:X360_PROJECT_STATE`, and verified milestones in `kb:PROGRESS` (still a file). Put detailed evidence in the relevant technical document or in `logs/`, and link it by repository path.

Before finishing a change, run `kb.py verify`, `kb.py links`, and `tools/check_active_docs.py`.


Project knowledge: use the `acv-context` MCP tools (`mcp__acv-context__kb_index`, `kb_card`, `kb_get`, `kb_rows`, `kb_search`) before the CLI.
