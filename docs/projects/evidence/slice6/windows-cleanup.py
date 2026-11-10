"""End-of-task cleanup for the two verified Slice 6 Windows overlays."""
from pathlib import Path
import csv
import hashlib
import json
import shutil
import subprocess
import traceback

out = Path(r'\\vboxsvr\VM\project-slice6')
home = Path.home().resolve(strict=True)
system = Path(home.anchor)
baseline = home / 'VoidCI42377496702-Native'
report = {'system_drive': str(system), 'free_bytes_before': shutil.disk_usage(system).free,
          'removed_roots': [], 'removed_entries': [], 'retained': [], 'binary_hashes': {}}


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def alias(path):
    return path.is_symlink() or path.is_junction()


try:
    first = json.loads((out / 'native-results.json').read_text())
    final = json.loads((out / 'final/native-results.json').read_text())
    assert first['restored_original_source'] and final['restored_original_source']
    assert final['status'] == 'PASS' and {item['profile'] for item in final['profiles']} == {'Release', 'Debug'}
    assert all(item['status'] == 'PASS' and item['os_build'] == 19045 for item in final['profiles'])
    assert (out / 'final/Release-ctest.xml').is_file() and (out / 'final/Debug-ctest.xml').is_file()
    assert (out / 'preserved-evidence.json').is_file()
    receipt = json.loads((out / 'preserved-evidence.json').read_text())
    for name, expected in receipt.items():
        assert digest(out / name) == expected, name
    tasks = subprocess.run(['tasklist.exe', '/FO', 'CSV', '/NH'], capture_output=True, text=True, check=True)
    names = {row[0].lower() for row in csv.reader(tasks.stdout.splitlines()) if row}
    assert not names.intersection({'voidc.exe', 'voidls.exe', 'voidup.exe', 'ninja.exe', 'ctest.exe', 'cmake.exe', 'clang-cl.exe', 'clang.exe', 'lld-link.exe', 'void_numeric_enum_ir.exe', 'void_project_creation.exe'}), names
    for result in final['profiles']:
        build = home / ('VoidS6-Final-' + result['profile'])
        assert build.resolve(strict=True) == build and not alias(build)
        report['retained'].append({'path': str(build), 'reason': 'qualified binaries and reusable build tree'})
        for binary, expected in result['binaries'].items():
            path = build / (binary + '.exe')
            assert digest(path) == expected
            report['binary_hashes'][str(path)] = expected
    for leaf in ('VoidS6-source-packet', 'VoidS6-source-final'):
        path = home / leaf
        assert path.resolve(strict=True) == path and (path / 'tests/numeric/enums.py').is_file()
        report['retained'].append({'path': str(path), 'reason': 'source packet and test harness'})
    for result, area in ((first, out), (final, out / 'final')):
        archive = area / 'source-packet.zip'
        assert digest(archive) == result['source_packet_sha256']
        report['retained'].append({'path': str(archive), 'reason': 'verified source packet archive'})
    candidates = []
    for result, leaf in ((first, 'VoidS6-native-backup'), (final, 'VoidS6-native-final-backup')):
        target = home / leaf
        assert target.resolve(strict=True) == target and not alias(target)
        expected = {}
        for name, sha in result['before_overlay_sha256'].items():
            original = baseline / name
            assert original.is_relative_to(baseline) and '..' not in Path(name).parts
            if sha is None:
                assert not original.exists() and not alias(original), name
            else:
                assert digest(original) == sha and not alias(original), name
                expected[name] = sha
        entries = sorted(target.rglob('*'))
        assert all(path.resolve(strict=True) == path and not alias(path) for path in entries)
        files = {path.relative_to(target).as_posix(): digest(path) for path in entries if path.is_file() and path.stat().st_nlink == 1}
        assert files == expected, (target, set(files).symmetric_difference(expected))
        assert all(path.is_dir() or path.is_file() for path in entries)
        candidates.append((target, entries))
    for target, entries in candidates:
        for path in sorted(entries, key=lambda item: len(item.parts), reverse=True):
            if path.is_dir():
                path.rmdir()
            else:
                path.unlink()
            report['removed_entries'].append(str(path))
        target.rmdir()
        report['removed_roots'].append(str(target))
    for leaf in ('VoidS6-tmp-Release', 'VoidS6-tmp-Debug'):
        target = home / leaf
        assert target.resolve(strict=True) == target and not alias(target)
        if any(target.iterdir()):
            report['retained'].append({'path': str(target), 'reason': 'nonempty test directory retained for inspection',
                                       'entries': sorted(str(item) for item in target.iterdir())})
        else:
            target.rmdir()
            report['removed_roots'].append(str(target))
    report['editor_executable_lookup'] = {}
    for binary in ('code', 'node'):
        check = subprocess.run(['where.exe', binary], capture_output=True, text=True)
        report['editor_executable_lookup'][binary] = {'returncode': check.returncode,
                                                      'paths': check.stdout.splitlines()}
    report['status'] = 'PASS'
except BaseException:
    report['status'] = 'RETAINED_OR_PARTIAL'
    report['error'] = traceback.format_exc()
finally:
    report['free_bytes_after'] = shutil.disk_usage(system).free
    (out / 'cleanup-results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
