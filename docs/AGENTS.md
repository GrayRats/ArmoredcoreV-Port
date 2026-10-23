# Active documentation rules

Applies to active docs; docs/fine is preserved historical material and is
excluded from routine rewrites/optimization. Root AGENTS.md remains authoritative.

- Begin with README.md and X360_PROJECT_STATE.md; choose one open PLAN_GUI.MD item.
  Do not create another task list or chronological dump in a technical doc.
- PLAN_GUI.MD owns priorities and acceptance for the current launcher/profile
  implementation; docs/PLAN.md is a compact native status index. PROGRESS owns
  checked milestones and limits; STATE owns only a short resume pointer. ARCHITECTURE owns boundaries,
  technical documents own detailed claims and sources. README routes readers.
- Read the relevant Vulkan API/reference skills for GPU work and Ghidra RPC skill
  for PPC/XEX work (absolute links in README/root AGENTS). Discover MCP/RPC
  availability, project, binary and actual runtime versions instead of assuming.
- Each RE claim needs VA or SDK file:line, evidence and confidence. Preserve
  unknowns, rejected assumptions, dataflow/ABI limitations and oracle scope.
  Old runtime observations do not imply current process/system state.
- Replace a superseded section/row with current facts; preserve the earlier
  bytes in a hash-checked archive if context is compressed. Never merge
  conflicting historical statements into a new unqualified claim.
- Use relative links inside the project. When moving a file, rebase its links
  and update active references, including root instructions and code metadata.
  Validate links and snapshot hashes after edits; do not rewrite logs as history.
- Keep full stdout/stderr, raw captures and long listings in logs/SQLite;
  use compact evidence pointers in MD. No analysis screenshots or PNG previews.
- A docs-only change needs document/link/integrity checks, not a game launch,
  broad codegen/build or new runtime experiment. User stop remains in force.

- After active-doc edits run the read-only `tools/check_active_docs.py` through
  the SQLite logger. It excludes fine/ and reports missing local paths/obsolete
  duplicate plans; it does not verify remote URLs or Markdown anchors.
