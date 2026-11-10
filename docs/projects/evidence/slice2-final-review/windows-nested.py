"""Deliberate child selection keeps its own root; parent discovery stays isolated."""
from pathlib import Path
import json,os,subprocess,sys,tempfile
compiler=Path(sys.argv[1]).resolve();evidence=Path(sys.argv[2]) if len(sys.argv)>2 else None;rows=[]
manifest="schema=1\n[project]\nid='org.test.parent.sources'\nname='Parent'\nversion='1.0.0'\nkind='application'\n[application]\nid='org.test.parent'\n"
def run(args,status=0,error=None):
 p=subprocess.run(list(map(str,args)),capture_output=True,timeout=120);rows.append({'argv':list(map(str,args)),'status':p.returncode,'stdout':p.stdout.decode('utf-8','replace'),'stderr':p.stderr.decode('utf-8','replace')});assert p.returncode==status,rows[-1]
 if error is not None:assert error in p.stderr,rows[-1]
 return p
try:
 with tempfile.TemporaryDirectory(prefix='VOID deliberate child 雪 ') as d:
  root=Path(d);child=root/'nested';child.mkdir();(root/'void.project.toml').write_text(manifest);(child/'void.project.toml').write_text(manifest.replace('parent','child').replace('Parent','Child'))
  (root/'program.void').write_text('func main() -> i32:\n    return 17\n');(child/'launcher.void').write_text('func main() -> i32:\n    return 23\n')
  for path,status in [(root,17),(child,23)]:
   run([compiler,'check','--project',path])
   for flags in [[],['--no-opt']]:assert run([compiler,'run','--project',path,'--no-cache',*flags],status).stdout==b''
  exe=root/('child-native.exe' if os.name=='nt' else 'child-native');run([compiler,'build','--project',child,'-o',exe]);assert run([exe],23).stdout==b''
  run([compiler,'run','--project',root,'--entry','nested/launcher.void'],1,b'VOID5009')
  (child/'void.project.toml').write_text('[=roject]');run([compiler,'run','--project',root,'--no-cache'],17);run([compiler,'check','--project',child],1,b'VOID5001')
 print(f'project_nested_cli: {len(rows)} process checks passed')
finally:
 if evidence:evidence.parent.mkdir(parents=True,exist_ok=True);evidence.write_text(json.dumps({'platform':sys.platform,'commands':rows},indent=2)+'\n')
