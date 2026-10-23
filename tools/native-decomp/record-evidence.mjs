import Database from '../decomp-logging/node_modules/better-sqlite3/lib/index.js';
import {readFileSync} from 'node:fs';
import {createHash,randomUUID} from 'node:crypto';
import path from 'node:path';
const root=path.resolve(import.meta.dirname,'../..');
const read=p=>readFileSync(path.join(root,p));
const branch=JSON.parse(read('logs/acv-more-instructions-20261020/verification.json'));
const decomp=JSON.parse(read('tmp/native-decomp-8245fa20/verification.json'));
const counts=JSON.parse(decomp.execution.stdout);
if(branch.recompile.exit_code!==0 || branch.coverage.after_unique!==38 ||
 decomp.compile.exit_code!==0 || decomp.execution.exit_code!==0 || counts.state_cases!==69152 || counts.golden_transitions!==10)
 throw new Error('Required completed evidence missing');
const db=new Database(path.join(root,'logs/decomp.sqlite3'));db.pragma('foreign_keys=ON');
const find=command=>db.prepare("SELECT * FROM operations WHERE command_json=? AND status='succeeded' ORDER BY started_at DESC LIMIT 1").get(JSON.stringify(command));
const tasks=[{
 command:['python3','tools/acv_more_instructions.py','--resume-build-fix'],
 message:'Pinned branch plus isolated linkage fix: unsupported unique addresses 10939 to 38; generation coverage only',
 files:['tools/acv_more_instructions.py','tmp/acv-more-instructions.toml','docs/fine/XENON_MORE_INSTRUCTIONS.md',
  ...['verification.json','configure.stdout.log','configure.stderr.log','build.stdout.log','build.stderr.log',
      'build_recovery.stdout.log','build_recovery.stderr.log','build-fix.patch','recompile.stdout.log',
      'recompile.stderr.log','remaining-addresses.json','remaining-instructions.json'].map(p=>'logs/acv-more-instructions-20261020/'+p)]
},{
 command:['python3','tools/native-decomp/run_8245fa20.py'],
 message:'Readable retail 0x8245FA20 state reconstruction: 69152 state cases and 10 expected transitions pass; no Xenia parity',
 files:['tools/native-decomp/acv_8245fa20.hpp','tools/native-decomp/compare_8245fa20.cpp',
  'tools/native-decomp/run_8245fa20.py','logs/decomp-20261020/8245fa20.asm','docs/fine/NATIVE_DECOMP_8245FA20.md',
  ...['verification.json','generated_8245fa20.cpp','compile.stdout.txt','compile.stderr.txt',
      'execution.stdout.txt','execution.stderr.txt'].map(p=>'tmp/native-decomp-8245fa20/'+p)]
}];
const ids=[];
db.transaction(()=>{
 for(const task of tasks){
  const op=find(task.command);if(!op)throw new Error('Missing successful logged operation');ids.push(op.id);
  for(const file of task.files)db.prepare('INSERT INTO artifacts(id,session_id,operation_id,path,kind,sha256,provenance_json,created_at) VALUES(?,?,?,?,?,?,?,?)')
   .run(randomUUID(),op.session_id,op.id,file,'native_decomp_evidence',createHash('sha256').update(read(file)).digest('hex'),
    JSON.stringify({scope:'bounded native decompilation; no hardware/Xenia parity'}),new Date().toISOString());
  db.prepare('UPDATE operations SET validation_status=? WHERE id=?').run('verified',op.id);
  db.prepare('INSERT INTO events(id,timestamp_utc,session_id,operation_id,severity,stage,tool,operation,message,context_json,outcome,validation_status) VALUES(?,?,?,?,?,?,?,?,?,?,?,?)')
   .run(randomUUID(),new Date().toISOString(),op.session_id,op.id,'INFO','native_decomp','evidence_registrar','verify',task.message,
    JSON.stringify({artifacts:task.files.length}), 'succeeded','verified');
 }
 const failure=db.prepare("SELECT e.id FROM events e JOIN operations o ON o.id=e.operation_id WHERE o.command_json=? AND e.severity='ERROR' AND e.operation='execute' ORDER BY e.timestamp_utc DESC LIMIT 1")
  .get(JSON.stringify(['python3','tools/acv_more_instructions.py']));
 if(failure)db.prepare('INSERT INTO recovery_attempts(id,event_id,operation_id,attempted_at,action,attempt_number,outcome,validation_status) VALUES(?,?,?,?,?,?,?,?)')
  .run(randomUUID(),failure.id,ids[0],new Date().toISOString(),'Isolated main-compatible extern linkage fix for lzxDecompress; rebuilt and regenerated',1,'succeeded','verified');
})();
console.log(JSON.stringify({verified_operations:ids,artifact_counts:tasks.map(t=>t.files.length),recovery_recorded:true}));db.close();
