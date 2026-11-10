from hashlib import file_digest
from pathlib import Path
import json
import os
import stat

repo = Path.cwd().resolve()
scratch = repo/'build/slice7-g1'
evidence = repo/'docs/projects/evidence/slice7-g1'
receipt = evidence/'host-cleanup.json'

def digest(path):
    with path.open('rb') as stream:
        return file_digest(stream, 'sha256').hexdigest()

profiles = ('release', 'debug', 'strict')
targets = [scratch]
builds = [repo/('build/llvm23-' + p) for p in profiles]
retained = {}
for profile, build in zip(profiles, builds):
    artifact_receipt = json.loads((evidence/'linux'/f'{profile}-artifacts.json').read_text())
    for name, expected in artifact_receipt['artifacts'].items():
        binary = build/name
        assert binary.resolve(strict=True) == binary and digest(binary) == expected, binary
        retained[str(binary)] = expected
    for name in ('tests', 'direct-replay', 'evidence-replay'):
        path = build/'ffi-g1'/name
        if path.exists():
            assert sorted(p.name for p in path.iterdir()) == ['probe.void'], path
            text = (path/'probe.void').read_text()
            assert text == ('use "/native_primitives.void" as c\nasync func entry(value: i32) -> i32:\n'
                            '    return c.signed32(value)\n'), path
            targets.append(path)

for name in ('linux-release-g1-first', 'linux-release-g1-diagnostic-field-original', 'linux-debug-g1-ctest-first'):
    assert digest(scratch/(name + '.log')) == digest(evidence/'original'/(name + '.txt'))
assert digest(scratch/'qualify-linux.sh') == digest(evidence/'replay-linux.sh')
assert digest(scratch/'collect-linux.py') == digest(evidence/'collect-linux.py')
assert digest(scratch/'inventory.py') == digest(evidence/'inventory.py')
for profile in profiles:
    for suffix in ('configure.txt', 'build.txt', 'ctest.txt', 'ctest.xml',
                   'artifacts.json', 'primitives.descriptor', 'primitives.void'):
        name = profile + '-' + suffix
        assert digest(scratch/'final'/name) == digest(evidence/'linux'/name), name

source_hashes = json.loads((evidence/'source-hashes.json').read_text())
for file in (scratch/'starting').rglob('*'):
    if file.is_file():
        name = file.relative_to(scratch/'starting').as_posix()
        assert digest(file) == source_hashes[name]['before']
share = Path('/home/shmellyorc/VM/project-slice7-g1')
native = json.loads((evidence/'windows/native-results.json').read_text())
assert native['status'] == 'PASS' and native['restored_original_source']
archive = share/'source-packet.zip'
assert digest(archive) == native['source_packet_sha256']
retained[str(archive)] = native['source_packet_sha256']

approved = []
for target in targets:
    assert target.resolve(strict=True) == target
    for path in [target, *target.rglob('*')]:
        info = path.lstat()
        assert path.resolve(strict=True) == path and info.st_uid == os.getuid(), path
        if stat.S_ISDIR(info.st_mode):
            approved.append({'path': str(path), 'kind': 'directory'})
        else:
            assert stat.S_ISREG(info.st_mode) and info.st_nlink == 1, path
            approved.append({'path': str(path), 'kind': 'file', 'sha256': digest(path)})

for process in Path('/proc').iterdir():
    if not process.name.isdecimal():
        continue
    try:
        descriptors = list((process/'fd').iterdir())
    except OSError:
        descriptors = []
    for item in (process/'cwd', *descriptors):
        try:
            resolved = os.readlink(item)
        except OSError:
            continue
        assert not any(resolved == str(target) or resolved.startswith(str(target)+'/') for target in targets), resolved

result = {'roots': [str(path) for path in targets], 'approved': approved,
          'deleted': [], 'retained_artifact_hashes': retained}
def save():
    receipt.write_text(json.dumps(result, indent=2) + '\n')
save()
for entry in sorted(approved, key=lambda e: len(Path(e['path']).parts), reverse=True):
    path = Path(entry['path'])
    if entry['kind'] == 'directory':
        path.rmdir()
    else:
        assert digest(path) == entry['sha256']
        path.unlink()
    result['deleted'].append(str(path))
result['status'] = 'PASS'
save()
print(f"Removed {len(result['deleted'])} exact entries. Retained {len(retained)} artifacts.")
