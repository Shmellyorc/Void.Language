"""Actual lock contention, SIGKILL recovery, corruption and durability failures."""
from pathlib import Path
import argparse,copy,json,os,subprocess,tempfile,time
from support import ROOT,EVIDENCE,Runner,Service,stack,preserve
parser=argparse.ArgumentParser();parser.add_argument('application',type=Path)
parser.add_argument('root',type=Path);parser.add_argument('--name',default='recovery-1')
options=parser.parse_args();app=options.application.resolve();root=options.root.resolve()
runner=Runner(options.name);work=Path(tempfile.mkdtemp(prefix='VOIDUP recovery 雪 # % '))
initial=(root/'control.json').read_bytes();fixtures=ROOT/'build/voidup-fixtures'
library=Path(tempfile.mkdtemp(prefix='voidup-fault-'))/'fault.so'
runner.run(['cc','-shared','-fPIC',ROOT/'tests/voidup/fsync_fault.c','-ldl','-o',library])
def state():return json.loads((root/'control.json').read_bytes())
base=[app,'--root',root]
old=state()['current'];target='engineering-a' if old=='engineering-b' else 'engineering-b'
env=dict(os.environ,LD_PRELOAD=str(library),VOIDUP_TEST_FAIL_SYNC=str(root/'txn-')+'*')
before=(root/'control.json').read_bytes()
result=runner.run([*base,'default',target],env=env,expected=1)
assert state()['current']==old and (root/'control.json').read_bytes()==before
env['VOIDUP_TEST_FAIL_SYNC']=str(root)
result=runner.run([*base,'default',target],env=env,expected=1)
diagnostic=json.loads(result.stderr)
assert diagnostic['published'] and not diagnostic['durable'] and diagnostic['native_code']==5,diagnostic
assert state()['current']==target
runner.run([*base,'doctor']);runner.run([*base,'rollback']);assert state()['current']==old
prefix=root/'toolchains'/target
tool=prefix/'bin/voidc';saved_mode=tool.stat().st_mode
tool.chmod(0o600)
result=runner.run([*base,'verify',target],expected=1)
assert 'permission' in json.loads(result.stderr)['message']
runner.run([*base,'default',target],expected=1);assert state()['current']==old
tool.chmod(saved_mode)
with tool.open('r+b') as stream:
 original=stream.read(1);stream.seek(0);stream.write(bytes([original[0]^1]))
runner.run([*base,'run',target,'voidc','--','--version'],expected=1)
with tool.open('r+b') as stream:stream.write(original)
sentinel=work/'sentinel';sentinel.write_bytes(b'KEEP')
(prefix/'extra-link').symlink_to(sentinel)
runner.run([*base,'verify',target],expected=1)
(prefix/'extra-link').unlink();assert sentinel.read_bytes()==b'KEEP'
runner.run([*base,'verify',target])
# A signed new release ID cannot substitute the old selected version. Kill its
# owner while its actual download waits for headers; another process must fail
# the native lock, then recover without deleting the abandoned transaction.
document=json.loads((fixtures/'www/catalog.json').read_bytes())
new_release=copy.deepcopy(document['releases'][0]);new_release['id']='crash-probe'
# Inventory must agree with this ID; package bytes are intentionally never read.
inventory=json.loads((fixtures/'www/inventories/engineering-a.json').read_bytes());inventory['release_id']='crash-probe'
from support import encoded
import hashlib
raw=encoded(inventory);new_release.update(inventory_bytes=len(raw),inventory_sha256=hashlib.sha256(raw).hexdigest(),inventory_url='crash-inventory.json')
document['releases'].append(new_release);document['serial']=11
with Service(fixtures) as server:
 server.catalog(document);server.overrides['/crash-inventory.json']=(200,raw,{})
 server.stall='/packages/engineering-a.zip'
 args=[*base,'--catalog',server.url,'--ca-file',server.ca,'--timeout-ms','120000','install','crash-probe','--activate']
 # Snapshot before process launch, including every actual imported source.
 sources={path.resolve():path.read_bytes() for directory in (ROOT/'apps/voidup',ROOT/'stdlib') for path in directory.rglob('*.void')}
 owner=subprocess.Popen(list(map(str,args)),cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.PIPE,preexec_fn=stack)
 assert server.started.wait(60),'owner did not reach actual controlled transfer'
 selected_before=state()['current'];assert state()['serial']==11 and selected_before==old
 runner.run([*base,'doctor'],expected=1)
 owner.kill();out,err=owner.communicate(timeout=30)
 killed=subprocess.CompletedProcess(list(map(str,args)),owner.returncode,out,err)
 preserve(args,ROOT,killed,(ROOT/'apps/voidup',),(Path(__file__).parent,),failure_kind='intentional_SIGKILL_recovery_probe',source_snapshots=sources)
 assert owner.returncode==-9
 pending=sorted(p.name for p in (root/'transactions').iterdir())
 server.resume.set()
 runner.run([*base,'doctor'])
 assert state()['current']==selected_before and state()['serial']==11
 assert sorted(p.name for p in (root/'transactions').iterdir())==pending
 # Revocation defeats rollback and run, even after restarting the application.
 document['serial']=12;document['revoked']=[target];server.catalog(document)
 network=[*base,'--catalog',server.url,'--ca-file',server.ca]
 runner.run([*network,'list','--available'])
 runner.run([*base,'default',target],expected=1)
 runner.run([*base,'run',target,'voidc','--','--version'],expected=1)
 runner.run([*base,'rollback'],expected=1)
 assert state()['serial']==12 and state()['current']==old
 (EVIDENCE/(options.name+'-summary.json')).write_text(json.dumps(dict(schema=1,root=str(root),initial_control=json.loads(initial),final_control=state(),published_uncertain_diagnostic=diagnostic,intentional_owner_status=-9,retained_transactions=pending,work=str(work)),indent=2)+'\n')
print('Pre/post-publication faults, corruption, lock contention, SIGKILL recovery and revocation passed')
