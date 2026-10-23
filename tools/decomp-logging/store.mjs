import Database from 'better-sqlite3';
import {readFileSync,mkdirSync,appendFileSync,existsSync} from 'node:fs';
import path from 'node:path';
import {randomUUID,createHash} from 'node:crypto';
import {deflateRawSync,inflateRawSync} from 'node:zlib';

export const workspaceRoot=()=>path.resolve(process.env.XBOXDECOMP_WORKSPACE || path.join(import.meta.dirname,'../..'));
export const now=()=>new Date().toISOString();
export const redact=value=>String(value ?? '').replace(/((?:token|password|secret|api[_-]?key|authorization)["']?\s*[=:]\s*["']?)([^\s,;"']+)/gi,'$1[REDACTED]').replace(/(Bearer\s+)\S+/gi,'$1[REDACTED]').replace(/(https?:\/\/)[^\s/@]+:[^\s/@]+@/gi,'$1[REDACTED]@');
export function redactArgs(args){let hide=false;return args.map(arg=>{if(hide){hide=false;return '[REDACTED]';}if(/^--(?:token|password|secret|api[-_]?key|authorization)$/i.test(arg))hide=true;return redact(arg);});}
export const fingerprint=e=>createHash('sha256').update(JSON.stringify([e.tool,e.input_path,e.original_va,e.error_category,e.error_code,e.message])).digest('hex');
export function emergency(workspace,error,context={}) {
 const record={event_id:randomUUID(),timestamp_utc:now(),severity:'ERROR',workflow_stage:'sqlite_logging',error_code:error.code || null,message:redact(error.message),stack_trace:redact(error.stack),...context};
 process.stderr.write(JSON.stringify(record)+'\n');
 try {mkdirSync(path.join(workspace,'logs'),{recursive:true});appendFileSync(path.join(workspace,'logs/sqlite-emergency.jsonl'),JSON.stringify(record)+'\n');} catch(e){process.stderr.write('Emergency persistence failed: '+e.message+'\n');}
}
export class Store {
 constructor(workspace=workspaceRoot()) {
  this.workspace=workspace;const log=path.join(workspace,'logs');mkdirSync(log,{recursive:true});
  this.paths={workflow:path.join(log,'decomp.sqlite3'),errors:path.join(log,'errors.sqlite3'),output:path.join(log,'output.sqlite3'),artifacts:path.join(log,'artifacts.sqlite3')};
  try {
   this.db=new Database(this.paths.workflow,{timeout:Number(process.env.XBOXDECOMP_BUSY_MS || 5000)});
   this.db.pragma('foreign_keys=ON');this.db.pragma('journal_mode=WAL');
   this.db.exec('CREATE TABLE IF NOT EXISTS schema_migrations(version INTEGER PRIMARY KEY, applied_at TEXT NOT NULL)');
   for(const [v,file] of [[1,'001-initial.sql'],[2,'002-control.sql']]) {
    this.db.transaction(()=>{if(this.db.prepare('SELECT 1 FROM schema_migrations WHERE version=?').get(v))return;this.db.exec(readFileSync(new URL('./migrations/'+file,import.meta.url),'utf8'));this.db.prepare('INSERT INTO schema_migrations VALUES(?,?)').run(v,now());}).immediate();
   }
   for(const [alias,key] of [['diag','errors'],['out','output'],['catalog','artifacts']]) {
    this.db.prepare(`ATTACH DATABASE ? AS ${alias}`).run(this.paths[key]);this.db.pragma(`${alias}.journal_mode=WAL`);
    // Migrations for separate stores execute against their own connection.
    const other=new Database(this.paths[key],{timeout:5000});try {
     other.exec('CREATE TABLE IF NOT EXISTS schema_migrations(version INTEGER PRIMARY KEY, applied_at TEXT NOT NULL)');
     other.transaction(()=>{if(other.prepare('SELECT 1 FROM schema_migrations WHERE version=1').get())return;other.exec(readFileSync(new URL('./migrations/'+key+'-001.sql',import.meta.url),'utf8'));other.prepare('INSERT INTO schema_migrations VALUES(1,?)').run(now());}).immediate();
    } finally {other.close();}
   }
   this.reconcile();
  } catch(e) {this.db?.close();throw e;}
 }
 reconcile(eventId=null) {
  // The main DB is the durable event/metadata index. Rebuild mirrors by ID after interruption.
  const sql=`INSERT OR IGNORE INTO diag.diagnostics(id,timestamp_utc,session_id,operation_id,severity,stage,tool,input_path,original_va,category,code,message,stack_trace,context_json,fingerprint,outcome,validation_status)
   SELECT id,timestamp_utc,session_id,operation_id,severity,stage,tool,input_path,original_va,error_category,error_code,message,stack_trace,context_json,fingerprint,outcome,validation_status FROM main.events WHERE severity IN ('WARNING','ERROR','CRITICAL')`;
  if(eventId)this.db.prepare(sql+' AND id=?').run(eventId);
  else {
   this.db.exec(sql);this.db.exec('INSERT OR IGNORE INTO catalog.records SELECT * FROM main.artifacts;');
   this.db.exec(`UPDATE diag.diagnostics SET resolved_by=(SELECT r.operation_id FROM main.recovery_attempts r JOIN main.operations o ON o.id=r.operation_id WHERE r.event_id=diagnostics.id AND r.outcome='succeeded' AND r.validation_status='verified' AND o.status='succeeded' AND o.validation_status='verified' ORDER BY r.attempted_at DESC LIMIT 1) WHERE resolved_by IS NULL AND id IN (SELECT r.event_id FROM main.recovery_attempts r JOIN main.operations o ON o.id=r.operation_id WHERE r.outcome='succeeded' AND r.validation_status='verified' AND o.status='succeeded' AND o.validation_status='verified');`);
  }
 }
 session(id=null) {
  if(id){if(!this.db.prepare('SELECT id FROM sessions WHERE id=? AND ended_at IS NULL').get(id))throw new Error('Unknown or closed session: '+id);return id;}
  id=randomUUID();this.db.prepare('INSERT INTO sessions(id,started_at,workspace,environment_json,outcome) VALUES(?,?,?,?,?)').run(id,now(),this.workspace,JSON.stringify({node:process.version,platform:process.platform}),'running');return id;
 }
 endSession(id,outcome) {this.db.prepare('UPDATE sessions SET ended_at=?,outcome=? WHERE id=?').run(now(),outcome,id);}
 event(session,data) {
  const allowed=['id','timestamp_utc','operation_id','function_id','severity','stage','tool','operation','input_path','function_name','original_va','error_category','error_code','message','stack_trace','exit_code','stdout','stderr','context_json','fingerprint','recovery_action','outcome','validation_status'];
  const e={id:randomUUID(),timestamp_utc:now(),session_id:session};
  for(const k of allowed)if(data[k]!==undefined)e[k]=typeof data[k]==='string'?redact(data[k]):data[k];
  if(e.context_json && typeof e.context_json!=='string')e.context_json=redact(JSON.stringify(e.context_json));
  if(['ERROR','CRITICAL','WARNING'].includes(e.severity))e.fingerprint ||= fingerprint(e);
  const cols=Object.keys(e);this.db.prepare(`INSERT OR IGNORE INTO events(${cols.join(',')}) VALUES(${cols.map(()=>'?').join(',')})`).run(...cols.map(k=>e[k] ?? null));
  if(['ERROR','CRITICAL','WARNING'].includes(e.severity))this.reconcile(e.id);return e.id;
 }
 stream(operation,name,{legacy=false}={}) {
  const id=randomUUID(),hash=createHash('sha256');let seq=0,bytes=0,pending=[],pendingBytes=0;
  this.db.prepare('INSERT INTO out.streams(id,operation_id,name,legacy_preview_only) VALUES(?,?,?,?)').run(id,operation,name,Number(legacy));
  const flush=()=>{if(!pendingBytes)return;const data=Buffer.concat(pending,pendingBytes);this.db.prepare('INSERT INTO out.chunks VALUES(?,?,?,?)').run(id,seq++,data.length,deflateRawSync(data));this.db.prepare('UPDATE out.streams SET bytes=? WHERE id=?').run(bytes,id);pending=[];pendingBytes=0;};
  return {id,write:buffer=>{if(!buffer.length)return;bytes+=buffer.length;hash.update(buffer);pending.push(buffer);pendingBytes+=buffer.length;if(pendingBytes>=65536)flush();},finish:()=>{flush();this.db.prepare('UPDATE out.streams SET bytes=?,sha256=?,complete=? WHERE id=?').run(bytes,hash.digest('hex'),Number(!legacy),id);}};
 }
 importLegacy() {
  let streams=0;
  for(const op of this.db.prepare('SELECT id,stdout,stderr FROM operations').all()) for(const name of ['stdout','stderr']) {
   if(op[name]===null || this.db.prepare('SELECT 1 FROM out.streams WHERE operation_id=? AND name=?').get(op.id,name))continue;
   const stream=this.stream(op.id,name,{legacy:true});stream.write(Buffer.from(redact(op[name])));stream.finish();streams++;
  }
  const emergencyPath=path.join(this.workspace,'logs/sqlite-emergency.jsonl');let imported=0;
  if(existsSync(emergencyPath)){
   const session=this.session();try {
    for(const [index,line] of readFileSync(emergencyPath,'utf8').split('\n').entries())if(line.trim()) {
     let e;try {e=JSON.parse(line);}catch {e={message:line,error_category:'malformed_emergency_record'};}
     const id=e.event_id || createHash('sha256').update(index+':'+line).digest('hex');
     if(this.db.prepare('SELECT 1 FROM events WHERE id=?').get(id))continue;
     this.event(session,{id,timestamp_utc:e.timestamp_utc,severity:e.severity || 'ERROR',stage:e.workflow_stage || 'sqlite_logging',tool:e.tool_name,operation:e.operation,message:e.message,error_category:e.error_category,error_code:e.error_code,stack_trace:e.stack_trace,context_json:e,outcome:'observed'});imported++;
    }
   }finally {this.endSession(session,'succeeded');}
  }
  return {streams_imported:streams,emergency_imported:imported};
 }
 task(id,title,status,notes='') {
  if(!['pending','running','blocked','done'].includes(status))throw new Error('Invalid task status');
  this.db.prepare('INSERT INTO tasks VALUES(?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET title=excluded.title,status=excluded.status,notes=excluded.notes,updated_at=excluded.updated_at').run(id,title,status,notes,now());
 }
 verify(operation,note) {
  const op=this.db.prepare('SELECT * FROM operations WHERE id=?').get(operation);if(!op || op.status!=='succeeded')throw new Error('Only successful operations can be verified');
  this.db.prepare("UPDATE operations SET validation_status='verified' WHERE id=?").run(operation);
  this.db.prepare("UPDATE recovery_attempts SET validation_status='verified' WHERE operation_id=? AND outcome='succeeded'").run(operation);
  this.db.prepare(`UPDATE diag.diagnostics SET resolved_by=? WHERE id IN (SELECT event_id FROM main.recovery_attempts WHERE operation_id=? AND outcome='succeeded')`).run(operation,operation);
  this.event(op.session_id,{operation_id:operation,severity:'INFO',stage:op.stage,tool:op.tool,operation:'verify',message:note,outcome:'succeeded',validation_status:'verified'});
 }
 resolve(eventId,operation,action) {
  const op=this.db.prepare("SELECT * FROM operations WHERE id=? AND status='succeeded' AND validation_status='verified'").get(operation);
  const error=this.db.prepare("SELECT id FROM events WHERE id=? AND severity IN ('ERROR','CRITICAL')").get(eventId);
  if(!op||!error||!action)throw new Error('Resolution requires an error, a verified successful operation and an explanatory action');
  if(this.db.prepare('SELECT resolved_by FROM diag.diagnostics WHERE id=?').get(eventId)?.resolved_by===operation)return;
  const n=this.db.prepare('SELECT count(*) AS n FROM recovery_attempts WHERE event_id=?').get(eventId).n+1;
  this.db.prepare('INSERT INTO recovery_attempts(id,event_id,operation_id,attempted_at,action,attempt_number,outcome,validation_status) VALUES(?,?,?,?,?,?,?,?)').run(randomUUID(),eventId,operation,now(),redact(action),n,'succeeded','verified');
  this.db.prepare('UPDATE diag.diagnostics SET resolved_by=? WHERE id=?').run(operation,eventId);
  this.event(op.session_id,{operation_id:operation,severity:'INFO',stage:op.stage,tool:op.tool,operation:'resolve',message:action,context_json:{resolved_event:eventId},outcome:'succeeded',validation_status:'verified'});
 }
 status() {
  this.reconcile();return {databases:this.paths,wal:this.db.pragma('journal_mode',{simple:true}),foreign_keys:this.db.pragma('foreign_keys',{simple:true}),schema:this.db.prepare('SELECT * FROM schema_migrations').all(),events:this.db.prepare('SELECT severity,count(*) AS count FROM events GROUP BY severity').all(),operations:this.db.prepare('SELECT status,count(*) AS count FROM operations GROUP BY status').all(),tasks:this.db.prepare('SELECT * FROM tasks ORDER BY updated_at DESC').all(),recent_errors:this.errors({limit:10}),historical_diagnostics:this.db.prepare("SELECT severity,count(*) AS count FROM diag.diagnostics WHERE stage='historical_import' GROUP BY severity").all(),archive:this.db.prepare('SELECT count(*) AS files,coalesce(sum(source_bytes),0) AS source_bytes FROM catalog.files').get(),stream_chunks:this.db.prepare('SELECT count(*) AS count FROM out.chunks').get().count};
 }
 errors({limit=20,va=null,stage=null,tool=null,includeHistory=false}={}) {
  return this.db.prepare(`SELECT * FROM diag.diagnostics WHERE severity IN ('ERROR','CRITICAL') AND resolved_by IS NULL AND (?=1 OR stage IS NULL OR stage<>'historical_import') AND (? IS NULL OR original_va=?) AND (? IS NULL OR stage=?) AND (? IS NULL OR tool=?) ORDER BY timestamp_utc DESC LIMIT ?`).all(Number(includeHistory || stage==='historical_import'),va,va,stage,stage,tool,tool,Number(limit));
 }
 *output(operation,name) {
  const stream=this.db.prepare('SELECT * FROM out.streams WHERE operation_id=? AND name=?').get(operation,name);if(!stream)throw new Error('No captured stream');
  for(const row of this.db.prepare('SELECT data FROM out.chunks WHERE stream_id=? ORDER BY seq').iterate(stream.id))yield inflateRawSync(row.data);
 }
 close(){this.db.close();}
}
