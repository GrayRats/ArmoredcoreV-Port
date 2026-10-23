import {StringDecoder} from 'node:string_decoder';
import {spawn} from 'node:child_process';
import {randomUUID} from 'node:crypto';
import path from 'node:path';
import {now,redact,redactArgs} from './store.mjs';

// Lines are sanitized before storage or passthrough; huge unbroken lines are explicitly replaced.
export class SafeText {
 constructor(emit){this.emit=emit;this.decoder=new StringDecoder('utf8');this.pending='';this.dropping=false;this.transformed=false;}
 push(buffer){this.consume(this.decoder.write(buffer));}
 consume(text){
  for(const part of text.match(/[^\n]*\n|[^\n]+$/g) || []) {
   const ended=part.endsWith('\n');
   if(this.dropping){if(ended)this.dropping=false;continue;}
   this.pending+=part;
   if(this.pending.length>1024*1024){this.transformed=true;this.emit('[oversized log line redacted]\n');this.pending='';this.dropping=!ended;continue;}
   if(ended)this.flush();
  }
 }
 flush(){if(!this.pending)return;const safe=redact(this.pending);if(safe!==this.pending)this.transformed=true;this.emit(safe);this.pending='';}
 end(){this.consume(this.decoder.end());this.flush();}
}
export function diagnostic(line) {
 const s=line.trim();if(!s)return null;
 if(s.startsWith('{') && s.endsWith('}'))try {
  const j=JSON.parse(s);
  if(j.ok===false || j.success===false || j.isError===true || !!j.error || j.status==='failed' || ['ERROR','CRITICAL'].includes(j.severity) || (Number.isInteger(j.exit_code)&&j.exit_code!==0))return 'ERROR';
  return j.warning || j.result?.warning || j.severity==='WARNING' || (Array.isArray(j.warnings)&&j.warnings.length) ? 'WARNING':null;
 }catch{}
 const warning=/^(?:warning\b|w>|\[warning\])|(?<!:):\s*warning\b|^"warning"\s*:\s*"/i.test(s);
 const error=/^(?:error\b|fatal\b|critical\b|traceback\b|e>|\[error\])|(?<!:):\s*(?:fatal )?error\b|(?:unrecognized|unsupported|invalid) instruction|^"(?:ok|success)"\s*:\s*false|^"isError"\s*:\s*true|^"error"\s*:\s*(?:"|\{)/i.test(s);
 return error ? 'ERROR' : warning ? 'WARNING' : null;
}
export async function runCommand(store,argv,options={}) {
 if(!argv.length)throw new Error('Missing command');
 const supplied=options.session || process.env.XBOXDECOMP_SESSION_ID;
 const id=randomUUID(),start=Date.now();
 const stage=options.stage || 'external_command',tool=options.tool || path.basename(argv[0]),cwd=path.resolve(options.cwd || store.workspace);
 let retry=null;
 if(options.retryOf){
  retry=store.db.prepare('SELECT * FROM events WHERE id=? AND severity IN (\'ERROR\',\'CRITICAL\')').get(options.retryOf);
  if(!retry || !options.action)throw new Error('Retry needs a recorded error ID and --action');
  // Generic execution failures can share a fingerprint across unrelated stages.
  // Preserve historic fingerprints/IDs, but bound recovery within the source stage.
  const previous=store.db.prepare('SELECT count(*) AS n FROM recovery_attempts r JOIN events e ON e.id=r.event_id WHERE e.fingerprint=? AND e.stage IS ?').get(retry.fingerprint,retry.stage).n;
  const limit=Number(options.maxRetries || 3);if(!Number.isInteger(limit)||limit<1)throw new Error('Invalid retry limit');
  if(previous>=limit)throw new Error('Recovery retry limit reached for this fingerprint');
 }
 const timeout=Number(options.timeoutMs || 120000);
 if(!Number.isFinite(timeout)||timeout<1)throw new Error('Invalid timeout');
 if(options.task && !store.db.prepare('SELECT 1 FROM tasks WHERE id=?').get(options.task))throw new Error('Unknown task: '+options.task);
 const session=store.session(supplied || null);
 store.db.prepare('INSERT INTO operations(id,session_id,started_at,stage,tool,operation,input_path,command_json,status,cwd,context_json) VALUES(?,?,?,?,?,?,?,?,?,?,?)').run(id,session,now(),stage,tool,'execute',options.input || null,redact(JSON.stringify(redactArgs(argv))),'running',cwd,JSON.stringify({original_va:options.va || null,stdout_as_data:options.stdoutData===true}));
 if(options.task){
  if(!store.db.prepare('SELECT 1 FROM tasks WHERE id=?').get(options.task))throw new Error('Unknown task: '+options.task);
  store.db.prepare('INSERT INTO operation_tasks VALUES(?,?)').run(id,options.task);
 }
 if(retry){const n=store.db.prepare('SELECT count(*) AS n FROM recovery_attempts WHERE event_id=?').get(retry.id).n+1;store.db.prepare('INSERT INTO recovery_attempts(id,event_id,operation_id,attempted_at,action,attempt_number,outcome,validation_status) VALUES(?,?,?,?,?,?,?,?)').run(randomUUID(),retry.id,id,now(),redact(options.action),n,'running','not_checked');}
 const streams={stdout:store.stream(id,'stdout'),stderr:store.stream(id,'stderr')};
 const preview={stdout:'',stderr:''},seen=new Set();let errors=0,spawnError=null,timedOut=false,interrupted=null,persistError=null;
 const event=line=>{
  const severity=diagnostic(line);if(!severity)return;
  const message=line.trim().slice(0,8192),key=severity+':'+message;if(seen.has(key))return;seen.add(key);if(severity==='ERROR')errors++;
  store.event(session,{operation_id:id,severity,stage,tool,operation:'diagnostic',input_path:options.input || null,original_va:options.va || null,error_category:'tool_diagnostic',message,outcome:'observed'});
 };
 const captures={};
 for(const name of ['stdout','stderr'])captures[name]=new SafeText(text=>{
  const buffer=Buffer.from(text);streams[name].write(buffer);preview[name]=(preview[name]+text).slice(0,65536);
  // Source listings may contain diagnostic-looking text, e.g. regex literals.
  // Explicit data mode affects classification only; storage/redaction and stderr stay intact.
  if(name!=='stdout' || options.stdoutData!==true)event(text);
  (name==='stdout'?process.stdout:process.stderr).write(text);
 });
 let child,timer;
 const terminate=signal=>{if(!child?.pid)return;try {if(process.platform==='win32')child.kill(signal);else process.kill(-child.pid,signal);}catch{}};
 const interrupt=signal=>{interrupted=signal;terminate(signal);};
 try {
  child=spawn(argv[0],argv.slice(1),{cwd,env:{...process.env,XBOXDECOMP_LOGGING_ACTIVE:id,XBOXDECOMP_SESSION_ID:session},stdio:['inherit','pipe','pipe'],detached:process.platform!=='win32'});
  process.on('SIGINT',interrupt);process.on('SIGTERM',interrupt);
  timer=setTimeout(()=>{timedOut=true;terminate('SIGKILL');},timeout);
  for(const name of ['stdout','stderr'])child[name].on('data',b=>{try{captures[name].push(b);}catch(e){persistError=e;terminate('SIGKILL');}});
  const result=await new Promise(resolve=>{child.on('error',e=>{spawnError=e;});child.on('close',(code,signal)=>resolve({code,signal}));});
  clearTimeout(timer);for(const name of ['stdout','stderr']){captures[name].end();streams[name].finish();}
  if(persistError)throw persistError;
  const failed=result.code!==0 || !!spawnError || errors>0 || timedOut || !!interrupted;
  const status=failed?'failed':'succeeded',exitCode=timedOut?124:spawnError?127:result.code ?? (interrupted==='SIGINT'?130:143);
  store.db.prepare('UPDATE operations SET ended_at=?,duration_ms=?,exit_code=?,stdout=?,stderr=?,status=?,validation_status=?,signal=? WHERE id=?').run(now(),Date.now()-start,result.code,preview.stdout,preview.stderr,status,failed?'failed':'not_checked',result.signal,id);
  store.event(session,{operation_id:id,severity:failed?'ERROR':'INFO',stage,tool,operation:'execute',input_path:options.input || null,original_va:options.va || null,error_category:failed?(timedOut?'timeout':spawnError?'spawn_failure':'tool_execution'):null,error_code:spawnError?.code || (failed?String(exitCode):null),message:spawnError?.message || (timedOut?'Command timed out':`Command ${status}`),stack_trace:spawnError?.stack || null,exit_code:result.code,outcome:status,validation_status:failed?'failed':'not_checked'});
  if(retry)store.db.prepare('UPDATE recovery_attempts SET outcome=?,validation_status=? WHERE operation_id=?').run(status,failed?'failed':'not_checked',id);
  if(options.task)store.db.prepare('UPDATE tasks SET status=?,updated_at=? WHERE id=?').run(failed?'blocked':'running',now(),options.task);
  store.reconcile();if(!supplied)store.endSession(session,status);
  process.stderr.write(`[sqlite operation=${id} status=${status}]\n`);
  return {operation:id,status,exitCode:failed?(exitCode || 1):0};
 } catch(e){terminate('SIGKILL');store.db.prepare("UPDATE operations SET ended_at=?,status='failed',validation_status='failed' WHERE id=?").run(now(),id);if(!supplied)store.endSession(session,'failed');throw e;}
 finally {clearTimeout(timer);process.removeListener('SIGINT',interrupt);process.removeListener('SIGTERM',interrupt);}
}
