# ACV project — work and evidence rules

## Start with the smallest useful context

1. Read `docs/README.md` and `docs/X360_PROJECT_STATE.md` for document routing
   and current blockers. Use `docs/PLAN.md` for the compact status and decisions;
   `PLAN_GUI.MD` owns the implementation queue and acceptance criteria. Do not scan all docs/fine, logs, generated shards
   or original media on every turn.
2. Review current SQLite task/status and recent relevant errors, then inspect
   the latest evidence for the selected `PLAN_GUI.MD` item. SQLite records
   execution status; it does not replace the plan or prove gameplay acceptance. Verify tool availability
   before use; old status is historical, not a live connection check.
3. Follow the current user request first; otherwise pick the highest-priority
   authorized task whose dependencies/evidence exist.
   Select one concrete uncertainty and an acceptance check. A stop from the user
   remains in force until they request resuming; documentation work is not a
   runtime launch authorization.
4. Use `rg` / `rg --files` on specific files/directories first. Widen only when
   needed. For a guest VA use the relevant evidence index/.pdata/raw listing;
   for an SDK defect trace the source. Do not repeat full ISO scans or whole
   builds when an indexed/local or incremental check answers the question.

## Scope and protected state

- Inspect `git status` before edits. Preserve existing user changes and unrelated
  work; do not reset, clean or rewrite them. Identify affected callers, tests,
  settings, packaging and documentation before implementation.

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

- Before GUI, image extraction, ReXGlue tooling or binary inspection work, read
  [TOOLS.MD](TOOLS.MD) and select the applicable tool/resource. Check availability;
  the guide does not imply that every listed tool is installed.

- Vulkan API, swapchain, pipelines, descriptors, memory, synchronization,
  render targets, shader/output or portability work: read and apply
  Vulkan implementation skill (локальный навык вне репозитория: `~/.codex/skills/vulkan/SKILL.md`) and
  local Vulkan references (локальный навык вне репозитория: `~/.agents/skills/vulkan/SKILL.md`).
  Check ownership/teardown, every fallible VkResult, extensions/features,
  layout/stage/access and visibility, shader/binding contracts. Use only the
  relevant pinned Khronos source; match the actual runtime/driver. Do not
  silently update references. Verify referenced resources exist before using them.
- PPC/XEX, disassembly, decompilation, xrefs or dataflow work: use
  Ghidra RPC skill (локальный навык вне репозитория: `~/.agents/skills/ghidra-rpc/SKILL.md`).
  Discover Ghidra MCP first; if unavailable use `scripts/ghidra-rpc.sh`.
  Check status, project and loaded binary. Reuse saved default.xex with
  --no-analyze for read-only investigation; do not reimport/auto-analyze the
  protected DB. Incomplete function bodies or noreturn metadata require raw
  instructions/.pdata fallback, not acceptance of truncated pseudocode.
- Xbox360 recomp/ISO/STFS/imports/configs: apply relevant
  xbox360-decomp (локальный навык вне репозитория: `~/.agents/skills/xbox360-decomp/SKILL.md`) and
  [360tools](tools/360tools-skill/SKILL.md) resources after reading their guidance.
  Do not assume a tool, supported PPC/XEX profile, command flag or oracle exists.

## Implementation boundaries

- The selected GUI is trg-launcher. Follow stage C in `PLAN_GUI.MD`; the existing
  Qt launcher is a working checkpoint until the replacement passes acceptance.
  Reuse the Python settings/profile backend, stable UUID/XUID, XDG paths, writer
  locks and backup/restore. Do not create a second settings schema in the UI.
- Consult the relevant ReAnswered example through `TOOLS.MD` before analogous
  runtime/input changes. Validate SDK compatibility and ACV guest evidence;
  another game's hooks and addresses are not ACV fixes.
- Keep durable fixes in source, ReXGlue configuration or reproducible SDK patches.
  Generated translations are evidence/output, not the primary repair layer.
  Do not mask invalid guest calls with fabricated objects or silent skips.
- Reuse existing code and dependencies. Add only what the requested behavior needs;
  preserve error handling, validation and accessibility when moving code.

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
  candidate if blocked. Bound long runs explicitly; default timeout is 120 s.
- Track work with `cli.mjs task ID STATUS TITLE`. Exit 0 is not semantic proof;
  `verify OPERATION_ID NOTE` requires a checked result. Keep evidence by guest
  VA or SDK file:line, source, scope and confidence. Separate instructions,
  generated output and hypotheses; C++ reconstructions use // @retail 0x... .
- Validate the change at its actual boundary: same-input oracle when available,
  meaningful targeted static/host test otherwise. Preserve working checkpoints;
  do not clean incremental objects or switch codegen before required byte checks.
- At a milestone update the relevant technical section and one PROGRESS row;
  update `PLAN_GUI.MD` for changed work/blockers, `docs/PLAN.md` for summary
  decisions, and STATE only if resume context changes.
  Full command output stays in SQLite/logs, not repeated MD transcripts.

## Acceptance and hand-off

- Choose checks that exercise the changed boundary. Non-trivial logic needs a
  targeted regression or assert-based check; documentation edits need link and
  diff checks, not a runtime launch. Run builds/tests through the logger.
- Distinguish source available, built, host-tested, runtime-tested and accepted
  by the user. A successful build, visible window or swap cadence does not prove
  gameplay, save/load, image correctness or game-time pacing.
- Candidate runtime/GPU packages do not replace accepted artifacts until the
  relevant checks pass. Record the manifest/hashes, effective settings and session;
  a historical path or abbreviated hash is not a current artifact verification.
- Report what changed, what was checked and remaining limits. Name untested
  behavior and material risks briefly; do not claim full playability or parity
  from a single mission transition.

## Documentation and cleanup

| Document | Ownership |
|---|---|
| `PLAN_GUI.MD` | Implementation queue, priorities, dependencies and acceptance |
| `docs/PLAN.md` / `docs/PLAN.RU.MD` | Compact status and user decisions; keep translations aligned |
| `docs/PROGRESS.md` | Verified milestones and limits |
| `docs/X360_PROJECT_STATE.md` | Short resume pointer, not a second queue |
| `TOOLS.MD` | Tool selection, availability checks and usage boundaries |
| Technical docs | Detailed evidence and hypotheses |

Use `docs/README.md` for routing. Read only the relevant sections; keep full logs
in SQLite/logs. When evidence conflicts, compare session, artifact and verification
scope against the latest technical result; label unresolved contradictions rather
than merging them into a new claim. A dated record does not show a live process.

Replace superseded facts; when compressing history preserve the previous bytes
with checksums. `docs/fine/` is excluded from routine searches and rewrites.
After active documentation edits run:

```bash
node tools/decomp-logging/cli.mjs run --stage docs_check -- python3 tools/check_active_docs.py
node tools/decomp-logging/cli.mjs run --stage docs_check -- git diff --check -- <changed-files>
```

The link checker covers local paths, not remote URLs or anchors. Check those
separately when changed. Verify archive hashes when creating/moving snapshots.

`cli.mjs import-logs` / cleanup previews are available; cleanup --apply may
remove only archived, checksum-verified, unreferenced duplicates with restoration
records. Preserve source code/media, unique evidence and referenced files.
