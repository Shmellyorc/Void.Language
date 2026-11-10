from pathlib import Path
import tempfile,subprocess,json,hashlib,os
r=Path.cwd();out=r/'build/slice3-installed';out.mkdir(exist_ok=False);rows=[]
def run(label,args,status=0,env=None):
 p=subprocess.run(list(map(str,args)),capture_output=True,env=env,timeout=180);row={'label':label,'argv':list(map(str,args)),'status':p.returncode,'expected':status,'stdout':p.stdout.decode('utf-8','replace'),'stderr':p.stderr.decode('utf-8','replace')};rows.append(row);assert p.returncode==status,row;return p
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
for component in ['compiler','tooling']:
 prefix=out/component;run(component+'-install',['cmake','--install',r/'build/llvm23-release','--prefix',prefix,'--component',component]);base=prefix/'share/void/licenses/tomlplusplus'
 for local,installed in [('toml.hpp','corresponding-source/toml.hpp'),('README.md','corresponding-source/README.md'),('LICENSE','LICENSE'),('upstream.json','upstream.json'),('void-bounds-and-locations.patch','void-bounds-and-locations.patch')]:assert sha(r/'third_party/tomlplusplus'/local)==sha(base/installed)
 run(component+'-provenance',['python3',r/'tests/project/provenance.py','--installed',base])
compiler=out/'compiler/bin/voidc';server=out/'tooling/bin/voidls'
run('installed-project-protocol',['python3',r/'tests/project/protocol.py',server,out/'protocol.json'])
with tempfile.TemporaryDirectory(prefix='VOID S3 installed 雪 ') as temp:
 root=Path(temp)/'project';(root/'src').mkdir(parents=True);(root/'void.project.toml').write_text("schema=1\n[project]\nid='org.test.installed'\nname='Installed'\nversion='1.0.0'\nkind='application'\n[application]\nid='org.test.application'\n[sources]\nroots=['src']\n")
 (root/'src/game.void').write_text('use os\nfunc main() -> i32:\n    return 23\n')
 for profile in ['debug','release']:
  p=run(profile+'-installed-check',[compiler,'check','--project',root,'--profile',profile]);assert b'VOID5012' in p.stderr
  run(profile+'-installed-JIT',[compiler,'run','--project',root,'--profile',profile,'--no-cache'],23)
 exe=Path(temp)/'installed-application';run('installed-AOT-build',[compiler,'build','--project',root,'--profile','release','-o',exe]);run('independent-installed-AOT',[exe],23)
 root.rename(Path(temp)/'source-hidden');environment=dict(os.environ,VOID_STDLIB_ROOT=str(Path(temp)/'no-source-tree'));run('AOT-with-source-hidden-and-invalid-stdlib',[exe],23,environment)
record={'status':'PASS','scope':'Independent compiler/tooling installs, preserved license/corresponding source, installed project protocol, profiles/JIT/AOT and source-hidden native application','commands':rows,'installed_binary_sha256':{str(p):sha(p) for p in [compiler,server]}}
(out/'results.json').write_text(json.dumps(record,indent=2)+'\n');print('Installed qualification PASS:',len(rows),'commands')
