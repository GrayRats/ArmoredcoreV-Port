# ACV project — work and evidence rules

## Start with the smallest useful context

1. Read `docs/README.md` and `docs/X360_PROJECT_STATE.md` for document routing
   and current blockers. Read only the relevant sections of `docs/PLAN.md`;
   it is the sole task queue. Do not scan all docs/fine, logs, generated shards
   or original media on every turn.
2. Review current SQLite task/status and recent relevant errors, then inspect
   the latest evidence for the selected PLAN item. Verify tool availability
   before use; old status is historical, not a live connection check.
3. Pick the highest-priority authorized task whose dependencies/evidence exist.
   Select one concrete uncertainty and an acceptance check. A stop from the user
   remains in force until they request resuming; documentation work is not a
   runtime launch authorization.
4. Use `rg` / `rg --files` on specific files/directories first. Widen only when
   needed. For a guest VA use the relevant evidence index/.pdata/raw listing;
   for an SDK defect trace the source. Do not repeat full ISO scans or whole
   builds when an indexed/local or incremental check answers the question.

## Scope and protected state

- Linux first. Windows cross-build/Wine/new Windows work needs a separate request.
  Unity/C# is optional. Keep XenonRecomp/ReXGlue code/runtime pipelines distinct.
- Original ISO/assets read-only. Workspace outputs: tools/, tmp/, logs/, docs/.
- Live Ghidra DB read-only. Structural/.pdata repair only on a separately
  authorized copy. Do not stop/start foreign processes or change ptrace_scope,
  sudo, packages, desktop or user settings without authorization.
- Do not save analysis screenshots/PNG thumbnails. Use RDC/XML/replay, logs,
  raw instructions and guest memory. Preserve existing unique evidence.
- Default launch settings are `tools/native-rexglue/acv.config`:
  pilot_name=ARTEM, gamertag=ARTEM, headless=true. Preserve this user config;
  record diagnostic overrides explicitly. Missing config must fail, not silently
  revert to SDK User. Do not replace it with temporary configs for routine use.

## Required skills and tool selection

- Vulkan API, swapchain, pipelines, descriptors, memory, synchronization,
  render targets, shader/output or portability work: read and apply
  [Vulkan implementation skill](/home/art/.codex/skills/vulkan/SKILL.md) and
  [local Vulkan references](/home/art/.agents/skills/vulkan/SKILL.md).
  Check ownership/teardown, every fallible VkResult, extensions/features,
  layout/stage/access and visibility, shader/binding contracts. Use only the
  relevant pinned Khronos source; match the actual runtime/driver. Do not
  silently update references. The implementation skill's PLAYBOOK is absent.
- PPC/XEX, disassembly, decompilation, xrefs or dataflow work: use
  [Ghidra RPC skill](/home/art/.agents/skills/ghidra-rpc/SKILL.md).
  Discover Ghidra MCP first; if unavailable use `scripts/ghidra-rpc.sh`.
  Check status, project and loaded binary. Reuse saved default.xex with
  --no-analyze for read-only investigation; do not reimport/auto-analyze the
  protected DB. Incomplete function bodies or noreturn metadata require raw
  instructions/.pdata fallback, not acceptance of truncated pseudocode.
- Xbox360 recomp/ISO/STFS/imports/configs: apply relevant
  [xbox360-decomp](/home/art/.agents/skills/xbox360-decomp/SKILL.md) and
  [360tools](tools/360tools-skill/SKILL.md) resources after reading their guidance.
  Do not assume a tool, supported PPC/XEX profile, command flag or oracle exists.

## Execute, validate, preserve

- Run analysis/build/tests/corrective commands through
  `node tools/decomp-logging/cli.mjs run --stage STAGE -- COMMAND ARG...`.
  `scripts/decomp.sh`, `ghidra-rpc.sh`, `assets.sh`, `unity.sh` already enter
  the logger. Do not bypass via internal XBOXDECOMP_LOGGING_ACTIVE.
- Source/document listings may use `run --stdout-data`: stdout is stored and
  redacted as data, stderr/exit codes remain checked. Never use that flag for
  builds, tests, runtime probes or MCP/Ghidra results; their stdout diagnostics
  must remain active. The mode is recorded in operation context.
- For direct MCP results save the actual result in logs/ or tmp/, then
  `cli.mjs ingest --tool NAME --stage STAGE FILE`. Unwrapped operations and
  invisible failures are not automatically recorded. See TASK-SQL.MD and
  docs/SQLITE_LOGGING.md for persistence/limits; read them when changing logging.
- Before retry query `cli.mjs errors --stage STAGE`; identify the actual event
  and prior recovery. Use `run --retry-of EVENT_ID --action DESCRIPTION` for an
  evidenced correction. Do not loop identical failures; use another independent
  candidate if blocked. Bound long runs explicitly; default timeout is120 s.
- Track work with `cli.mjs task ID STATUS TITLE`. Exit0 is not semantic proof;
  `verify OPERATION_ID NOTE` requires a checked result. Keep evidence by guest
  VA or SDK file:line, source, scope and confidence. Separate instructions,
  generated output and hypotheses; C++ reconstructions use // @retail 0x... .
- Validate the change at its actual boundary: same-input oracle when available,
  meaningful targeted static/host test otherwise. Preserve working checkpoints;
  do not clean incremental objects or switch codegen before required byte checks.
- At a milestone update the relevant technical section and one PROGRESS row;
  update PLAN for changed work/blockers, STATE only if resume context changes.
  Full command output stays in SQLite/logs, not repeated MD transcripts.

## Documentation and cleanup

`docs/README.md` maps document ownership. PLAN is the only work queue;
PROGRESS is accepted milestones/limits; STATE is a short resume index;
technical docs hold evidence. Replace superseded facts; keep history losslessly
archived with checksums. docs/fine is excluded from routine optimization/search.

`cli.mjs import-logs` / cleanup previews are available; cleanup --apply may
remove only archived, checksum-verified, unreferenced duplicates with restoration
records. Preserve source code/media, unique evidence and referenced files.
Do not claim completion/matching/parity beyond the observed verification.
