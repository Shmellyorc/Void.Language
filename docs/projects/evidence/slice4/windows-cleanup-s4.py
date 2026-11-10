"""Reclaim this slice's disposable VM test trees after local qualification."""
from pathlib import Path
import hashlib, json, shutil, traceback

out=Path(r'\\vboxsvr\VM\project-slice4')
home=Path.home()
record={'status':'running','removed':[],'errors':[]}
def save():(out/'cleanup-s4-results.json').write_text(json.dumps(record,indent=2)+'\n')
def sha(path):
    with path.open('rb') as file:return hashlib.file_digest(file,'sha256').hexdigest()
try:
    qualification=json.loads((out/'allocator-resumed-results.json').read_text())
    original=json.loads((out/'native-results.json').read_text())
    assert qualification['status']=='PASS' and original['restored_original_source']
    assert all((out/(name+'-ctest.xml')).is_file() for name in ('Release','Debug'))
    assert all((out/(name+'-corrected-process.json')).is_file() for name in ('Release','Debug'))
    record['retained_receipts']={}
    for profile in ('Release','Debug'):
        build=home/('VoidS4-Final-'+profile)
        assert sha(build/'voidc.exe')==qualification['compiler_sha256'][profile]
        for name in ('CMakeCache.txt','void-native.txt','void-native-resources.txt',
                     'project-discovery-evidence.json','project-protocol-evidence.json','project-profiles-evidence.json'):
            source=build/name
            if not source.is_file():continue
            target=out/(profile+'-retained-'+name)
            shutil.copyfile(source,target)
            record['retained_receipts'][target.name]=sha(target)
            assert sha(source)==record['retained_receipts'][target.name]
    baseline=home/'VoidCI42377496702-Native'
    for name,digest in original['before_overlay_sha256'].items():
        original_file=baseline/name
        assert (not original_file.exists() if digest is None else sha(original_file)==digest),name
    record['restored_source_paths_verified']=len(original['before_overlay_sha256'])
    record['free_bytes_before']=shutil.disk_usage(home).free
    save()
    for name in (
        'VoidS4-Release','VoidS4-Debug','VoidS4-tmp-Release','VoidS4-tmp-Debug',
        'VoidS4-Final-Release','VoidS4-Final-Debug','VoidS4-Allocator-Rerun',
        'VoidS4-Allocator-Final','VoidS4-source2','VoidS4-native-backup',
        'VoidS4-native-retry-backup','VoidS4-native-lean-backup',
    ):
        path=home/name
        try:
            assert path.parent==home and not path.is_symlink() and not path.is_junction(),str(path)
            if path.exists():
                assert path.is_dir(),str(path)
                shutil.rmtree(path)
                assert not path.exists(),str(path)
                record['removed'].append(str(path))
        except BaseException:record['errors'].append({'path':str(path),'error':traceback.format_exc()})
        save()
    artifacts=out/'allocator-local-artifacts'
    for profile in ('Release','Debug'):
        assert sha(artifacts/profile/'void_project_application-allocator.exe')==qualification['profiles'][profile]['binary_sha256']
    assert not artifacts.is_symlink() and not artifacts.is_junction()
    shutil.rmtree(artifacts)
    record['removed'].append(str(artifacts))
    record['free_bytes_after']=shutil.disk_usage(home).free
    record['status']='PASS' if not record['errors'] else 'PARTIAL'
except BaseException:record['status']='FAIL';record['error']=traceback.format_exc()
finally:save()
