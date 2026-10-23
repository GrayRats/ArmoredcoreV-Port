import {createReadStream,existsSync,lstatSync,readdirSync,readFileSync,statSync,unlinkSync,openSync,writeSync,closeSync,mkdirSync,realpathSync} from 'node:fs';
import path from 'node:path';
import {createHash,randomUUID} from 'node:crypto';
import {deflateRawSync,inflateRawSync} from 'node:zlib';
import {SafeText,diagnostic} from './capture.mjs';
import {now} from './store.mjs';
const TEXT=new Set(['.log','.txt','.json','.jsonl','.asm','.toml','.md','.tsv','.xml','.patch']);
const SKIP=/\.(?:sqlite3?|db)(?:-(?:wal|shm|journal))?$|^\.gitkeep$/i;
export function filesUnder(directory){
 const result=[];
 for(const e of readdirSync(directory,{withFileTypes:true})){
  const p=path.join(directory,e.name);if(e.isSymbolicLink())continue;
  if(e.isDirectory()){if(!['sqlite-backups'].includes(e.name))result.push(...filesUnder(p));}
  else if(e.isFile() && !SKIP.test(e.name))result.push(p);
 }return result.sort();
}
function logPath(store,relative){
 const p=path.resolve(store.workspace,relative),root=path.join(store.workspace,'logs')+path.sep;
 if(!p.startsWith(root))throw new Error('Archive path must be inside workspace/logs');
 if(existsSync(p) && (!realpathSync(p).startsWith(root)||lstatSync(p).isSymbolicLink()))throw new Error('Symlinks/outside paths are excluded');return p;
}
export function verifyContent(store,id,expected=null){
 const row=store.db.prepare('SELECT * FROM out.contents WHERE id=? AND complete=1').get(id);if(!row)throw new Error('Archive content is incomplete');
 const h=createHash('sha256');let bytes=0,seq=0;
 for(const c of store.db.prepare('SELECT * FROM out.content_chunks WHERE content_id=? ORDER BY seq').iterate(id)){
  if(c.seq!==seq++)throw new Error('Missing archive chunk');const raw=inflateRawSync(c.data);if(raw.length!==c.raw_size)throw new Error('Wrong chunk size');h.update(raw);bytes+=raw.length;
 }
 const sha=h.digest('hex');if(sha!==row.sha256 || bytes!==row.bytes || (expected && sha!==expected))throw new Error('Archive checksum mismatch');return {sha256:sha,bytes};
}
async function hashFile(file){const h=createHash('sha256');for await(const b of createReadStream(file))h.update(b);return h.digest('hex');}
export async function importLogs(store){
 const run=randomUUID(),session=store.session(),started=now();store.db.prepare('INSERT INTO archive_runs(id,started_at,status) VALUES(?,?,?)').run(run,started,'running');
 const report={run,imported:0,unchanged:0,failed:0,source_bytes:0,stored_bytes:0,redacted_or_changed:0,diagnostics:0};
 try {
  const files=filesUnder(path.join(store.workspace,'logs'));
  for(const file of files){
   const relative=path.relative(store.workspace,file),before=statSync(file);
   const cached=store.db.prepare('SELECT 1 FROM catalog.files WHERE path=? AND source_bytes=? AND mtime_ms=? AND verified_at IS NOT NULL').get(relative,before.size,before.mtimeMs);
   if(cached){report.unchanged++;continue;}
   const id=randomUUID(),sourceHash=createHash('sha256'),storedHash=createHash('sha256');let seq=0,stored=0,pending=[],pendingBytes=0,sourceBytes=0,lineNumber=0;
   const messages=new Map();
   store.db.prepare('INSERT INTO out.contents(id,bytes,sha256,complete) VALUES(?,0,?,0)').run(id,'pending:'+id);
   const flush=()=>{if(!pendingBytes)return;const b=Buffer.concat(pending,pendingBytes);storedHash.update(b);stored+=b.length;store.db.prepare('INSERT INTO out.content_chunks VALUES(?,?,?,?)').run(id,seq++,b.length,deflateRawSync(b,{level:1}));pending=[];pendingBytes=0;};
   const append=buffer=>{pending.push(buffer);pendingBytes+=buffer.length;if(pendingBytes>=1024*1024)flush();};
   const emit=text=>{append(Buffer.from(text));lineNumber++;const severity=diagnostic(text);if(severity){const message=text.trim().slice(0,8192);if(!messages.has(message))messages.set(message,{severity,message,line:lineNumber});}};
   const safe=TEXT.has(path.extname(file).toLowerCase())?new SafeText(emit):null;
   try {
    if(before.size)for await(const b of createReadStream(file,{end:before.size-1})){sourceHash.update(b);sourceBytes+=b.length;if(safe)safe.push(b);else append(b);}
    safe?.end();flush();const sourceSha=sourceHash.digest('hex'),storedSha=storedHash.digest('hex'),after=statSync(file);
    const changed=before.size!==after.size || before.mtimeMs!==after.mtimeMs;
    const transformed=sourceSha!==storedSha || changed;
    const existing=store.db.prepare('SELECT * FROM out.contents WHERE sha256=? AND complete=1').get(storedSha);let content=id;
    if(existing){content=existing.id;store.db.prepare('DELETE FROM out.contents WHERE id=?').run(id);}else store.db.prepare('UPDATE out.contents SET sha256=?,bytes=?,complete=1 WHERE id=?').run(storedSha,stored,id);
    // Verify every new snapshot by decompressing the persisted SQLite chunks.
    verifyContent(store,content,storedSha);
    store.db.prepare('INSERT INTO catalog.files(id,path,source_sha256,stored_sha256,content_id,source_bytes,stored_bytes,transformed,mtime_ms,imported_at,verified_at) VALUES(?,?,?,?,?,?,?,?,?,?,?) ON CONFLICT(path,source_sha256) DO UPDATE SET content_id=excluded.content_id,stored_sha256=excluded.stored_sha256,stored_bytes=excluded.stored_bytes,transformed=excluded.transformed,mtime_ms=excluded.mtime_ms,verified_at=excluded.verified_at').run(randomUUID(),relative,sourceSha,storedSha,content,sourceBytes,stored,Number(transformed),before.mtimeMs,now(),now());
    for(const m of messages.values()){
     const eventId=createHash('sha256').update('archive:'+sourceSha+':'+m.severity+':'+m.message).digest('hex');
     const exists=store.db.prepare('SELECT 1 FROM events WHERE id=?').get(eventId);
     if(!exists){store.event(session,{id:eventId,severity:m.severity,stage:'historical_import',tool:'log_archive',operation:'import_diagnostic',input_path:relative,error_category:'historical_tool_diagnostic',message:m.message,context_json:{source_sha256:sourceSha,line:m.line,source_time:'unknown; recorded time is ingestion time'},outcome:'observed'});report.diagnostics++;}
    }
    report.imported++;report.source_bytes+=sourceBytes;report.stored_bytes+=stored;if(transformed)report.redacted_or_changed++;
    process.stderr.write(`[archive ${report.imported}/${files.length} ${relative} bytes=${sourceBytes}]\n`);
   }catch(e){report.failed++;store.event(session,{severity:'ERROR',stage:'archive',tool:'log_archive',operation:'import_file',input_path:relative,error_category:'archive_failure',message:e.message,stack_trace:e.stack,outcome:'failed'});}
  }
  store.db.prepare('UPDATE archive_runs SET ended_at=?,status=?,report_json=? WHERE id=?').run(now(),report.failed?'failed':'succeeded',JSON.stringify(report),run);store.endSession(session,report.failed?'failed':'succeeded');return report;
 }catch(e){store.db.prepare('UPDATE archive_runs SET ended_at=?,status=?,report_json=? WHERE id=?').run(now(),'failed',JSON.stringify({error:e.message,...report}),run);store.endSession(session,'failed');throw e;}
}
function referenceText(store){
 const roots=[store.workspace,path.join(store.workspace,'docs'),path.join(store.workspace,'logs')],texts=[];
 const scan=(directory,recursive)=>{if(!existsSync(directory))return;for(const e of readdirSync(directory,{withFileTypes:true})){if(e.isSymbolicLink())continue;const p=path.join(directory,e.name);if(e.isDirectory()&&recursive)scan(p,true);else if(e.isFile()&&/\.(?:md|MD)$/.test(e.name))texts.push(readFileSync(p,'utf8'));}};
 scan(roots[0],false);scan(roots[1],true);scan(roots[2],true);return texts.join('\n');
}
export async function cleanup(store,{apply=false,minAgeMs=300000}={}){
 const reference=referenceText(store),groups=new Map(),report={apply,deleted:[],candidates:[],protected:0};
 const evidencePaths=new Set(store.db.prepare('SELECT path FROM catalog.records').all().map(r=>r.path));
 const rows=store.db.prepare('SELECT * FROM catalog.files WHERE transformed=0 AND source_sha256=stored_sha256 AND verified_at IS NOT NULL ORDER BY imported_at DESC').all();
 const visited=new Set();
 const protectedPath=relative=>{
  if(!/\.(?:log|txt|json|jsonl)$/i.test(relative))return true;
  const parent=path.dirname(relative),base=path.basename(relative);
  return evidencePaths.has(relative)||reference.includes(relative)||reference.includes(base)||(parent!=='logs'&&reference.includes(parent));
 };
 for(const row of rows){if(visited.has(row.path))continue;visited.add(row.path);const file=logPath(store,row.path);if(!existsSync(file)||lstatSync(file).isSymbolicLink())continue;const stat=statSync(file);if(stat.size!==row.source_bytes || stat.mtimeMs!==row.mtime_ms || Date.now()-stat.mtimeMs<minAgeMs)continue;
  row.protected=protectedPath(row.path);if(row.protected)report.protected++;if(!groups.has(row.source_sha256))groups.set(row.source_sha256,[]);groups.get(row.source_sha256).push(row);
 }
 for(const [sha,group]of groups){if(group.length<2)continue;
  group.sort((a,b)=>Number(b.protected)-Number(a.protected)||a.path.localeCompare(b.path));const keep=group[0],keepFile=logPath(store,keep.path);
  if(await hashFile(keepFile)!==sha)continue;
  for(const row of group.slice(1)){
   if(row.protected)continue;const file=logPath(store,row.path);if(await hashFile(file)!==sha)continue;
   verifyContent(store,row.content_id,sha);
   const entry={path:row.path,kept:keep.path,sha256:sha,bytes:row.source_bytes};report.candidates.push(entry);
   if(!apply)continue;
   const id=randomUUID();store.db.prepare('INSERT INTO catalog.cleanup(id,path,kept_path,source_sha256,content_id,planned_at,status,reason) VALUES(?,?,?,?,?,?,?,?)').run(id,row.path,keep.path,sha,row.content_id,now(),'planned','exact_duplicate; SHA-256 verified against SQLite and retained file; no documentation reference');
   // Compare file identity and mtime once more immediately before unlink.
   const last=lstatSync(file);if(last.isSymbolicLink()||last.size!==row.source_bytes||last.mtimeMs!==row.mtime_ms)throw new Error('Cleanup source changed: '+row.path);
   unlinkSync(file);store.db.prepare("UPDATE catalog.cleanup SET status='deleted',deleted_at=? WHERE id=?").run(now(),id);report.deleted.push(entry);
  }
 }return report;
}
export function restore(store,relative,output=null){
 const row=store.db.prepare('SELECT * FROM catalog.files WHERE path=? ORDER BY imported_at DESC LIMIT 1').get(relative);if(!row)throw new Error('No archived file: '+relative);
 const file=output?path.resolve(store.workspace,output):logPath(store,relative);
 if(!['logs','tmp'].some(d=>file.startsWith(path.join(store.workspace,d)+path.sep)))throw new Error('Restore output must be inside workspace logs/ or tmp/');
 verifyContent(store,row.content_id,row.stored_sha256);mkdirSync(path.dirname(file),{recursive:true});
 const parent=realpathSync(path.dirname(file));if(!['logs','tmp'].some(d=>parent===path.join(store.workspace,d)||parent.startsWith(path.join(store.workspace,d)+path.sep)))throw new Error('Restore parent resolves outside allowed directory');
 const fd=openSync(file,'wx');try {for(const c of store.db.prepare('SELECT data FROM out.content_chunks WHERE content_id=? ORDER BY seq').iterate(row.content_id)){
  const b=inflateRawSync(c.data);let n=0;while(n<b.length)n+=writeSync(fd,b,n);
 }}catch(e){closeSync(fd);unlinkSync(file);throw e;}closeSync(fd);
 store.db.prepare("UPDATE catalog.cleanup SET restored_at=? WHERE path=? AND status='deleted'").run(now(),relative);
 return {path:path.relative(store.workspace,file),sha256:row.stored_sha256,bytes:row.stored_bytes,transformed:!!row.transformed};
}
