"""Actual CLI optimization, selected warnings, cache conflicts and independent AOT."""
from pathlib import Path
import json,subprocess,sys,tempfile,os,re
compiler=Path(sys.argv[1]).resolve();objdump=Path(sys.argv[2]).resolve();evidence=Path(sys.argv[3]) if len(sys.argv)>3 else None
rows=[];checks=0
manifest="schema=1\n[project]\nid='org.test.sources'\nname='Sources'\nversion='1.0.0'\nkind='application'\n[application]\nid='org.test.app'\n[sources]\nroots=['src']\n"
source='public func compute(x: i32) -> i32:\n    var y = x + 0\n    y = y * 1\n    return y\nfunc main() -> i32:\n    return compute(23)\n'
def run(args,status=0,warning=None):
 global checks
 p=subprocess.run(list(map(str,args)),capture_output=True,timeout=120);rows.append({'argv':list(map(str,args)),'status':p.returncode,'stdout':p.stdout.decode('utf-8','replace'),'stderr':p.stderr.decode('utf-8','replace')});assert p.returncode==status,rows[-1]
 if warning is not None:
  assert p.stderr.count(b'VOID5012')==int(bool(warning)),rows[-1]
  if warning:assert b'warning:' in p.stderr and b"debug_info='"+warning.encode()+b"'" in p.stderr and b'profile-controlled' in p.stderr,rows[-1]
 checks+=1;return p
try:
 with tempfile.TemporaryDirectory(prefix='VOID profile execution 雪 ') as directory:
  root=Path(directory);(root/'src').mkdir();toml=root/'void.project.toml';toml.write_bytes(manifest.encode());(root/'src/game.void').write_bytes(source.encode())
  for flags,warning in [([], 'line'),(['--profile','release'],'line'),(['--profile','debug'],'full'),(['--no-opt'],'line')]:
   run([compiler,'check','--project',root,*flags],warning=warning)
   run([compiler,'run','--project',root,'--no-cache',*flags],23,warning=warning)
  for flags in [['--profile'],['--profile','Debug'],['--profile','other'],['--profile','debug','--profile','release'],['--profile','release','--cache'],['--profile','release','--cache','--no-opt'],['--profile','debug','--cache','--no-cache']]:run([compiler,'run','--project',root,*flags],2)
  for command in ['run','check','build','emit']:
   flags=['-o',root/'unused.o'] if command=='emit' else []
   run([compiler,command,root/'src/game.void','--profile','debug',*flags],2)
  # Emit and inspect the actual compute function. Register allocation may simplify
  # arithmetic even at O0; its retained stack traffic still distinguishes O2.
  def instructions(label,flags,warning):
   path=root/(label+'.o');run([compiler,'emit','--project',root,*flags,'-o',path],warning=warning)
   dump=run([objdump,'-d','--no-show-raw-insn',path]).stdout.decode();symbol='__vd_'+b'/src/game.void::compute'.hex()+'__void'
   match=re.search(r'<'+symbol+r'>:\n(.*?)(?:\n\n|\Z)',dump,re.S);assert match,dump
   body=[]
   for line in match[1].splitlines():
    if re.match(r'\s*[0-9a-f]+:',line):
     body.append(line.split(':',1)[1].strip())
     if re.match(r'ret\w*\b',body[-1]):break
   assert body and re.match(r'ret\w*\b',body[-1]),body
   return len(body)
  debug=instructions('debug',['--profile','debug'],'full');release=instructions('release',['--profile','release'],'line');noopt=instructions('release-noopt',['--profile','release','--no-opt'],'line');assert debug>release and noopt==debug,(debug,release,noopt)
  for profile,warning in [('debug','full'),('release','line')]:
   exe=root/(profile+('.exe' if os.name=='nt' else ''));run([compiler,'build','--project',root,'--profile',profile,'-o',exe],warning=warning);run([exe],23,warning='')
  cold=run([compiler,'run','--project',root,'--profile','debug','--cache'],23,warning='full');warm=run([compiler,'run','--project',root,'--profile','debug','--cache'],23,warning='full')
  if b'Debug cache: unavailable' not in cold.stderr:assert b'Debug cache: miss' in cold.stderr and b'Debug cache: hit' in warm.stderr,(cold.stderr,warm.stderr)
  else:print('UNQUALIFIED: native persistent disk cache warm hit')
  run([compiler,'run','--project',root,'--cache'],23,warning='full')
  overrides="[build.debug]\noptimization='O2'\ndebug_info='none'\n[build.release]\noptimization='none'\ndebug_info='full'\n";toml.write_bytes((manifest+overrides).encode())
  optimized_debug=instructions('custom-debug',['--profile','debug'],'');plain_release=instructions('custom-release',['--profile','release'],'full');assert optimized_debug==release and plain_release==debug,(optimized_debug,plain_release)
  run([compiler,'run','--project',root,'--profile','debug','--cache'],2,warning='');run([compiler,'run','--project',root,'--profile','debug','--cache','--no-opt'],23,warning='')
  # An unselected unsupported setting is quiet; only the selected setting warns.
  toml.write_bytes((manifest+"[build.debug]\ndebug_info='full'\n[build.release]\ndebug_info='none'\n").encode());run([compiler,'check','--project',root],warning='');run([compiler,'check','--project',root,'--profile','debug'],warning='full')
  # Legacy standalone emission/run stays on the established option grammar.
  run([compiler,'run',root/'src/game.void','--no-cache'],23,warning='');run([compiler,'run',root/'src/game.void','--cache'],23,warning='')
 print(f'project_profiles: {checks} process checks passed; compute instruction counts O0={debug}, O2={release}, release --no-opt={noopt}')
finally:
 if evidence:evidence.write_text(json.dumps({'checks':checks,'commands':rows},indent=2)+'\n')
