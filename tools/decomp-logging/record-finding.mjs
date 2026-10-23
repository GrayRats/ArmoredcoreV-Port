import Database from 'better-sqlite3';
import {randomUUID,createHash} from 'node:crypto';
import {readFileSync,existsSync} from 'node:fs';
import path from 'node:path';
const workspace=path.resolve(process.env.XBOXDECOMP_WORKSPACE || path.join(import.meta.dirname,'../..'));
const [addressHex,summary,confidence,...evidencePaths]=process.argv.slice(2);
if(!addressHex || !summary || !evidencePaths.length) throw new Error('Usage: record-finding.mjs 0xVA "summary" confidence evidence-path...');
const va=Number(addressHex);if(!Number.isInteger(va)||va<0||va>0xffffffff)throw new Error('Invalid 32-bit guest VA');
const db=new Database(path.join(workspace,'logs/decomp.sqlite3'),{timeout:5000});
db.pragma('foreign_keys=ON');
const now=new Date().toISOString(),session=randomUUID(),operation=randomUUID(),fn=randomUUID();
db.transaction(()=>{
 db.prepare('INSERT INTO sessions(id,started_at,ended_at,workspace,outcome) VALUES(?,?,?,?,?)').run(session,now,now,workspace,'succeeded');
 db.prepare('INSERT INTO functions(id,binary_path,original_va,name,reconstructed_signature,recovery_status) VALUES(?,?,?,?,?,?)').run(fn,'games/acv/extracted/default.xex',va,null,null,'partial_analysis');
 db.prepare('INSERT INTO operations(id,session_id,function_id,started_at,ended_at,stage,tool,operation,input_path,status,exit_code,validation_status) VALUES(?,?,?,?,?,?,?,?,?,?,?,?)').run(operation,session,fn,now,now,'static_analysis','ppc_dis','analyze_function',evidencePaths[0],'succeeded',0,'evidence_recorded');
 for(const relative of evidencePaths){const file=path.resolve(workspace,relative);if(!file.startsWith(workspace+path.sep)||!existsSync(file))throw new Error(`Missing/outside-workspace evidence: ${relative}`);db.prepare('INSERT INTO artifacts(id,session_id,operation_id,path,kind,sha256,provenance_json,created_at) VALUES(?,?,?,?,?,?,?,?)').run(randomUUID(),session,operation,path.relative(workspace,file),'disassembly',createHash('sha256').update(readFileSync(file)).digest('hex'),JSON.stringify({guest_va:addressHex,confidence}),now);}
 db.prepare('INSERT INTO events(id,timestamp_utc,session_id,operation_id,function_id,severity,stage,tool,operation,input_path,original_va,message,context_json,outcome,validation_status) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)').run(randomUUID(),now,session,operation,fn,'INFO','static_analysis','ppc_dis','inspect_function',evidencePaths[0],va,summary,JSON.stringify({confidence,evidence:evidencePaths}), 'succeeded','evidence_recorded');
})();db.close();console.log(JSON.stringify({session,operation,function_id:fn,address:addressHex,evidence:evidencePaths}));
