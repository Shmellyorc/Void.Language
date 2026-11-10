"""Remove the verified temporary overlay backup and empty storage-audit temp roots."""
from pathlib import Path
import csv
import hashlib
import json
import shutil
import subprocess
import traceback

share = Path(r'\\vboxsvr\VM\project-slice7-g3')
home = Path.home().resolve(strict=True)
system = Path(home.anchor)
baseline = home/'VoidCI42377496702-Native'
report = {'system_drive': str(system), 'free_bytes_before': shutil.disk_usage(system).free,
          'removed_roots': [], 'removed_entries': [], 'retained': [], 'binary_hashes': {},
          'prior_g2_binary_hashes': {}}


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def alias(path):
    return path.is_symlink() or path.is_junction()


try:
    qualification = json.loads((share/'native-results.json').read_text())
    assert qualification['status']=='PASS' and qualification['restored_original_source']
    assert {r['profile'] for r in qualification['profiles']}=={'Release','Debug'}
    assert all(r['status']=='PASS' and r['os_build']==19045 for r in qualification['profiles'])
    for name, expected in json.loads((share/'preserved-evidence.json').read_text()).items():
        assert digest(share/name)==expected, name
    tasks=subprocess.run(['tasklist.exe','/FO','CSV','/NH'],capture_output=True,text=True,check=True)
    running={row[0].lower() for row in csv.reader(tasks.stdout.splitlines()) if row}
    assert not running.intersection({'voidc.exe','voidls.exe','ninja.exe','ctest.exe','cmake.exe',
                                     'clang-cl.exe','clang.exe','lld-link.exe','void_numeric_enum_ir.exe',
                                     'void_fixed_arrays_tests.exe','void_async_tests.exe','void_stage10_fields.exe',
                                     'void_g1_tests.exe','void_g1_aot_normal.exe','void_g1_aot_optimized.exe','void_g1_aot_mismatch.exe',
                                     'void_g2_tests.exe','void_g2_aot_normal.exe','void_g2_aot_optimized.exe',
                                     'void_g3_tests.exe','void_g3_aot_normal.exe','void_g3_aot_optimized.exe',
                                     'void-analysis-diagnostic.exe'}),running
    prior=json.loads((share.parent/'project-slice7-g2/native-results.json').read_text())
    for result, prefix, key in ((qualification,'VoidS7-G3-','binary_hashes'),
                                (prior,'VoidS7-G2-','prior_g2_binary_hashes')):
        for profile in result['profiles']:
            build=home/(prefix+profile['profile'])
            assert build.resolve(strict=True)==build and not alias(build)
            report['retained'].append({'path':str(build),'reason':'qualified native binaries and reusable build tree'})
            for name, expected in profile['binaries'].items():
                binary=build/(name+'.exe')
                assert digest(binary)==expected,binary
                report[key][str(binary)]=expected
            for name, expected in (profile.get('g1_artifacts', {}) | profile.get('g2_artifacts', {}) | profile.get('g3_artifacts', {})).items():
                artifact=build/name
                assert artifact.resolve(strict=True)==artifact and digest(artifact)==expected,artifact
                report[key][str(artifact)]=expected
    archive=share/'source-packet.zip'
    assert digest(archive)==qualification['source_packet_sha256']
    source=home/'VoidS7-G3-source'
    assert source.resolve(strict=True)==source and not alias(source)
    for name in ('tests/ffi/resources.cpp','tests/ffi/resources_thunk.c','tests/ffi/resources_fixture.c','src/runtime/c_resource.cpp'):
        assert digest(source/name)==qualification['source_inputs_sha256'][name]
    report['retained'].extend([{'path':str(archive),'reason':'verified source archive'},
                               {'path':str(source),'reason':'extracted qualification harness'}])

    backup=home/'VoidS7-G3-backup'
    assert backup.resolve(strict=True)==backup and not alias(backup)
    expected={}
    for name, original_hash in qualification['before_overlay_sha256'].items():
        path=baseline/name
        assert path.is_relative_to(baseline) and '..' not in Path(name).parts and not alias(path)
        if original_hash is None:
            assert not path.exists(),path
        else:
            assert digest(path)==original_hash,path
            expected[name]=original_hash
    entries=list(backup.rglob('*'))
    assert all(path.resolve(strict=True)==path and not alias(path) for path in entries)
    actual={path.relative_to(backup).as_posix():digest(path) for path in entries
            if path.is_file() and path.stat().st_nlink==1}
    assert actual==expected
    assert all(path.is_dir() or path.is_file() for path in entries)
    for path in sorted(entries,key=lambda path:len(path.parts),reverse=True):
        if path.is_dir():
            path.rmdir()
        else:
            path.unlink()
        report['removed_entries'].append(str(path))
    backup.rmdir()
    report['removed_roots'].append(str(backup))
    for name in ('VoidS7-G3-tmp-Release','VoidS7-G3-tmp-Debug'):
        path=home/name
        assert path.resolve(strict=True)==path and not alias(path)
        if any(path.iterdir()):
            report['retained'].append({'path':str(path),'reason':'nonempty temp directory retained for review',
                                       'entries':sorted(str(item) for item in path.iterdir())})
        else:
            path.rmdir()
            report['removed_roots'].append(str(path))
    for profile in ('Release','Debug'):
        path=home/('VoidS7-G3-'+profile)/'ffi-g1'/'tests'
        assert path.resolve(strict=True)==path and not alias(path)
        probe=path/'probe.void'
        assert list(path.iterdir())==[probe] and probe.resolve(strict=True)==probe and not alias(probe)
        assert probe.is_file() and probe.stat().st_nlink==1
        assert probe.read_text()==('use "/native_primitives.void" as c\nasync func entry(value: i32) -> i32:\n'
                                   '    return c.signed32(value)\n')
        probe.unlink()
        path.rmdir()
        report['removed_entries'].append(str(probe))
        report['removed_roots'].append(str(path))
    original=(source/'tests/ffi/resources.void').read_text()
    for profile in ('Release','Debug'):
        path=home/('VoidS7-G3-'+profile)/'ffi-g3'/'tests'
        assert path.resolve(strict=True)==path and not alias(path)
        probe=path/'resources.void'
        assert list(path.iterdir())==[probe] and probe.resolve(strict=True)==probe and not alias(probe)
        assert probe.is_file() and probe.stat().st_nlink==1
        assert probe.read_text()==original.replace(' + 7',' + 8',1)
        probe.unlink()
        path.rmdir()
        report['removed_entries'].append(str(probe))
        report['removed_roots'].append(str(path))

    diagnostic=home/'VoidS7-G3-concurrency'
    assert diagnostic.resolve(strict=True)==diagnostic and not alias(diagnostic)
    diag=json.loads((share/'diagnostic-results.json').read_text())
    assert diag['status']=='complete'
    for name, expected in [('analysis-diagnostic.cpp',diag['source']['diagnostic']),
                           ('analysis-diagnostic.obj',diag['artifacts']['diagnostic_object']),
                           ('void-analysis-diagnostic.exe',diag['artifacts']['executable']),
                           ('link.rsp',diag['artifacts']['response'])]:
        assert digest(diagnostic/name)==expected,name
    entries=list(diagnostic.rglob('*'))
    assert all(p.resolve(strict=True)==p and not alias(p) for p in entries)
    for item in sorted(entries,key=lambda path:len(path.parts),reverse=True):
        if item.is_dir():item.rmdir()
        else:
            assert item.is_file() and item.stat().st_nlink==1,item
            item.unlink()
        report['removed_entries'].append(str(item))
    diagnostic.rmdir()
    report['removed_roots'].append(str(diagnostic))
    report['status']='PASS'
except BaseException:
    report['status']='RETAINED_OR_PARTIAL'
    report['error']=traceback.format_exc()
finally:
    report['free_bytes_after']=shutil.disk_usage(system).free
    (share/'cleanup-results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
