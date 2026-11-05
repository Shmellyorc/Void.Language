from pathlib import Path
import os,sys,json,shutil,time
root=Path.cwd();d=Path(Path('/tmp/void-no-match-current').read_text());sys.path.insert(0,str(root/'tests/log'));import support as h
h.EVIDENCE=d/'receipts';os.environ['VOID_CRASH_EVIDENCE']=str(d/'crashes');shutil.copy2(__file__,d/'confirm-heavy.py');rows=[];events=[]
for block,version in enumerate(['candidate','original','original','candidate']):
 label=f'confirm-heavy-{block}-{version}';work=d/label;work.mkdir();env=os.environ.copy()
 for k in ['PROFILE','COUNTS','SAMPLE_RULES','VOID_STDLIB_ROOT','VOID_PERF_SHAPES','VOID_PERF_STAGES']:env.pop(k,None)
 env.update(VOID_STDLIB_ROOT=str(d/'stdlib'),VOID_PERF_SHAPES='4',VOID_PERF_STAGES='7,12')
 args=[d/('components-'+version),d/'components.void',work,'optimized','timing'];event={'label':label,'started_unix':time.time(),'loadavg':os.getloadavg(),'command':list(map(str,args)),'environment':{k:env[k] for k in ['VOID_STDLIB_ROOT','VOID_PERF_SHAPES','VOID_PERF_STAGES']}};events.append(event)
 p=h.run(args,cwd=work,env=env,small=True,timeout=600,sources=[d]);event.update(ended_unix=time.time(),exit_code=p.returncode);(d/(label+'.stdout')).write_bytes(p.stdout);(d/(label+'.stderr')).write_bytes(p.stderr)
 for line in p.stdout.decode().splitlines():
  if line.startswith('PERF '):r=json.loads(line[5:]);r.update(block=block,version=version,workload='components');rows.append(r)
 (d/'confirm-heavy-samples.json').write_text(json.dumps(rows,indent=2)+'\n');(d/'confirm-heavy-events.json').write_text(json.dumps(events,indent=2)+'\n');print(label,p.returncode,flush=True)
