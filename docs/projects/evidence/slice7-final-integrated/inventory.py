"""Produce an exact accepted-source and review inventory without writing the index."""
from collections import Counter
from hashlib import file_digest, sha256
from pathlib import Path
import json, os, subprocess

root=Path.cwd()
work=root/'build/slice7-final-integrated'
evidence=Path('docs/projects/evidence/slice7-final-integrated')
report=Path('docs/projects/SLICE7_FINAL_CLOSEOUT.md')
accepted=Path('docs/projects/evidence/slice7-g5-release-refusal/inventory.txt')
opening=json.loads((work/'opening.json').read_text())
replay=json.loads((evidence/'layer-roundtrip.json').read_text())
touched={name for gate in replay['ordered_layers'] for name in gate['files']}

def git(*args):
    return subprocess.check_output(['git','--no-optional-locks',*args])

def digest(path):
    with Path(path).open('rb') as stream:
        return file_digest(stream,'sha256').hexdigest()

def lines(path,entries):
    path.write_text(''.join(str(name)+'\n' for name in sorted(entries)))

index=git('ls-files','--stage','-z')
assert git('rev-parse','HEAD').decode().strip()==opening['head']
assert sha256(index).hexdigest()==opening['index']
staged=git('diff','--cached','--binary','HEAD','--')
assert not staged
git('diff','--check')

prior_layers={}
for row in accepted.read_text().splitlines():
    fields=row.split(' | ',4)
    if len(fields)==5 and fields[0] in (' M','??'):
        prior_layers[fields[2]]=fields[1]
assert prior_layers.keys()==opening['entries'].keys()
for name,(_,hashed) in opening['entries'].items():
    assert digest(name)==hashed,name

explicit_reports={Path('docs/projects')/name for name in (
    'SLICE7_G1.md','SLICE7_G2.md','SLICE7_G3.md','SLICE7_G3_FINAL_CLOSEOUT.md',
    'SLICE7_G4_CONVENTIONAL_FFI.md','SLICE7_G4_LINUX.md','SLICE7_G5_CALLBACKS.md',
    'SLICE7_RAYLIB_INTEGRATION.md','SLICE7_FINAL_CLOSEOUT.md')}
product={Path(name) for name in opening['entries'] if not name.startswith(('docs/projects/','docs/log/evidence/'))}
integration=product|explicit_reports
assert all(path.is_file() for path in integration)
lines(evidence/'integration-paths.txt',integration)
lines(evidence/'slice7-product-paths.txt',touched)

required={report,accepted,Path('docs/projects/C_CALLBACK_RESEARCH.md'),
          Path('docs/projects/evidence/slice7-inventory.txt'),
          *(Path('docs/projects/evidence')/gate['gate']/'source.patch' for gate in replay['ordered_layers']),
          Path('docs/projects/evidence/slice7-g5-release-refusal/linux/original-test.txt'),
          Path('docs/projects/evidence/slice7-g5-release-refusal/linux/original.xml'),
          Path('docs/projects/evidence/slice7-raylib/archive-sha256.txt'),
          Path('docs/projects/evidence/slice7-raylib/windows/software-result.json'),
          Path('docs/projects/evidence/slice7-raylib/failures/linux-mesa-address-attribution.txt'),
          *(evidence/file for file in ('source-inputs.json','source-archive.txt','dependency-inputs.json','layer-roundtrip.json',
               'inventory.txt','integration-paths.txt','slice7-product-paths.txt','workspace.txt','result-summary.json','git-state.txt',
               'public-workflow.py','audit-layers.py','inventory.py','linux/run-linux.sh','linux/run-public.sh','linux/isolate-ctest.py',
               'windows/run.py','windows/run.cmd','windows/resume.py','windows/resume.cmd','windows/public-windows.py',
               'windows/configure-second/packet-verified.json','windows/configure-second/HostDependencies-attested.cmake',
               'windows/configure-second/attested-include.cmake','windows/dependency-inputs.json','windows/public-results.json',
               'windows/workspace-cleanup.json','windows/Debug-symbols-initial.json','windows/Debug-symbols-final.json','test-scope.json','summarize-results.py','collect-and-clean.py',
               'linux/isolated-package.sh','linux/rerun-linux.sh','linux/test-source-isolation.txt',
               'linux/test-evidence-directories.txt',
               'review-evidence-paths.txt'))}
for gate in ('slice7-raylib','slice7-g5'):
    base=Path('docs/projects/evidence')/gate
    receipts=sorted([*base.glob('linux/*/*/result.json'),*base.glob('windows/*/*/result.json')])
    assert len(receipts)==20 and all(json.loads(p.read_text())['exit']==0 for p in receipts)
    required.update(receipts)
for file in evidence.rglob('*'):
    if not file.is_file():
        continue
    if file.suffix=='.xml' or file.name.endswith(('-public.txt','-result.json','-ctest.txt','-selection.json',
        '-test-plan.json','-configure.txt','-public-results.json','-failure.txt')):
        required.add(file)
assert all(p.is_file() or p.name in ('inventory.txt','review-evidence-paths.txt','git-state.txt') for p in required), sorted(p for p in required if not p.exists())
lines(evidence/'review-evidence-paths.txt',required)

def category(name):
    path=Path(name)
    if path in explicit_reports:
        return 'required project documentation'
    if path in required:
        return 'required review evidence'
    if name.startswith(('docs/projects/evidence/','docs/log/evidence/')):
        return 'retained optional evidence'
    if name.startswith('docs/projects/'):
        return 'retained project report/research'
    if name.startswith('docs/'):
        return 'user/developer documentation'
    if name.startswith('examples/'):
        return 'application sample'
    if name.startswith('third_party/'):
        return 'vendored build input'
    if name.startswith('tests/'):
        return 'test and fixture'
    return 'build, source or tooling'

snapshot=evidence/'inventory.txt'
snapshot.touch(exist_ok=False)
(evidence/'git-state.txt').write_text(
    'HEAD '+opening['head']+'\n'+
    'Index tuple SHA-256 '+opening['index']+'\n'+
    f'Index entries {index.count(bytes([0]))}\n'+
    'git diff --cached --binary HEAD -- output bytes 0\n'+
    'git diff --check passed\n'+
    git('diff','--shortstat','HEAD','--').decode())
status=git('status','--porcelain=v1','-z','--untracked-files=all').split(b'\0')[:-1]
paths={os.fsdecode(row[3:]):row[:2].decode() for row in status}
new=set(paths)-opening['entries'].keys()
assert new=={str(report),*(str(p) for p in evidence.rglob('*') if p.is_file())},new
assert all(paths[name]==state for name,(state,_) in opening['entries'].items())
summary=dict(opening=len(opening['entries']),new_audit_paths=len(new),proposed_integration_paths=len(integration),
             required_review_paths=len(required),status=Counter(paths.values()),
             categories=Counter(category(p) for p in paths))
rows=[
    'HEAD '+opening['head'],
    'Index tuple SHA-256 '+opening['index'],
    'Accepted working-tree paths verified byte-for-byte. Staged diff empty.',
    json.dumps(summary,sort_keys=True),'',
    'STATUS | CATEGORY | ACCEPTED LAYER | EXACT PATH | OPENING SHA-256 | FINAL SHA-256',
]
for name,state in sorted(paths.items()):
    before=opening['entries'].get(name,('', '-'))[1]
    after='(self)' if name==str(snapshot) else digest(name)
    layer=prior_layers.get(name,'Slice 7 integrated audit')
    rows.append(f'{state} | {category(name)} | {layer} | {name} | {before} | {after}')
snapshot.write_text('\n'.join(rows)+'\n')
print(json.dumps(summary,sort_keys=True))
