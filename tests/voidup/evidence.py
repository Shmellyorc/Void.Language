"""Finalize receipts without deleting or relabeling any original failure."""
from pathlib import Path
import hashlib,json,platform,subprocess
ROOT=Path(__file__).resolve().parents[2];E=ROOT/'docs/voidup/evidence/phase_b'
def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda:stream.read(1048576),b''):h.update(block)
    return h.hexdigest()
required=['acceptance-4','ubuntu-2','rejection-3','recovery-2','interruption-1','timeout-3','installed-1','final-2','final-ubuntu-2','lifetime-5','debug-install-1','strict-install-1','matrix-release-3','matrix-debug-3','matrix-strict-3']
qualification={}
for name in required:
    summary=E/(name+'-summary.json');assert summary.is_file(),f'incomplete: {name}'
    rows=[json.loads(line) for line in (E/(name+'.jsonl')).read_text().splitlines()]
    assert rows and all(row['status']==row['expected'] for row in rows),name
    qualification[name]=dict(invocations=len(rows),summary=summary.name,summary_sha256=digest(summary))
baseline=json.loads((E/'baseline.json').read_text());changed=[];missing=[]
for name,old in baseline.items():
    path=ROOT/name
    if not path.is_file():missing.append(name)
    elif digest(path)!=old:changed.append(name)
allowed={'CMakeLists.txt','include/void/filesystem.hpp','include/void/host_ops.hpp','src/runtime/filesystem.cpp','src/runtime/host_abi.cpp','src/runtime/host_directory.cpp','src/runtime/host_directory_windows.cpp','stdlib/io.void','tests/host_capabilities/native.cpp'}
assert not missing and set(changed)==allowed,(missing,changed)
subprocess.run(['git','diff','--exit-code','--','README.md'],cwd=ROOT,check=True,capture_output=True)
preserved=0
for incident in (E/'crashes').glob('incident-*'):
    manifests=list(incident.glob('*.json'))
    for manifest in manifests:
        data=json.loads(manifest.read_bytes())
        for item in data.get('sources',[]):
            assert digest(Path(item['copy']))==item['preserved_sha256']==item['original_sha256']
            preserved+=1
sources={str(path.relative_to(ROOT)):digest(path) for directory in ['apps/voidup','tests/voidup'] for path in (ROOT/directory).rglob('*') if path.is_file() and '__pycache__' not in path.parts}
binary=ROOT/'build/voidup-final-3/bin/voidup'
artifact=dict(schema=1,application=str(binary),sha256=digest(binary),source_hashes=sources,
    source_language='VOID',compiler=str(ROOT/'build/voidup-ubuntu-source/build/ubuntu-release/voidc'),
    compiler_sha256=digest(ROOT/'build/voidup-ubuntu-source/build/ubuntu-release/voidc'),
    actual_platform=platform.platform(),native_platforms_unqualified=['Windows x64','macOS ARM64'])
(E/'final-artifacts.json').write_text(json.dumps(artifact,indent=2)+'\n')
safety=dict(schema=1,baseline_files=len(baseline),missing_original_files=missing,changed_original_files=changed,
    all_other_original_hashes_unchanged=True,phase_a_records_and_evidence_unchanged=True,
    unrelated_website_hashes_unchanged=True,root_readme_git_diff_empty=True,
    verified_preserved_crash_source_copies=preserved,qualification=qualification)
(E/'final-qualification.json').write_text(json.dumps(safety,indent=2)+'\n')
files=[dict(path=path.relative_to(E).as_posix(),bytes=path.stat().st_size,sha256=digest(path)) for path in sorted(E.rglob('*')) if path.is_file() and path.name!='INDEX.json']
(E/'INDEX.json').write_text(json.dumps(dict(schema=1,scope='Phase B; includes original failed attempts, corrected runs and intentional crash probes',files=files,report_sha256=digest(ROOT/'docs/voidup/PHASE_B_REPORT.md')),indent=2)+'\n')
print(f'{len(files)} evidence files indexed; {preserved} preserved crash source copies verified; protected baseline unchanged')
