from pathlib import Path
from hashlib import sha256
from zipfile import ZipFile
from difflib import unified_diff
import json
import subprocess

base = Path('docs/projects/evidence')
prior = base/'slice7-g4/inventory.txt'
audit = base/'slice7-g4-linux'
files = ('CMakeLists.txt','include/void/c_dynamic.hpp','include/void/c_primitive.hpp',
         'include/void/execution.hpp','include/void/runtime.hpp','src/backend/llvm/backend.cpp',
         'src/dynamic_closure.cpp','src/ir/c_primitive.cpp','src/runtime/abi.cpp',
         'src/runtime/c_dynamic.cpp','tests/ffi/g4_descriptor.h.in')
files = sorted((*files, *(str(x) for x in Path('tests/ffi').glob('dynamic*'))))
assert len(files) == 23, files
inventory = {name: hash_ for row in prior.read_text().splitlines()[7:] if row
             for _, _, name, hash_ in [row.split(' | ',3)]}
changes = {}
patch = []
with ZipFile(Path.home()/'VM/project-slice7-g3-final/source-packet.zip') as archive:
    for name in files:
        new = Path(name).read_bytes()
        old = None
        if name in archive.namelist():
            old = archive.read(name)
            if name in inventory:
                assert sha256(old).hexdigest() == inventory[name], name
            else:
                assert old == subprocess.check_output(['git','show','HEAD:'+name]), name
        else:
            assert name not in inventory
        changes[name] = {'before': sha256(old).hexdigest() if old else None,
                         'after': sha256(new).hexdigest()}
        patch.append(f'diff --git a/{name} b/{name}\n')
        if old is None:
            patch.append('new file mode 100644\n')
        patch.extend(unified_diff((old or b'').decode().splitlines(keepends=True), new.decode().splitlines(keepends=True),
                                 '/dev/null' if old is None else 'a/'+name, 'b/'+name))
(audit/'source.patch').write_text(''.join(patch))
(audit/'source-hashes.json').write_text(json.dumps(changes,indent=2)+'\n')
print(f'{len(files)} exact source/test paths, {sum(v["before"] is None for v in changes.values())} new')
