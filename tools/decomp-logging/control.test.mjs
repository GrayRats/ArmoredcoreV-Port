import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtempSync,mkdirSync,readFileSync,writeFileSync,existsSync,rmSync,utimesSync} from 'node:fs';
import path from 'node:path';
import {spawnSync,spawn} from 'node:child_process';
import {createHash} from 'node:crypto';
import {inflateRawSync} from 'node:zlib';
import Database from 'better-sqlite3';
import {Store} from './store.mjs';
const cli=path.join(import.meta.dirname,'cli.mjs'),root=path.resolve(import.meta.dirname,'../..');
function fixture(){
 const workspace=mkdtempSync(path.join(root,'tmp/logging-test-'));mkdirSync(path.join(workspace,'logs'));mkdirSync(path.join(workspace,'docs'));
 const run=(...argv)=>spawnSync(process.execPath,[cli,...argv],{cwd:workspace,encoding:'utf8',maxBuffer:8*1024*1024,env:{...process.env,XBOXDECOMP_WORKSPACE:workspace,XBOXDECOMP_SESSION_ID:'',XBOXDECOMP_LOGGING_ACTIVE:''}});
 const db=name=>new Database(path.join(workspace,'logs/'+name+'.sqlite3'),{readonly:true});
 return {workspace,run,db,dispose:()=>rmSync(workspace,{recursive:true,force:true})};
}
test('explicit stdout data mode preserves listings and still detects stderr and process failures',()=>{
 const f=fixture();try {
  const listing=String.raw`170: key = re.sub(r"^.*?\[(?:error|critical|fatal)\]\s*", "", message)`;
  const script='console.log('+JSON.stringify(listing)+')';
  assert.equal(f.run('run',process.execPath,'-e',script).status,1,'default classifier remains strict');
  assert.equal(f.run('run','--stdout-data','--',process.execPath,'-e',script).status,0);
  let op;const db=f.db('decomp');try {
   op=db.prepare("SELECT * FROM operations WHERE status='succeeded' ORDER BY started_at DESC LIMIT 1").get();
   assert.equal(JSON.parse(op.context_json).stdout_as_data,true);
  }finally{db.close();}
  assert.equal(f.run('output',op.id,'stdout').stdout,listing+'\n');
  assert.equal(f.run('run','--stdout-data','--',process.execPath,'-e','console.error("sample.cpp:4: error: actual diagnostic")').status,1);
  assert.equal(f.run('run','--stdout-data','--',process.execPath,'-e','process.exit(7)').status,7);
  assert.equal(f.run('run','--stdout-data','--','__missing_source_reader__').status,127);
  assert.equal(f.run('run',process.execPath,'-e','console.log(JSON.stringify({ok:false}))').status,1,'structured tool failure stays enabled by default');
 }finally{f.dispose();}
});
test('separate stores, large streaming output, redaction, warnings and structured errors',async()=>{
 const f=fixture();try {
  const concurrentInit=()=>new Promise(resolve=>{const child=spawn(process.execPath,[cli,'init'],{env:{...process.env,XBOXDECOMP_WORKSPACE:f.workspace,XBOXDECOMP_SESSION_ID:'',XBOXDECOMP_LOGGING_ACTIVE:''},stdio:['ignore','pipe','pipe']});let stderr='';child.stderr.on('data',b=>stderr+=b);child.on('close',status=>resolve({status,stderr}));});
  for(const r of await Promise.all([concurrentInit(),concurrentInit(),concurrentInit()]))assert.equal(r.status,0,r.stderr);
  for(const name of ['decomp','errors','output','artifacts'])assert.ok(existsSync(path.join(f.workspace,'logs/'+name+'.sqlite3')));
  assert.equal(f.run('run',process.execPath,'-e','for(let i=0;i<4000;i++)console.log("row "+i+" "+"x".repeat(80)); console.error("WARNING: sample"); console.log("token=split"+"_secret"); console.log(JSON.stringify({errors:0}));').status,0);
  let op;const db=f.db('decomp');try {op=db.prepare('SELECT * FROM operations ORDER BY started_at DESC LIMIT 1').get();assert.equal(op.validation_status,'not_checked');assert.ok(op.stdout.length<=65536);assert.equal(db.prepare("SELECT count(*) n FROM sessions WHERE outcome='running'").get().n,0);}finally{db.close();}
  const output=f.run('output',op.id,'stdout');assert.equal(output.status,0);assert.ok(output.stdout.length>320000);assert.match(output.stdout,/row 3999/);assert.match(output.stdout,/token=\[REDACTED\]/);assert.ok(!output.stdout.includes('split_secret'));
  assert.equal(f.run('run',process.execPath,'-e','console.log(JSON.stringify({ok:false,error:{message:"decompiler failed"}}))').status,1);
  assert.equal(f.run('run',process.execPath,'-e','console.log("Unsupported instruction at 0x1234")').status,1);
  assert.equal(f.run('run',process.execPath,'-e','console.log("#0 std::expected<void, rex::Error> rex::Warning()")').status,0,'C++ namespace-qualified types in a successful debugger stack are not diagnostics');
  assert.equal(f.run('run',process.execPath,'-e','console.log("sample.cpp:12:3: error: compiler-style diagnostic")').status,1,'single-colon compiler diagnostics still fail even at exit zero');
  const errors=f.db('errors');try{assert.ok(errors.prepare("SELECT count(*) n FROM diagnostics WHERE severity='ERROR'").get().n>=2);assert.ok(errors.prepare("SELECT count(*) n FROM diagnostics WHERE severity='WARNING'").get().n>=1);}finally{errors.close();}
 }finally{f.dispose();}
});
test('missing binary, spawn failure, compiler failure, timeout and nonzero status are persistent',()=>{
 const f=fixture();try {
  assert.equal(f.run('run',process.execPath,'-e',"require('fs').readFileSync('missing.xex')").status,1);
  assert.equal(f.run('run','__missing_decompiler__').status,127);
  assert.equal(f.run('run',process.execPath,'-e','process.exit(7)').status,7);
  assert.equal(f.run('run','--timeout-ms','40','--',process.execPath,'-e','setInterval(()=>{},1000)').status,124);
  writeFileSync(path.join(f.workspace,'invalid.cpp'),'int main( { broken;');
  const cxx=spawnSync('clang++',['--version'],{encoding:'utf8'});assert.equal(cxx.status,0,'Clang required for real compiler-failure validation');
  assert.notEqual(f.run('run','clang++','-fsyntax-only','invalid.cpp').status,0);
  const db=f.db('decomp');try {assert.equal(db.prepare("SELECT count(*) n FROM operations WHERE status='failed'").get().n,5);assert.equal(db.prepare("SELECT count(*) n FROM sessions WHERE outcome='running'").get().n,0);assert.equal(db.prepare('PRAGMA foreign_key_check').all().length,0);}finally{db.close();}
 }finally{f.dispose();}
});
test('failed recovery and successful retry require an explicit verification; retry limit enforced',()=>{
 const f=fixture();try {
  assert.equal(f.run('run','--tool','example','--va','0x8245fa20','--',process.execPath,'-e','process.exit(2)').status,2);
  const errors=JSON.parse(f.run('errors','--tool','example','--va','0x8245fa20').stdout);const source=errors[0];assert.ok(source.id);
  assert.equal(f.run('run','--retry-of',source.id,'--action','first correction','--',process.execPath,'-e','process.exit(3)').status,3);
  assert.equal(f.run('run','--retry-of',source.id,'--action','corrected input','--',process.execPath,'-e','console.log("result 42")').status,0);
  let id;const db=f.db('decomp');try {id=db.prepare("SELECT id FROM operations WHERE status='succeeded' ORDER BY started_at DESC LIMIT 1").get().id;assert.equal(db.prepare('SELECT count(*) n FROM recovery_attempts').get().n,2);}finally{db.close();}
  assert.ok(JSON.parse(f.run('errors').stdout).some(e=>e.id===source.id));
  assert.equal(f.run('resolve','--by',id,'--action','Unverified result','--',source.id).status,1);
  assert.equal(f.run('verify',id,'Checked result equals 42').status,0);
  assert.equal(f.run('resolve','--by',id,'--action','Already checked','--',source.id).status,0);
  assert.ok(!JSON.parse(f.run('errors').stdout).some(e=>e.id===source.id));
  assert.equal(f.run('run','--retry-of',source.id,'--action','bounded','--max-retries','2','--',process.execPath,'-e','process.exit(0)').status,1);
 }finally{f.dispose();}
});
test('retry bounds do not mix unrelated stages with identical generic failure fingerprints',()=>{
 const f=fixture();try {
  const source=stage=>{
   assert.equal(f.run('run','--stage',stage,'--',process.execPath,'-e','process.exit(2)').status,2);
   return JSON.parse(f.run('errors','--stage',stage).stdout).find(e=>e.category==='tool_execution');
  };
  const first=source('analysis_alpha');
  for(let i=0;i<3;i++)assert.equal(f.run('run','--retry-of',first.id,'--action','recorded correction '+i,'--',process.execPath,'-e','process.exit(0)').status,0);
  assert.equal(f.run('run','--retry-of',first.id,'--action','fourth attempt blocked','--',process.execPath,'-e','process.exit(0)').status,1);
  const second=source('analysis_beta');
  assert.equal(first.fingerprint,second.fingerprint,'fixture must reproduce the historical fingerprint collision');
  assert.equal(f.run('run','--retry-of',second.id,'--action','independent stage correction','--',process.execPath,'-e','process.exit(0)').status,0);
  const repeated=source('analysis_alpha');
  assert.equal(f.run('run','--retry-of',repeated.id,'--action','same-stage repeated failure remains bounded','--',process.execPath,'-e','process.exit(0)').status,1);
 }finally{f.dispose();}
});
test('legacy migration preserves IDs/history/artifacts and marks truncated old output honestly',()=>{
 const f=fixture();try {
  const db=new Database(path.join(f.workspace,'logs/decomp.sqlite3'));db.exec(readFileSync(path.join(import.meta.dirname,'migrations/001-initial.sql'),'utf8'));
  db.prepare('INSERT INTO schema_migrations VALUES(1,?)').run('2026-01-01');db.prepare('INSERT INTO sessions(id,started_at,workspace,outcome) VALUES(?,?,?,?)').run('session-old','2026-01-01',f.workspace,'failed');
  db.prepare('INSERT INTO operations(id,session_id,started_at,status,stdout,stderr) VALUES(?,?,?,?,?,?)').run('op-old','session-old','2026-01-01','failed','old limited stdout','');
  db.prepare('INSERT INTO events(id,timestamp_utc,session_id,operation_id,severity,message) VALUES(?,?,?,?,?,?)').run('error-old','2026-01-01','session-old','op-old','ERROR','old failure');
  db.prepare('INSERT INTO artifacts(id,session_id,path,kind,created_at) VALUES(?,?,?,?,?)').run('artifact-old','session-old','logs/old.log','evidence','2026-01-01');db.close();
  const first=f.run('init');assert.equal(first.status,0,first.stderr);assert.equal(f.run('init').status,0);
  const errors=f.db('errors');try{assert.equal(errors.prepare("SELECT count(*) n FROM diagnostics WHERE id='error-old'").get().n,1);}finally{errors.close();}
  const catalog=f.db('artifacts');try{assert.equal(catalog.prepare("SELECT count(*) n FROM records WHERE id='artifact-old'").get().n,1);}finally{catalog.close();}
  const out=f.db('output');try{assert.equal(out.prepare("SELECT legacy_preview_only FROM streams WHERE operation_id='op-old' AND name='stdout'").get().legacy_preview_only,1);assert.equal(out.prepare('SELECT count(*) n FROM streams').get().n,2);}finally{out.close();}
 }finally{f.dispose();}
});
test('database lock falls back to emergency JSONL and later import is idempotent',()=>{
 const f=fixture();try {
  assert.equal(f.run('init').status,0);const db=new Database(path.join(f.workspace,'logs/decomp.sqlite3'));db.exec('BEGIN EXCLUSIVE');
  const failed=spawnSync(process.execPath,[cli,'init'],{encoding:'utf8',env:{...process.env,XBOXDECOMP_WORKSPACE:f.workspace,XBOXDECOMP_BUSY_MS:'30'}});assert.equal(failed.status,1);assert.ok(existsSync(path.join(f.workspace,'logs/sqlite-emergency.jsonl')));db.exec('ROLLBACK');db.close();
  assert.equal(f.run('init').status,0);assert.equal(f.run('init').status,0);
  const errors=JSON.parse(f.run('errors').stdout);assert.ok(errors.some(e=>e.code==='SQLITE_BUSY'));
  const ids=errors.map(e=>e.id);assert.equal(ids.length,new Set(ids).size);
 }finally{f.dispose();}
});
test('existing logs archive losslessly, deduplicate, protect references, restore, skip redacted sources',()=>{
 const f=fixture();try {
  const content='WARNING: legacy diagnostic\r\n'+'UTF8 Ж🙂 preserved\n'.repeat(12000),old=new Date(Date.now()-600000);
  for(const name of ['keep.log','duplicate.log','registry.log']){writeFileSync(path.join(f.workspace,'logs',name),content);utimesSync(path.join(f.workspace,'logs',name),old,old);}
  writeFileSync(path.join(f.workspace,'docs/refs.md'),'Evidence: logs/keep.log\n');
  const registry=new Store(f.workspace);const sid=registry.session();registry.db.prepare('INSERT INTO artifacts(id,session_id,path,kind,created_at) VALUES(?,?,?,?,?)').run('registered-log',sid,'logs/registry.log','evidence','2026-01-01');registry.endSession(sid,'succeeded');registry.close();
  writeFileSync(path.join(f.workspace,'logs/secret.log'),'token=do_not_store_this\n');
  writeFileSync(path.join(f.workspace,'logs/tool-error.json'),JSON.stringify({ok:false,error:'failed parse'}));
  writeFileSync(path.join(f.workspace,'logs/raw.bin'),Buffer.from([0,255,0,128]));
  const imported=f.run('import-logs');assert.equal(imported.status,0,imported.stderr);const report=JSON.parse(imported.stdout);assert.equal(report.failed,0);assert.equal(report.imported,6);
  const again=f.run('import-logs');assert.equal(again.status,0);assert.equal(JSON.parse(again.stdout).imported,0);
  const dry=JSON.parse(f.run('cleanup').stdout);assert.equal(dry.candidates.length,1);assert.ok(existsSync(path.join(f.workspace,'logs/duplicate.log')));
  const cleaned=f.run('cleanup','--apply');assert.equal(cleaned.status,0,cleaned.stderr);assert.equal(JSON.parse(cleaned.stdout).deleted.length,1);assert.ok(!existsSync(path.join(f.workspace,'logs/duplicate.log')));assert.ok(existsSync(path.join(f.workspace,'logs/keep.log')));assert.ok(existsSync(path.join(f.workspace,'logs/secret.log')));assert.ok(existsSync(path.join(f.workspace,'logs/registry.log')));
  const restored=f.run('restore','logs/duplicate.log');assert.equal(restored.status,0,restored.stderr);assert.equal(readFileSync(path.join(f.workspace,'logs/duplicate.log'),'utf8'),content);
  assert.equal(f.run('restore','logs/duplicate.log').status,1,'must refuse overwrite');
  const store=new Store(f.workspace);try{const row=store.db.prepare("SELECT * FROM catalog.files WHERE path='logs/raw.bin'").get();const chunks=store.db.prepare('SELECT data FROM out.content_chunks WHERE content_id=? ORDER BY seq').all(row.content_id);assert.deepEqual(Buffer.concat(chunks.map(x=>inflateRawSync(x.data))),Buffer.from([0,255,0,128]));}finally{store.close();}
 }finally{f.dispose();}
});
test('saved MCP result is recorded and control tasks survive sessions',()=>{
 const f=fixture();try {
  assert.equal(f.run('task','L3','pending','Host runtime').status,0);
  const started=JSON.parse(f.run('session','start','implementation').stdout).session;
  assert.equal(f.run('run','--session',started,'--task','L3','--',process.execPath,'-e','process.exit(4)').status,4);
  assert.equal(JSON.parse(f.run('tasks').stdout)[0].status,'blocked');
  writeFileSync(path.join(f.workspace,'mcp.json'),JSON.stringify({isError:true,content:[{type:'text',text:'backend unavailable'}]}));
  assert.equal(f.run('ingest','--tool','MCP','mcp.json').status,1);
  const finish=f.run('session','finish',started);assert.equal(finish.status,0);assert.ok(JSON.parse(finish.stdout).unresolved_errors.length>0);
 }finally{f.dispose();}
});
