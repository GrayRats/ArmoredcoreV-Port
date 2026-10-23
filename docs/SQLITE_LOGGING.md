# SQLite workflow control

The workspace-local package is `tools/decomp-logging/` (Node >=22,
`better-sqlite3` pinned to 13.0.3). It uses four persistent files:

| Database | Contents |
|---|---|
| `logs/decomp.sqlite3` | Existing sessions, operations, functions, recovery attempts and event/artifact index; tasks and archive-run progress |
| `logs/errors.sqlite3` | WARNING/ERROR/CRITICAL diagnostics, fingerprints, source paths, context and verified resolution links |
| `logs/output.sqlite3` | Compressed stdout/stderr chunks, stream checksums, archived log contents and completeness flags |
| `logs/artifacts.sqlite3` | Artifact provenance, archived paths and source/stored checksums; exact-duplicate deletion/restoration audit |

The main database and its existing IDs are preserved. A pre-upgrade SQLite backup
is at `logs/sqlite-backups/decomp-before-split.sqlite3`. `init` applies versioned
main migrations, reconciles event/artifact mirrors by ID, imports emergency
JSONL entries idempotently and performs a real write/read check. The old output
previews are labelled `legacy_preview_only=1`, `complete=0`; missing historical
bytes are never claimed recovered from the previews.

Each database uses WAL; the connection enables foreign keys and a 5 s busy
timeout. Cross-file IDs are reconciled by the logger: SQLite cannot enforce
foreign keys across attached databases, and WAL does not provide crash atomicity
across files. The main event index is the durable source for diagnostic/metadata
mirrors. A crash during output capture leaves the stream incomplete; an archive
is eligible for cleanup only after full decompression/checksum verification.

## Execute and review

From the workspace root:

```bash
node tools/decomp-logging/cli.mjs init
node tools/decomp-logging/cli.mjs run --stage static_analysis --va 0x8245FA20 -- python3 tools/ppc_dis.py tmp/scan/text.bin 0x8245FA20 0x8245FAE0
node tools/decomp-logging/cli.mjs status
node tools/decomp-logging/cli.mjs history --limit 20
node tools/decomp-logging/cli.mjs errors --va 0x8245FA20
node tools/decomp-logging/cli.mjs errors --include-history --limit 20
```

The four common scripts (`decomp.sh`, `ghidra-rpc.sh`, `assets.sh`, `unity.sh`)
automatically invoke the logger and preserve their caller's working directory.
Nested scripts use the parent operation's output capture, avoiding recursive
instrumentation. Other subprocesses must use `run`; direct MCP/API results must
be saved and passed to `ingest --tool NAME --stage STAGE RESULT_FILE`.
Unobservable external activity is not captured.

For historical diagnostic review use the CLI `errors` query directly, or output
only IDs/counts from a wrapped custom audit. For a known source/document listing
use `run --stdout-data --stage source_read -- COMMAND...`: stdout remains
redacted and fully stored but is treated as data, not new tool diagnostics.
Stderr, nonzero exit, spawn failure, timeout and signals remain checked; mode
is recorded as `context_json.stdout_as_data=true`. Do not use this flag for
builds, tests, runtime or MCP/Ghidra results. Default classification is unchanged.
Reprinting compiler lines or regex literals in default mode can create observed
diagnostics; resolve them explicitly after checking their source, never erase history.

`run` preserves child exit codes, records spawn failures, timeouts and signals,
and also detects explicit/structured failures at exit zero. A successful command
is `not_checked` until its result is actually validated. Output is streamed into
compressed SQLite chunks; operation rows retain at most 64 KiB per stream as a
preview. Recognized credentials are redacted before storage/passthrough. To bound
memory, a single unbroken line exceeding 1 MiB is explicitly replaced by
`[oversized log line redacted]`; this is not lossless capture of that line.

```bash
node tools/decomp-logging/cli.mjs task runtime_l3 running 'Linux/Windows host runtime'
node tools/decomp-logging/cli.mjs run --task runtime_l3 --stage build --timeout-ms 300000 -- COMMAND ARGS
node tools/decomp-logging/cli.mjs run --retry-of EVENT_ID --action 'Specific correction' -- COMMAND ARGS
node tools/decomp-logging/cli.mjs verify OPERATION_ID 'What was actually checked'
node tools/decomp-logging/cli.mjs resolve --by VERIFIED_OPERATION --action 'Evidence for this additional resolution' EVENT_ID
node tools/decomp-logging/cli.mjs output OPERATION_ID stderr
```

Recovery is explicit, never an automatic retry loop. The default limit is three
attempts per failure fingerprint within the original event's workflow stage
(`--max-retries`); an attempt is linked to the
original error and its new operation. Only verification of a successful retry
marks the linked diagnostic resolved. This does not automatically prove that
other errors from the failed operation were resolved.

For multi-command sessions, run `session start LABEL`, set the returned ID in
`XBOXDECOMP_SESSION_ID`, and run `session finish ID` to list remaining errors.
Without an explicit session, each invocation closes its own operation session.

## Existing logs and deletion

```bash
node tools/decomp-logging/cli.mjs import-logs
node tools/decomp-logging/cli.mjs cleanup
node tools/decomp-logging/cli.mjs cleanup --apply
node tools/decomp-logging/cli.mjs restore logs/PATH.log
node tools/decomp-logging/cli.mjs restore logs/PATH.log tmp/restored-PATH.log
```

The importer reads `logs/` recursively, excludes SQLite files/sidecars and the
backup directory, and skips symlinks. Existing text outputs and small binary
evidence dumps are archived in compressed chunks. Each snapshot has source and
stored SHA-256; a changed/redacted file is labelled and is ineligible for cleanup.
Known diagnostics found in historical files are recorded as observed history,
with source path and line. Their event time is ingestion time; `status` counts these separately and `errors` includes them only with `--include-history` or `--stage historical_import`. Original tool
execution/semantic validation is not inferred.

Import is idempotent for unchanged snapshots. Content deduplication is by checksum;
every imported snapshot is verified by reading/decompressing it from SQLite.
The original files remain until a separate cleanup step.

Cleanup considers only exact duplicate `.log`, `.txt`, `.json`, `.jsonl` files.
It retains a canonical source file and all files registered as artifacts or referenced by root/docs/log
Markdown (including basename/directory mentions). It excludes files changed in
the last five minutes and checks source SHA-256, size and mtime before deleting.
Unique files, source code and original media are excluded. Each deletion has a
persistent row in `artifacts.sqlite3:cleanup` and can be restored from the archive.
Restore verifies content and refuses to overwrite an existing file.

If SQLite is unavailable, the CLI exits unsuccessfully and writes a sanitized
`logs/sqlite-emergency.jsonl` record. `init` imports that record once persistence
is restored. The original failure is not silently ignored.

## Validation

```bash
node --test tools/decomp-logging/*.test.mjs
```

Tests use isolated workspaces. They cover missing binary, invalid/unsupported
instruction, structured tool failure at exit zero, nonzero subprocess, actual
compiler failure, failed recovery, verified corrected retry, retry bounds and
SQLite write lock. Additional checks cover complete large-output recovery,
credential redaction, legacy migration, emergency replay, archive idempotence,
checksum-preserving restore and protection of referenced/unique evidence.


Verified checkpoint: final eight test groups passed in operation
`3a250564-b1f1-45eb-bc51-73458ae017af`, including concurrent first initialization.
Initial bulk import archived 294 existing files (4,403,652,210 source bytes);
pre-upgrade history preserved 303 events, 113 operations, 84 artifacts and the
remaining session/function/recovery records. Three unreferenced duplicate logs (two empty and one duplicate help output)
were deleted after archive verification. See [validation.json](../logs/sqlite-control/validation.json)
and [cleanup.json](../logs/sqlite-control/cleanup.json). Validation was performed
on Linux; Windows execution of the logger was not tested.

2026-10-21 correction: generic `Command failed` fingerprints from unrelated
stages previously shared the retry budget and blocked an independent header
inspection correction. Retry counting now includes the original event's stage,
preserving all stored fingerprints and IDs. Full nine-group suite passes in
operation `82c3fa67-761b-4139-a58b-0f1747456b7b`; the new group proves independent
stages do not collide while repeated failures in the same stage remain bounded.
The previously blocked header correction then succeeded in operation
`7aa09384-46a4-4bf8-a594-93112e01f2b5` and its actual declaration was verified.

Namespace-qualified C++ frames such as `rex::Error`/`rex::Warning` are excluded from colon diagnostic matching; real `file:line:column: error:` still fails an exit-zero operation. All nine integration groups passed, verified operation `39e25242-5e5e-47c9-9f6c-a251d1072cff`.

2026-10-22: explicit stdout-data mode fixes source-listing false positives
without weakening default tool checks. node --test: 10 tests, 10 pass, 0 fail, operation c2bfac88-7367-4964-8865-9bb0cd363956. The new group verifies exact
output round-trip, persisted mode, default source-literal detection, real stderr
diagnostics, nonzero exit/spawn failures and default structured-tool detection.
