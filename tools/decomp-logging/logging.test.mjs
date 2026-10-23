import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtempSync,rmSync} from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {spawnSync} from 'node:child_process';
import Database from 'better-sqlite3';
const cli=fileURLToPath(new URL('./cli.mjs',import.meta.url));
test('persistent initialization, command failures and diagnostics with exit zero',()=>{
 const workspace=mkdtempSync(path.join(path.resolve(import.meta.dirname,'../..'),'tmp/decomp-logging-'));
 const run=(...args)=>spawnSync(process.execPath,[cli,...args],{encoding:'utf8',env:{...process.env,XBOXDECOMP_WORKSPACE:workspace,XBOXDECOMP_SESSION_ID:'',XBOXDECOMP_LOGGING_ACTIVE:''}});
 try {
  assert.equal(run('init').status,0);
  assert.equal(run('run',process.execPath,'-e','process.exit(7)').status,7);
  assert.equal(run('run',process.execPath,'-e','console.log("ERROR: unsupported instruction")').status,1);
  assert.equal(run('run',process.execPath,'-e','console.log("validated command")').status,0);
  const status=run('status');
  assert.equal(status.status,0);
  const report=JSON.parse(status.stdout);
  assert.equal(report.wal,'wal'); assert.equal(report.foreign_keys,1);
  const db=new Database(path.join(workspace,'logs/decomp.sqlite3'),{readonly:true});
  try {
   assert.equal(db.prepare('SELECT count(*) AS n FROM operations').get().n,3);
   assert.equal(db.prepare("SELECT count(*) AS n FROM operations WHERE status='failed'").get().n,2);
   assert.equal(db.prepare('PRAGMA foreign_key_check').all().length,0);
   assert.equal(db.prepare("SELECT count(*) AS n FROM sessions WHERE outcome='running'").get().n,0);
  } finally {db.close();}
 } finally {rmSync(workspace,{recursive:true,force:true});}
});
