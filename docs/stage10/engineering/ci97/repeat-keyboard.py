"""Repeat the original real-PTY driver without sleeps or changed assertions."""
from pathlib import Path
import json,subprocess,sys,time
root=Path(__file__).resolve().parents[4];base=Path(__file__).resolve().parent
name=sys.argv[1];binary=root/'build'/('step42-ci97-'+name)/'void_runtime_console_keyboard';records=[]
for n in range(20):
 start=time.monotonic();p=subprocess.run(['python3',str(root/'tests/runtime/console_keyboard.py'),str(binary)],capture_output=True)
 records.append(dict(iteration=n+1,returncode=p.returncode,seconds=time.monotonic()-start,stdout_hex=p.stdout.hex(),stderr_hex=p.stderr.hex()))
 (base/(name+'-keyboard-repeat-records.json')).write_text(json.dumps(records,indent=2)+'\n')
 if p.returncode:print(p.stdout,p.stderr);raise SystemExit(p.returncode)
print(name,'20/20 original native PTY keyboard repetitions passed')
