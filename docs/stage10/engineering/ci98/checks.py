"""Focused original console drivers, isolated binaries, unchanged strict settings."""
from pathlib import Path
import hashlib,json,os,shutil,subprocess,sys,time
root=Path(__file__).resolve().parents[4];base=Path(__file__).resolve().parent
name=sys.argv[1];assert name in ('debug','san');build=root/'build'/('step42-ci98-'+name)
freeze=json.loads((base/'repair-source-freeze.json').read_text())
assert all(hashlib.sha256((root/p).read_bytes()).hexdigest()==h for p,h in freeze.items())
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1:exitcode=87',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1:exitcode=86')
commands=[]
for stem in ('console','console_transfers','console_input','console_keyboard','console_terminal','console_wide'):
 commands.append((stem,[str(build/('void_runtime_'+stem))]))
for stem in ('console_endpoints','console_transfers','console_input','console_keyboard','console_terminal'):
 binary='console' if stem=='console_endpoints' else stem
 commands.append((stem+'-native',['python3',str(root/'tests/runtime'/(stem+'.py')),str(build/('void_runtime_'+binary))]))
records=[]
for label,command in commands:
 start=time.monotonic()
 with (base/(name+'-test-'+label+'.log')).open('wb') as log:r=subprocess.run(command,cwd=root,stdout=log,stderr=subprocess.STDOUT,env=env)
 records.append(dict(label=label,command=command,returncode=r.returncode,seconds=time.monotonic()-start))
 (base/(name+'-tests.json')).write_text(json.dumps(dict(ASAN_OPTIONS=env['ASAN_OPTIONS'],UBSAN_OPTIONS=env['UBSAN_OPTIONS'],tests=records),indent=2)+'\n')
 print(name,label,r.returncode,flush=True)
 if r.returncode:raise SystemExit(r.returncode)
for report in build.glob('step*-*results.json'):shutil.copy2(report,base/(name+'-'+report.name))
print(name,len(records),'focused console suites passed',flush=True)
