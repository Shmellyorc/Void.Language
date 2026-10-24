"""Repeat only the two intermittently failing native fixtures; no added sleeps."""
from pathlib import Path
import json,os,subprocess,sys,time
root=Path(__file__).resolve().parents[4];base=Path(__file__).resolve().parent
name=sys.argv[1];build=root/'build'/('step42-ci98-'+name)
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1:exitcode=87',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1:exitcode=86')
records=[]
for stem in ('console_terminal','console_transfers'):
 for n in range(20):
  command=['python3',str(root/'tests/runtime'/(stem+'.py')),str(build/('void_runtime_'+stem))]
  start=time.monotonic();p=subprocess.run(command,capture_output=True,env=env)
  records.append(dict(fixture=stem,iteration=n+1,command=command,returncode=p.returncode,seconds=time.monotonic()-start,stdout_hex=p.stdout.hex(),stderr_hex=p.stderr.hex()))
  (base/(name+'-repeat-records.json')).write_text(json.dumps(records,indent=2)+'\n')
  if p.returncode:print(p.stdout,p.stderr);raise SystemExit(p.returncode)
 print(name,stem,'20/20 passed',flush=True)
