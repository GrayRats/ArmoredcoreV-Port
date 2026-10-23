ALTER TABLE operations ADD COLUMN cwd TEXT;
ALTER TABLE operations ADD COLUMN signal TEXT;
ALTER TABLE operations ADD COLUMN context_json TEXT;
CREATE TABLE tasks(id TEXT PRIMARY KEY, title TEXT NOT NULL, status TEXT NOT NULL CHECK(status IN ('pending','running','blocked','done')), notes TEXT, updated_at TEXT NOT NULL);
CREATE TABLE operation_tasks(operation_id TEXT PRIMARY KEY REFERENCES operations(id), task_id TEXT NOT NULL REFERENCES tasks(id));
CREATE TABLE archive_runs(id TEXT PRIMARY KEY, started_at TEXT NOT NULL, ended_at TEXT, status TEXT NOT NULL, report_json TEXT);
