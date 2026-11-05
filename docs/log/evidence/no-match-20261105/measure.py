from pathlib import Path
import os,sys,json,shutil,time
root=Path.cwd();d=Path(Path('/tmp/void-no-match-current').read_text());sys.path.insert(0,str(root/'tests/log'));import support as h
h.EVIDENCE=d/'receipts';os.environ['VOID_CRASH_EVIDENCE']=str(d/'crashes');shutil.copy2(__file__,d/'measure.py');rows=[];events=[]
for block,version in enumerate(['original','candidate','candidate','original']):
 for workload in ['original','components']:
  label=f'{block}-{version}-{workload}';work=d/label;work.mkdir();env=os.environ.copy()
  for k in ['PROFILE','COUNTS','SAMPLE_RULES','VOID_STDLIB_ROOT','VOID_PERF_SHAPES','VOID_PERF_STAGES']:env.pop(k,None)
  exe=d/(workload+'-'+version);args=[exe,root,work]
  if workload=='components':
   env.update(VOID_STDLIB_ROOT=str(d/'stdlib'),VOID_PERF_SHAPES='0,4',VOID_PERF_STAGES='6,7,12');args=[exe,d/'components.void',work,'optimized','timing']
  event={'label':label,'started_unix':time.time(),'loadavg':os.getloadavg(),'environment':{k:env[k] for k in ['VOID_STDLIB_ROOT','VOID_PERF_SHAPES','VOID_PERF_STAGES'] if k in env},'command':list(map(str,args))};events.append(event);(d/'events.json').write_text(json.dumps(events,indent=2)+'\n')
  p=h.run(args,cwd=work,env=env,small=True,timeout=600,sources=[d]);event.update(ended_unix=time.time(),exit_code=p.returncode)
  (d/(label+'.stdout')).write_bytes(p.stdout);(d/(label+'.stderr')).write_bytes(p.stderr)
  for line in p.stdout.decode().splitlines():
   if line.startswith(('BENCH ','PERF ')):
    r=json.loads(line.split(' ',1)[1]);r.update(block=block,version=version,workload=workload);rows.append(r)
  (d/'samples.json').write_text(json.dumps(rows,indent=2)+'\n');(d/'events.json').write_text(json.dumps(events,indent=2)+'\n');print(label,p.returncode,flush=True)
print('COMPLETE',len(rows),'rows')
