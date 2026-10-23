CREATE TABLE IF NOT EXISTS diagnostics(id TEXT PRIMARY KEY, timestamp_utc TEXT NOT NULL, session_id TEXT, operation_id TEXT, severity TEXT NOT NULL, stage TEXT, tool TEXT, input_path TEXT, original_va INTEGER, category TEXT, code TEXT, message TEXT, stack_trace TEXT, context_json TEXT, fingerprint TEXT, outcome TEXT, validation_status TEXT, resolved_by TEXT);
CREATE INDEX IF NOT EXISTS diagnostics_fingerprint ON diagnostics(fingerprint);
CREATE INDEX IF NOT EXISTS diagnostics_operation ON diagnostics(operation_id);
