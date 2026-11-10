"""Verify the accepted working tree and record the integration delta."""
from collections import Counter
from hashlib import file_digest, sha256
from pathlib import Path
import json, os, subprocess

root=Path.cwd()
work=root/'build/slice7-final-link-repair'
evidence=Path('docs/projects/evidence/slice7-final-link-repair')
prior=Path('docs/projects/evidence/slice7-final-integrated')
report='docs/projects/SLICE7_FINAL_CLOSEOUT.md'
opening=json.loads((work/'opening.json').read_text())
approved=set(opening['repair_paths'])
before=opening['paths']

def digest(path):
    with Path(path).open('rb') as stream:
        return file_digest(stream,'sha256').hexdigest()

def git(*args):
    return subprocess.check_output(['git','--no-optional-locks',*args])

index=git('ls-files','--stage','-z')
assert git('rev-parse','HEAD').decode().strip()==opening['head']
assert sha256(index).hexdigest()==opening['index_sha256']
assert index.count(b'\0')==opening['index_entries']
assert not git('diff','--cached','--binary','HEAD','--')
git('diff','--check')
git('apply','--reverse','--check',str(evidence/'source.patch'))
for name in approved:
    old=work/'before'/name
    if name in before:
        assert digest(old)==before[name][3]
    else:
        assert sha256(git('show','HEAD:'+name)).hexdigest()==digest(old)
    assert digest(name)!=digest(old)

integration=set((prior/'integration-paths.txt').read_text().splitlines())|approved
(evidence/'integration-paths.txt').write_text('\n'.join(sorted(integration))+'\n')
assert all(Path(name).is_file() for name in integration)
required=set((prior/'review-evidence-paths.txt').read_text().splitlines())
required|={str(evidence/name) for name in ('inventory.txt','integration-paths.txt','review-evidence-paths.txt',
           'git-state.txt','recipe-scope.json','result-summary.json','workspace.txt','source.patch',
           'verify-source-delta.py','finish-qualification.py','inventory.py','test-source-inputs.json',
           'linux/prepare-build.py','linux/prepare-test-sources.py','linux/isolate-full-tests.py','linux/run-profile.sh',
           'linux/run-native-full.sh','linux/run-public.sh','linux/isolated-package.sh')}
for name in evidence.rglob('*'):
    if name.is_file() and (name.suffix=='.xml' or name.name.endswith((
        '-clone.json','-inventory.json','-plan.json','-registered.cmake.txt','-public.txt','-isolated-package.txt',
        '-focused.txt','-full.txt','-LastTest.txt','-exit.txt','-environment.txt'))):
        required.add(str(name))
(evidence/'review-evidence-paths.txt').write_text('\n'.join(sorted(required))+'\n')
(evidence/'git-state.txt').write_text(
    f"HEAD {opening['head']}\nIndex tuple SHA-256 {opening['index_sha256']}\n"
    f"Index entries {opening['index_entries']}\nStaged diff bytes 0\n"
    'git diff --check and reverse application check for the source patch passed.\n'
)
inventory=evidence/'inventory.txt'
inventory.touch(exist_ok=False)
assert all(Path(name).is_file() for name in required)
rows=git('status','--porcelain=v1','-z','--untracked-files=all').split(b'\0')[:-1]
paths={os.fsdecode(row[3:]):row[:2].decode() for row in rows}
added=set(paths)-before.keys()
assert added==(approved-before.keys())|{str(p) for p in evidence.rglob('*') if p.is_file()},added
assert before.keys()<=paths.keys()
counts=dict(accepted_opening=len(before),amended_existing_paths=len(approved & before.keys()),
    newly_modified_tracked_paths=len(approved-before.keys()),new_evidence_paths=len(added-approved),
    integration_paths=len(integration),required_review_paths=len(required),status=Counter(paths.values()))
result=[
    f"HEAD {opening['head']}", f"Index tuple SHA-256 {opening['index_sha256']}",
    'The opening index matches, and the staged diff is empty.',
    'All opening paths outside the authorized source and report amendments are byte-identical.',
    json.dumps(counts,sort_keys=True),'',
    'STATUS | CATEGORY | LAYER | EXACT PATH | OPENING SHA-256 | FINAL SHA-256',
]
for name,state in sorted(paths.items()):
    current='(self)' if name==str(inventory) else digest(name)
    if name in before:
        old_state,category,layer,old=before[name]
        assert state==old_state and (old!=current)==(name in approved),name
        if name in approved:
            layer+=' + Slice 7 Linux AOT link repair'
    elif name in approved:
        assert state==' M'
        category,layer,old='test and fixture','HEAD + Slice 7 Linux AOT link repair',digest(work/'before'/name)
    else:
        assert state=='??'
        category='required review evidence' if name in required else 'retained qualification evidence'
        layer,old='Slice 7 Linux AOT link repair','-'
    result.append(f'{state} | {category} | {layer} | {name} | {old} | {current}')
inventory.write_text('\n'.join(result)+'\n')
print(json.dumps(counts,sort_keys=True))
