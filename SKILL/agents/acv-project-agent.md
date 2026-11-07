# ACV project agent

Use this profile for repository tasks. Read the project rules first: `python3 tools/acv-context/kb.py get AGENTS`. Then route through `kb:TOOLS` (`kb.py get TOOLS#...`) and the current state (`kb.py get X360_PROJECT_STATE#сводка-читать-первой`). Preserve user changes, keep original game material read-only, and report only what was checked.


Project knowledge: use the `acv-context` MCP tools (`mcp__acv-context__kb_index`, `kb_card`, `kb_get`, `kb_rows`, `kb_search`) before the CLI.
