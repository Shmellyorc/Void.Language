"""Verify results, retained compiler identity, and task-owned scratch cleanup."""
from hashlib import file_digest
from pathlib import Path
import json, re, shutil, subprocess, xml.etree.ElementTree as ET

root=Path.cwd()
work=root/'build/slice7-final-link-repair'
prior=root/'build/slice7-final-integrated'
evidence=root/'docs/projects/evidence/slice7-final-link-repair'
linux=evidence/'linux'
profiles={'release':443,'debug':456,'strict':433}
recipes=set(json.loads((evidence/'recipe-scope.json').read_text())['registrations'])

def digest(path):
    with Path(path).open('rb') as stream:
        return file_digest(stream,'sha256').hexdigest()

def receipt(path):
    suite=ET.parse(path).getroot()
    cases=suite.findall('testcase')
    tests={case.get('name'):('fail' if case.find('failure') is not None else
                            'skip' if case.find('skipped') is not None or case.get('status')=='notrun'
                            else 'pass') for case in cases}
    assert len(tests)==int(suite.get('tests'))==len(cases)
    return tests

summary={}
for mode,count in profiles.items():
    clone=json.loads((linux/f'{mode}-clone.json').read_text())
    native_build=work/f'native-linux-{mode}'
    native_inventory=json.loads((linux/f'{mode}-native-inventory.json').read_text())
    focus=receipt(linux/f'{mode}-focused.xml')
    full=receipt(linux/f'{mode}-full.xml')
    assert focus==dict.fromkeys(recipes,'pass'),focus
    assert len(full)==count and all(result=='pass' for result in full.values()),full
    full_suite=ET.parse(linux/f'{mode}-full.xml').getroot()
    assert all(full_suite.get(name)=='0' for name in ('failures','skipped','disabled'))
    plans={}
    for phase in ('plan','native-plan','full-plan'):
        plan=json.loads((linux/f'{mode}-{phase}.json').read_text())['tests']
        assert len(plan)==count and {test['name'] for test in plan}==full.keys()
        plans[phase]=plan
    for canonical,executed in zip(plans['native-plan'],plans['full-plan'],strict=True):
        name=canonical['name']
        assert executed['name']==name
        expected={p['name']:p['value'] for p in canonical['properties']}
        if name=='lsp_collections_stdlib':
            expected['ENVIRONMENT']=[f'VOID_TEST_STDLIB={root}/stdlib']
        if name=='text_encoding_semantic_depth':
            mirror=work/f'test-source-{mode}'
            expected['ENVIRONMENT']=[f'PYTHONPATH={mirror}/tests/text_encoding',
                                     f'VOID_CRASH_EVIDENCE={mirror}/docs/text-encoding/evidence/crashes']
        assert {p['name']:p['value'] for p in executed['properties']}==expected,name
    assert native_inventory['registered_tests']==count
    assert native_inventory['unchanged_registration_properties']
    assert set(native_inventory['recipe_changes'])==recipes
    gate_tests={name:result for name,result in full.items()
                if re.match(r'^ffi_g[1-5]_|^runtime_callbacks$|^cli_install',name)}
    assert all(name in gate_tests for name in ('ffi_g4_conventional','ffi_g4_jit_ir','ffi_g4_aot_startup','ffi_g5_callbacks',
                                                'runtime_callbacks','cli_install'))
    for phase in ('focused','public'):
        assert (linux/f'{mode}-{phase}-exit.txt').read_text().strip()=='0'
    raw_last_test=(linux/f'{mode}-full-LastTest.txt').read_text().splitlines()
    assert raw_last_test.count('Test Passed.')==count
    launcher_exit=linux/f'{mode}-full-exit.txt'
    if not launcher_exit.exists():
        launcher_exit=linux/'launcher-output-first'/launcher_exit.name
    launcher_status=launcher_exit.read_text().strip()
    assert launcher_status=='0' or (mode in ('release','strict') and launcher_status=='2'
        and launcher_exit.parent.name=='launcher-output-first')
    public=(linux/f'{mode}-public.txt').read_text()
    isolated=(linux/f'{mode}-isolated-package.txt').read_text()
    assert public.splitlines().count('RAYLIB trace invoked VOID')==4
    assert 'Integrated installed workflow passed' in public
    assert isolated.splitlines().count('RAYLIB trace invoked VOID')==2
    assert 'SLICE7 installed ordinary application' in isolated
    same={}
    native={}
    for name,expected in clone['reused_artifacts'].items():
        assert digest(prior/f'linux-{mode}'/name)==expected,name
        if name!='CTestTestfile.cmake':
            assert digest(work/f'linux-{mode}'/name)==expected,name
            same[name]=expected
            native[name]=digest(native_build/name)
    manifest=(native_build/'void-native.txt').read_text().splitlines()
    assert manifest[0]=='VOID native 23.1.4 v2'
    assert manifest[3:5]==[str(native_build/name) for name in ('libvoid_runtime.a','libvoid_native_fixture.a')]
    assert manifest[5:7]==native_inventory['host_archives']
    full_text=(linux/f'{mode}-full.txt').read_text().splitlines()
    failed_attempts=[line for line in full_text if re.search(r'\*\*\*(Failed|Timeout|Exception)',line)]
    summary[mode]=dict(focused_passed=len(focus),full_passed=len(full),failures=0,skipped=0,
        full_run_environment=(linux/f'{mode}-full-environment.txt').read_text().strip(),
        launcher_exit_receipt=str(launcher_exit.relative_to(evidence)),
        launcher_exit_status=int(launcher_status),
        failed_attempts_recovered_in_full_run=failed_attempts,
        gates_and_install=gate_tests,public_workflow='passed',isolated_package='passed',
        registered_host_archives={name:digest(name) for name in native_inventory['host_archives']},
        native_full_run_artifacts=native,
        native_full_wall_seconds=float(full_suite.get('time')),
        raw_focused_sha256=digest(linux/f'{mode}-focused.xml'),
        raw_full_sha256=digest(linux/f'{mode}-full.xml'),identical_qualified_artifacts=same)

source_inputs=json.loads((evidence/'test-source-inputs.json').read_text())['test_sources']
for name,sha in source_inputs.items():
    assert digest(root/name)==digest(work/'source'/name)==sha,name
    for mode in profiles:
        assert digest(work/f'test-source-{mode}'/name)==sha,name
(evidence/'result-summary.json').write_text(json.dumps(summary,indent=2)+'\n')
rows=['The prior qualification binaries, Runtime/fixture archives, native manifests and test registrations retain their recorded hashes.',
      'The focused and public replay build copies retain the same compiler, Runtime, callback harness and native manifest bytes.',
      'The result summary records native full-suite build hashes and checks profile host archives against both CTest and the native manifest.']
for marker in sorted(linux.rglob('*-temp.txt')):
    path=Path(marker.read_text().strip())
    assert path.parent==Path('/tmp') and re.fullmatch(
        r'void-s7-link-(release|debug|strict)-(focused|full|public)\.[A-Za-z0-9]+',path.name),path
    remnant=work/'retained-temp-remnants'/path.name
    archive=remnant.with_suffix(remnant.suffix+'.tar')
    if path.exists():
        assert path.is_dir() and not path.is_symlink()
        if any(path.iterdir()):
            archive.parent.mkdir(exist_ok=True)
            assert not archive.exists()
            tar_args=['--file',str(archive),'-C',str(path.parent)]
            subprocess.run(['tar','--create',*tar_args,'--',path.name],check=True)
            subprocess.run(['tar','--compare',*tar_args],check=True)
            shutil.rmtree(path)
            if remnant.exists():
                assert remnant.is_dir() and not remnant.is_symlink()
                shutil.rmtree(remnant)
        else:
            path.rmdir()
    if archive.exists():
        rows.append(f'Retained and compared diagnostic archive: {archive} SHA-256 {digest(archive)}')
    elif remnant.exists():
        rows.append(f'Retained remaining diagnostic files in {remnant}')
    assert not path.exists()
    rows.append(f'Task scratch verified absent: {path}')
probe=work/'reflink-probe'
assert not probe.is_symlink()
probe.unlink(missing_ok=True)
assert not probe.exists()
rows.append(f'Copy-on-write probe verified absent: {probe}')
rows.append(f'Retained build copies, installed packages, test sources and test output: {work}')
rows.append('The focused and public build copies replay at their configured paths in private mounts.')
rows.append('The retained native full-suite builds run directly on the host.')
(evidence/'workspace.txt').write_text('\n'.join(rows)+'\n')
for mode,entry in summary.items():
    print(mode,entry['focused_passed'],entry['full_passed'],entry['public_workflow'],entry['isolated_package'])
print('\n'.join(rows))
