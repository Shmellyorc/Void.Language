"""Copy declared build outputs and CMake state without copying old test outputs."""
from hashlib import file_digest
from pathlib import Path
import json, subprocess, sys

root=Path.cwd()
mode=sys.argv[1]
assert mode in ('release','debug','strict')
prior=root/'build/slice7-final-integrated'/f'linux-{mode}'
work=root/'build/slice7-final-link-repair'
dest=work/(f'native-linux-{mode}' if sys.argv[2:]==['--native'] else f'linux-{mode}')
ninja=root/'.toolchains/ninja-1.13.2/ninja'
declared=subprocess.check_output([ninja,'-C',prior,'-t','targets','all'],text=True)
names={'.ninja_log','.ninja_deps','CMakeCache.txt','CMakeFiles','build.ninja','cmake_install.cmake','curl'}
for line in declared.splitlines():
    target=Path(line.rsplit(': ',1)[0])
    if target.is_absolute():
        if not target.is_relative_to(prior):
            continue
        target=target.relative_to(prior)
    if target.parts and target.parts[0]!='..' and (prior/target).is_file():
        names.add(target.parts[0])
for path in prior.iterdir():
    if path.is_file() and (path.name.startswith(('native-inventory-inputs-','native-install-','native-resources-install-','void-native','cache-inputs-'))
                           or path.name in ('DartConfiguration.tcl','CTestTestfile.cmake','compile_commands.json')):
        names.add(path.name)
dest.mkdir()
subprocess.run(['cp','-a','--reflink=always','--target-directory',dest,'--',*(prior/n for n in sorted(names))],check=True)

def digest(path):
    with path.open('rb') as stream:
        return file_digest(stream,'sha256').hexdigest()

witness=('voidc','voidls','void_g5_reload','libvoid_runtime.a','libvoid_native_fixture.a',
         'void-native.txt','void-native-resources.txt','CTestTestfile.cmake')
hashes={name:digest(prior/name) for name in witness}
assert all(digest(dest/name)==sha for name,sha in hashes.items())
assert not (dest/'Testing').exists()
print(json.dumps(dict(prior=str(prior),copy=str(dest),top_level_entries=len(names),
    reused_artifacts=hashes,omitted=sorted(p.name for p in prior.iterdir() if p.name not in names)),indent=2))
