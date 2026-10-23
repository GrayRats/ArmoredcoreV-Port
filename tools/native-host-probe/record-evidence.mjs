// Register the completed probe against the command operation made by cli.mjs.
import Database from '../decomp-logging/node_modules/better-sqlite3/lib/index.js';
import {readFileSync} from 'node:fs';
import {createHash, randomUUID} from 'node:crypto';
import path from 'node:path';
import {parseArgs} from 'node:util';
const root = path.resolve(import.meta.dirname, '../..');
const {values} = parseArgs({options: {report: {type:'string'}, operation: {type:'string'}}});
const reportPath = values.report ?? 'tmp/native-host-probe/verification.json';
const reportAbsolute = path.resolve(root, reportPath);
if (!reportAbsolute.startsWith(path.join(root, 'tmp') + path.sep)) throw new Error('Report must be inside tmp/');
const report = JSON.parse(readFileSync(path.join(root, reportPath), 'utf8'));
if (report.compile.exit_code !== 0 || report.execution.exit_code !== 0 ||
    JSON.parse(report.execution.stdout).checks !== 41 ||
    JSON.parse(report.execution.stdout).generated_functions !== 2 ||
    JSON.parse(report.execution.stdout).status !== 'passed') throw new Error('Probe evidence is incomplete');
const sha = file => createHash('sha256').update(readFileSync(file)).digest('hex');
for (const [file, expected] of Object.entries(report.inputs)) {
 if (!path.resolve(root,file).startsWith(root + path.sep) || sha(path.join(root,file)) !== expected)
  throw new Error('Probe input changed since execution: ' + file);
}
const db = new Database(path.join(root, 'logs/decomp.sqlite3'));
db.pragma('foreign_keys=ON');
const operation = values.operation ? db.prepare("SELECT * FROM operations WHERE id=? AND status='succeeded'").get(values.operation) : db.prepare(`SELECT * FROM operations
 WHERE command_json = ? AND status = 'succeeded' ORDER BY started_at DESC LIMIT 1`)
 .get(JSON.stringify(['python3', 'tools/native-host-probe/run.py']));
if (!operation) throw new Error('No matching successful SQLite CLI operation');
const command = JSON.parse(operation.command_json);
if (command[0] !== 'python3' || command[1] !== 'tools/native-host-probe/run.py')
 throw new Error('Operation is not a native host probe run');
const outputIndex = command.indexOf('--output');
const expectedOutput = outputIndex >= 0 ? command[outputIndex+1] :
 (command.includes('windows') ? 'tmp/native-host-probe/windows' : 'tmp/native-host-probe');
if (path.dirname(reportAbsolute) !== path.resolve(root, expectedOutput))
 throw new Error('Operation output does not match report');
const output = path.dirname(reportPath);
const binary = path.join(output, report.target === 'windows' ? 'probe.exe' : 'probe');
if (sha(path.join(root,binary)) !== report.binary_sha256) throw new Error('Binary hash mismatch');
const files = [reportPath, ...['compile.stdout.txt','compile.stderr.txt','execution.stdout.txt',
 'execution.stderr.txt','generated_leaves.cpp'].map(file => path.join(output,file)), binary,
 'tools/native-host-probe/host.hpp', 'tools/native-host-probe/probe.cpp',
 'tools/native-host-probe/run.py'];
db.transaction(() => {
 for (const file of files) {
  db.prepare(`INSERT INTO artifacts(id,session_id,operation_id,path,kind,sha256,provenance_json,created_at)
   VALUES(?,?,?,?,?,?,?,?)`).run(randomUUID(), operation.session_id, operation.id, file,
   file.endsWith('.cpp') || file.endsWith('.hpp') ? 'native_probe_source' : 'native_probe_evidence',
   createHash('sha256').update(readFileSync(path.join(root,file))).digest('hex'),
   JSON.stringify({scope:'partial L3 ABI/memory probe',generated_functions:['0x822B0818','0x822B0820']}), new Date().toISOString());
 }
 db.prepare('UPDATE operations SET validation_status=? WHERE id=?').run('verified', operation.id);
 db.prepare(`INSERT INTO events(id,timestamp_utc,session_id,operation_id,severity,stage,tool,operation,
 message,context_json,outcome,validation_status) VALUES(?,?,?,?,?,?,?,?,?,?,?,?)`)
 .run(randomUUID(),new Date().toISOString(),operation.session_id,operation.id,'INFO','host_runtime',
 'native-host-probe','verify','41 checks passed for two generated leaves; no game boot or L4 parity',
 JSON.stringify({artifacts:files, target:report.target, execution_scope:report.execution_scope}), 'succeeded','verified');
})();
console.log(JSON.stringify({operation_id:operation.id, artifacts:files.length,validation_status:'verified'}));
db.close();
