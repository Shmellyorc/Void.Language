from hashlib import file_digest
from pathlib import Path
import json
import os
import stat

repo = Path.cwd().resolve()
audit = repo/'docs/projects/evidence/slice7-g3-final'
scratch = repo/'build/slice7-g3-final'
original = scratch/'original'
entries = {}
directories = [original, scratch]
retained = {}
def digest(path):
    with path.open('rb') as f:
        return file_digest(f, 'sha256').hexdigest()

assert sorted(p.name for p in scratch.iterdir()) == ['debug.status','original','release.status','strict.status']
assert not list(original.iterdir())
native = json.loads((audit/'windows/native-results.json').read_text())
assert native['status'] == 'PASS' and native['restored_original_source']
packet = Path.home()/'VM/project-slice7-g3-final/source-packet.zip'
assert digest(packet) == native['source_packet_sha256']
retained[str(packet)] = native['source_packet_sha256']
for log in ('release-ctest.txt','release-LastTest.txt','corrected-ctest.txt','corrected-LastTest.txt','corrected-sha256.txt'):
    assert (audit/'original'/log).stat().st_size > 0
for profile in ('release','debug','strict'):
    status = scratch/f'{profile}.status'
    assert status.read_text() == '0\n'
    entries[status] = digest(status)
    artifacts = audit/'linux'/f'{profile}-artifacts.sha256'
    for row in artifacts.read_text().splitlines():
        hash_, name = row.split('  ', 1)
        path = repo/name
        assert path.resolve(strict=True) == path and digest(path) == hash_
        retained[str(path)] = hash_
    build = repo/'build'/f'llvm23-{profile}'
    for gate, name, content in (
        ('ffi-g1','probe.void','use "/native_primitives.void" as c\nasync func entry(value: i32) -> i32:\n'
                               '    return c.signed32(value)\n'),
        ('ffi-g3','resources.void',(repo/'tests/ffi/resources.void').read_text().replace(' + 7',' + 8',1)),
    ):
        probe_dir = build/gate/'tests'
        probe = probe_dir/name
        assert list(probe_dir.iterdir()) == [probe] and probe.read_text() == content
        entries[probe] = digest(probe)
        directories.append(probe_dir)

for path in [*entries, *directories]:
    info = path.lstat()
    assert path.resolve(strict=True) == path and info.st_uid == os.getuid(), path
    if path in entries:
        assert stat.S_ISREG(info.st_mode) and info.st_nlink == 1, path
    else:
        assert stat.S_ISDIR(info.st_mode), path
for process in Path('/proc').iterdir():
    if not process.name.isdecimal():
        continue
    try:
        descriptors = list((process/'fd').iterdir())
    except OSError:
        descriptors = []
    for item in (process/'cwd', *descriptors):
        try:
            current = os.readlink(item)
        except OSError:
            continue
        assert not any(current == str(path) or current.startswith(str(path)+'/') for path in directories), current

report = {'status':'verified', 'files':{str(path):hash_ for path,hash_ in entries.items()},
          'directories':[str(path) for path in directories], 'deleted':[], 'retained':retained}
def save():
    (audit/'host-cleanup.json').write_text(json.dumps(report,indent=2)+'\n')
save()
for path, hash_ in entries.items():
    assert digest(path) == hash_, path
    path.unlink()
    report['deleted'].append(str(path))
for path in directories:
    path.rmdir()
    report['deleted'].append(str(path))
report['status'] = 'PASS'
save()
print(f"Removed {len(report['deleted'])} exact scratch paths. Verified {len(retained)} retained artifacts.")
