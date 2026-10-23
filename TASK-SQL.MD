# TASK-SQL — persistent project control

## Objective

Record observable project operations, output, errors and evidence in separate
SQLite files; preserve existing history and allow verified restoration of logs.
SQLite is the execution-history source, not proof of recovered gameplay semantics.

## Storage

- `logs/decomp.sqlite3`: sessions, operations, functions, event/artifact index,
  recovery attempts, tasks and archive progress (existing IDs preserved).
- `logs/errors.sqlite3`: diagnostic details, fingerprints and resolution links.
- `logs/output.sqlite3`: compressed output streams and archived log contents.
- `logs/artifacts.sqlite3`: paths, provenance, checksums, cleanup/restore audit.
- Fallback: `logs/sqlite-emergency.jsonl`; pre-upgrade snapshot:
  `logs/sqlite-backups/decomp-before-split.sqlite3`.

Implementation: `tools/decomp-logging/` (pinned better-sqlite3 13.0.3).
Instructions and limitations: [SQLITE_LOGGING.md](docs/SQLITE_LOGGING.md).

## Mandatory work loop

1. `node tools/decomp-logging/cli.mjs init` in a newly prepared workspace.
2. Before a retry: query `errors` / `history`; inspect prior failures and actions.
3. Run analysis, code generation, build, test and corrective commands through
   `cli.mjs run --stage STAGE -- COMMAND ARGS`. Common project scripts already
   enter that boundary automatically. Record saved direct tool/MCP results with
   `cli.mjs ingest --tool NAME --stage STAGE RESULT_FILE`.
4. Persist failures before recovery. A correction uses
   `run --retry-of EVENT_ID --action DESCRIPTION -- COMMAND...`; default bound is
   three attempts per fingerprint within the original event's workflow stage.
   Success remains `not_checked` until a real
   result check is recorded with `verify OPERATION_ID NOTE`.
   For source/document listings only, `run --stdout-data` retains stdout as
   redacted data; stderr/exit/spawn/timeout checks stay active. Never use it for
   build/test/runtime/MCP outputs. Mode is recorded in operation context.
5. Update a persistent task with `cli.mjs task ID STATUS TITLE`; review `status`
   and unresolved `errors` before ending the session.

## Existing records / cleanup

1. Keep all old DB history. Label legacy output previews as incomplete.
2. `import-logs`: stream old `logs/` files into compressed SQLite chunks; verify
   source/stored hashes and record historical diagnostic provenance.
3. `cleanup` previews exact duplicate logs; `cleanup --apply` removes only
   verified, archived, unreferenced duplicates, retaining a canonical source and
   a restoration record. Protect source media, source code and unique evidence.
4. Recheck archive integrity and documentation references after cleanup.

## Acceptance checks

- Main schema migrations and separate stores persist across invocations.
- WAL, FK enforcement, busy timeout, prepared writes and emergency replay work.
- Failures with exit zero, spawn/timeout/compiler errors and nonzero statuses are captured.
- Full bounded-chunk output is retrievable; redaction/oversized-line changes are explicit.
- Existing events/artifacts retain IDs; import is idempotent and hashes round-trip.
- Failed correction and successful verified retry have traceable links.
- Exact-duplicate deletion is audited and reversible; referenced files are kept.
- `node --test tools/decomp-logging/*.test.mjs` passes all 10 tests (node --test; 0 failures).

## Limits to retain

Raw commands/MCP calls outside these boundaries are not automatically captured.
Only observed results may be ingested; never manufacture historical executions.
Cross-database relationships are reconciled by ID, not cross-file SQLite FKs;
WAL does not guarantee atomic multi-file crash recovery. Inspect incomplete
streams and `archive_runs.status='running'` after an interrupted session.


## Verified checkpoint

Initial bulk import: 294 existing files / 4,403,652,210 bytes, all snapshots
verified by decompression and SHA-256. Previous IDs/history are unchanged;
303 original events and 113 original operations are preserved. Three unreferenced duplicate `.log` files (two empty and one duplicate help output) were deleted with restoration records.
Final eight test groups (including concurrent initialization) passed under
verified operation `3a250564-b1f1-45eb-bc51-73458ae017af`.
Evidence: [validation.json](logs/sqlite-control/validation.json),
[cleanup.json](logs/sqlite-control/cleanup.json). Further work is operational:
keep new commands/results within the logging boundaries and review actual errors.
