"""Final source/bootstrap guard check on the real relocated A/B installation."""
from pathlib import Path
import argparse,json,os,shutil,sys
from support import ROOT,EVIDENCE,Runner
parser=argparse.ArgumentParser();parser.add_argument('prefix',type=Path);parser.add_argument('work',type=Path)
parser.add_argument('--name',default='final-1');options=parser.parse_args()
work=options.work.resolve();root=work/'relocated private root';destination=work/'final bootstrap'
shutil.copytree(options.prefix,destination,symlinks=False)
runner=Runner(options.name);app=destination/'bin/voidup';launcher=ROOT/'tests/host_capabilities/isolated_exec.py'
env=dict(os.environ,PATH='/hostile',VOID_STDLIB_ROOT='/absent',LD_LIBRARY_PATH='/absent')
def run(*args,**kwargs):return runner.run([sys.executable,launcher,work,app,'--root',root,*args],cwd=work,env=env,**kwargs)
run('--version')
for release in ('engineering-a','engineering-b'):
 run('default',release);version=run('run','default','voidc','--','--version')
 assert json.loads((root/'toolchains'/release/'toolchain.json').read_bytes())['build_identity'].encode() in version.stdout
run('rollback');run('doctor')
state=json.loads((root/'control.json').read_bytes());assert state['current']=='engineering-a' and state['previous']=='engineering-b' and state['serial']==10
(EVIDENCE/(options.name+'-summary.json')).write_text(json.dumps(dict(schema=1,bootstrap=str(destination),root=str(root),checkout_denied=True,actual_a_b_selection=True,source_sha256=__import__('hashlib').sha256((ROOT/'apps/voidup/manager.void').read_bytes()).hexdigest(),control=state),indent=2)+'\n')
print('Final Ubuntu-built bootstrap: A/B selection, build identities, rollback and doctor passed with checkout denied')
