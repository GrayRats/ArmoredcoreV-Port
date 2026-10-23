import {readFileSync,statSync} from 'node:fs';
import {randomUUID} from 'node:crypto';
import path from 'node:path';
import {Store,workspaceRoot,emergency,now} from './store.mjs';
import {runCommand,SafeText,diagnostic} from './capture.mjs';
import {importLogs,cleanup,restore} from './archive.mjs';
const workspace=workspaceRoot();let store;
function parse(args){
 const opts={},rest=[];let i=0;
 for(;i<args.length;i++){
  if(args[i]==='--'){i++;break;}
  if(!args[i].startsWith('--'))break;
  const key=args[i].slice(2).replace(/-([a-z])/g,(_,c)=>c.toUpperCase());
  if(['apply','includeHistory','stdoutData'].includes(key)){opts[key]=true;continue;}
  if(args[i+1]===undefined)throw new Error('Missing option value: '+args[i]);opts[key]=args[++i];
 }
 rest.push(...args.slice(i));if(opts.va!==undefined){opts.va=Number(opts.va);if(!Number.isInteger(opts.va)||opts.va<0||opts.va>0xffffffff)throw new Error('Invalid guest VA');}
 return {opts,rest};
}
const help=`Usage: node tools/decomp-logging/cli.mjs COMMAND
 init | status | errors [--tool NAME] [--stage NAME] [--va 0xVA] [--limit N]
 run [--stage NAME] [--input FILE] [--va 0xVA] [--cwd DIR] [--timeout-ms N] [--task ID]
     [--retry-of EVENT_ID --action TEXT --max-retries N] [--stdout-data] [--] COMMAND [ARG...]
 history [--limit N] | verify OPERATION_ID NOTE
 resolve --by VERIFIED_OPERATION --action NOTE EVENT_ID
 task ID pending|running|blocked|done TITLE | tasks
 session start [LABEL] | session finish SESSION_ID
 ingest [--tool NAME --stage NAME --va 0xVA] RESULT_FILE
 import-logs | cleanup [--apply] | restore ARCHIVED_PATH [DESTINATION]
 output OPERATION_ID stdout|stderr`;
try {
 const [command='help',...args]=process.argv.slice(2);if(command==='help'||command==='--help'){console.log(help);}else{
  store=new Store(workspace);const {opts,rest}=parse(args);
  switch(command){
   case 'init': {const imported=store.importLegacy(),session=store.session();const id=store.event(session,{severity:'INFO',stage:'bootstrap',tool:'decomp-logging',operation:'initialize',message:'SQLite split stores initialized; persistent write/read checked',outcome:'succeeded',validation_status:'verified'});const verified=!!store.db.prepare('SELECT 1 FROM events WHERE id=?').get(id);store.endSession(session,'succeeded');console.log(JSON.stringify({databases:store.paths,...imported,write_read_verified:verified}));break;}
   case 'status': console.log(JSON.stringify(store.status(),null,2));break;
   case 'errors':console.log(JSON.stringify(store.errors(opts),null,2));break;
   case 'history':console.log(JSON.stringify(store.db.prepare('SELECT id,session_id,started_at,stage,tool,status,exit_code,duration_ms,validation_status FROM operations ORDER BY started_at DESC LIMIT ?').all(Number(opts.limit || 20)),null,2));break;
   case 'run':{const r=await runCommand(store,rest,opts);process.exitCode=r.exitCode;break;}
   case 'verify':if(rest.length<2)throw new Error('verify needs operation ID and validation note');store.verify(rest[0],rest.slice(1).join(' '));console.log(JSON.stringify({operation:rest[0],validation_status:'verified'}));break;
   case 'resolve':if(!rest[0]||!opts.by||!opts.action)throw new Error('resolve --by VERIFIED_OPERATION --action NOTE EVENT_ID');store.resolve(rest[0],opts.by,opts.action);console.log(JSON.stringify({event:rest[0],resolved_by:opts.by}));break;
   case 'task':if(rest.length<3)throw new Error('task needs ID STATUS TITLE');store.task(rest[0],rest.slice(2).join(' '),rest[1]);console.log(JSON.stringify({id:rest[0],status:rest[1]}));break;
   case 'tasks':console.log(JSON.stringify(store.db.prepare('SELECT * FROM tasks ORDER BY updated_at DESC').all(),null,2));break;
   case 'session':{
    if(rest[0]==='start'){const id=store.session();store.event(id,{severity:'INFO',stage:'session',operation:'start',message:rest.slice(1).join(' ')||'Project session started'});console.log(JSON.stringify({session:id}));}
    else if(rest[0]==='finish'&&rest[1]){const errors=store.errors({limit:100000}).filter(e=>e.session_id===rest[1]);store.endSession(rest[1],errors.length?'issues_open':'succeeded');console.log(JSON.stringify({session:rest[1],unresolved_errors:errors},null,2));}
    else throw new Error('session start LABEL | session finish ID');break;
   }
   case 'import-logs':console.log(JSON.stringify(await importLogs(store)));break;
   case 'cleanup':console.log(JSON.stringify(await cleanup(store,{apply:!!opts.apply}),null,2));break;
   case 'restore':if(!rest[0])throw new Error('restore needs archived relative path');console.log(JSON.stringify(restore(store,rest[0],rest[1])));break;
   case 'output':if(!rest[0]||!['stdout','stderr'].includes(rest[1]))throw new Error('output needs OPERATION_ID stdout|stderr');for(const b of store.output(rest[0],rest[1]))process.stdout.write(b);break;
   case 'ingest':{
    if(!rest[0])throw new Error('ingest needs saved tool result');
    const file=path.resolve(workspace,rest[0]);if(statSync(file).size>16*1024*1024)throw new Error('Use import-logs for result files above 16 MiB');const input=readFileSync(file);
    let structured=null;try{structured=JSON.parse(input.toString('utf8'));}catch{}
    const rootLevel=structured?diagnostic(JSON.stringify(structured)):null;
    const session=store.session(),id=randomUUID();
    const stage=opts.stage || 'external_tool',tool=opts.tool || 'saved_tool_result';
    store.db.prepare('INSERT INTO operations(id,session_id,started_at,stage,tool,operation,input_path,status) VALUES(?,?,?,?,?,?,?,?)').run(id,session,now(),stage,tool,'ingest',path.relative(workspace,file),'running');
    const stream=store.stream(id,'stdout');let failed=rootLevel==='ERROR';
    if(rootLevel){const message=structured.error?.message || (typeof structured.error==='string'?structured.error:null) || structured.message || structured.content?.filter(b=>b.type==='text').map(b=>b.text).join('\n') || 'Structured tool failure';store.event(session,{operation_id:id,severity:rootLevel,stage,tool,input_path:path.relative(workspace,file),original_va:opts.va ?? null,error_category:'external_tool_result',message,context_json:structured,outcome:'observed'});}
    const safe=new SafeText(text=>{stream.write(Buffer.from(text));const severity=structured?null:diagnostic(text);if(severity){if(severity==='ERROR')failed=true;store.event(session,{operation_id:id,severity,stage,tool,input_path:path.relative(workspace,file),original_va:opts.va || null,error_category:'external_tool_result',message:text.trim().slice(0,8192),outcome:'observed'});}});
    safe.push(input);safe.end();stream.finish();
    store.db.prepare('UPDATE operations SET ended_at=?,status=?,validation_status=? WHERE id=?').run(now(),failed?'failed':'observed','not_checked',id);
    store.endSession(session,failed?'failed':'succeeded');console.log(JSON.stringify({operation:id,status:failed?'failed':'observed'}));if(failed)process.exitCode=1;break;
   }
   default:throw new Error(help);
  }
 }
} catch(error) {
 if(store)try {const session=store.session();store.event(session,{severity:'ERROR',stage:'logging_control',tool:'decomp-logging',operation:process.argv[2] || null,error_category:'logger_exception',error_code:error.code || null,message:error.message,stack_trace:error.stack,outcome:'failed'});store.endSession(session,'failed');}catch{}
 emergency(workspace,error);process.exitCode=1;
} finally {store?.close();}
