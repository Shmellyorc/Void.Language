from hashlib import file_digest
from pathlib import Path
import json
import os
import stat

repo = Path.cwd().resolve()
evidence = repo/'docs/projects/evidence/slice7-g4-linux'
scratch = repo/'build/slice7-g4-linux'
targets = {}
directories = {scratch}
retained = {}
def digest(path):
    with path.open('rb') as raw:
        return file_digest(raw,'sha256').hexdigest()

def match(path, expected):
    assert path.read_bytes() == expected, path
    targets[path] = digest(path)
    directories.add(path.parent)

assert sorted(path.name for path in scratch.iterdir()) == ['debug.status','release.status','strict.status']
for profile in ('release','debug','strict'):
    status = scratch/f'{profile}.status'
    match(status, b'0\n')
    for row in (evidence/'linux'/f'{profile}-artifacts.sha256').read_text().splitlines():
        expected, name = row.split('  ',1)
        path = repo/name
        assert path.resolve(strict=True) == path and digest(path) == expected,path
        retained[str(path)] = expected
    build = repo/'build'/f'llvm23-{profile}'
    g1 = build/'ffi-g1/tests/probe.void'
    match(g1,b'use "/native_primitives.void" as c\nasync func entry(value: i32) -> i32:\n'
             b'    return c.signed32(value)\n')
    g3 = build/'ffi-g3/tests/resources.void'
    match(g3,(repo/'tests/ffi/resources.void').read_bytes().replace(b' + 7',b' + 8',1))
    g4 = build/'ffi-g4'
    got = build/'ffi-g4-got'
    expected_got = {got/name for name in ('closure.txt','provider-got.o','native/libvoid_g4_main.so',
                                          'native/libvoid_g4_dep.so')}
    for row in (evidence/'linux'/f'{profile}-got.txt').read_text().splitlines()[-4:]:
        expected, name = row.split('  ',1)
        path = Path(name)
        assert path in expected_got and path.resolve(strict=True) == path and digest(path) == expected,path
        expected_got.remove(path)
        retained[name] = expected
    assert not expected_got
    for probe, images in ((g4/'jit-probes',g4),(got/'probes',got)):
        directories.add(probe)
        match(probe/'dynamic.void',(repo/'tests/ffi/dynamic.void').read_bytes().replace(b' + 7',b' + 8',1))
        match(probe/'binding_authority.void',b'use "/native_dynamic.void" as ffi\n\n'
              b'func entry(value: i32) -> i32:\n    return ffi.add(value, 0)\n')
        for label, source in (('negative','initializer'),('replacement','outside')):
            match(probe/label/'libvoid_g4_main.so',(images/'native/libvoid_g4_main.so').read_bytes())
            match(probe/label/'libvoid_g4_dep.so',(g4/source/'libvoid_g4_dep.so').read_bytes())

for path in set(targets) | directories:
    info = path.lstat()
    assert path.resolve(strict=True) == path and info.st_uid == os.getuid(),path
    if path in directories:
        assert stat.S_ISDIR(info.st_mode)
        assert set(path.iterdir()) == {child for child in set(targets) | directories if child.parent == path},path
    else:
        assert stat.S_ISREG(info.st_mode) and info.st_nlink == 1,path
for process in Path('/proc').iterdir():
    if not process.name.isdecimal():
        continue
    try:
        descriptors = list((process/'fd').iterdir())
    except OSError:
        descriptors = []
    for path in [process/'cwd',*descriptors]:
        try:
            current = os.readlink(path)
        except OSError:
            continue
        assert not any(current == str(root) or current.startswith(str(root)+'/') for root in directories),current

receipt = evidence/'host-cleanup.json'
report = {'status':'verified','approved_files':{str(path):hash_ for path,hash_ in targets.items()},
          'approved_directories':sorted(map(str,directories)), 'deleted':[], 'retained':retained}
def save():
    receipt.write_text(json.dumps(report,indent=2)+'\n')
save()
for path, expected in targets.items():
    assert digest(path) == expected,path
    path.unlink()
    report['deleted'].append(str(path))
for path in sorted(directories,key=lambda item:len(item.parts),reverse=True):
    path.rmdir()
    report['deleted'].append(str(path))
report['status'] = 'PASS'
save()
print(f"Removed {len(report['deleted'])} exact scratch paths. Verified {len(retained)} retained artifacts.")
