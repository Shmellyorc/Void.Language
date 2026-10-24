"""Record bounded local commands without changing sanitizer/stack settings."""
from pathlib import Path
import json,os,subprocess,sys,time
base=Path(__file__).resolve().parent
label=sys.argv[1];command=sys.argv[2:]
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1:exitcode=87',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1:exitcode=86')
start=time.monotonic()
with (base/(label+'.log')).open('wb') as log:
 result=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,env=env)
record=dict(command=command,returncode=result.returncode,seconds=time.monotonic()-start,ASAN_OPTIONS=env['ASAN_OPTIONS'],UBSAN_OPTIONS=env['UBSAN_OPTIONS'])
(base/(label+'.json')).write_text(json.dumps(record,indent=2)+'\n');print(label,record['returncode'],record['seconds'],flush=True)
raise SystemExit(result.returncode)
