"""Independent revision-4 mixed resource ABI through real generated C and VOID."""
from hashlib import sha256
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile

compiler, clang = map(lambda value: Path(value).resolve(), sys.argv[1:3])
sanitized = sys.argv[3:] == ['ON']
windows = os.name == 'nt'
fixtures = Path(__file__).resolve().parents[1] / 'ffi'
environment = dict(os.environ)
if sanitized:
    environment.update(ASAN_OPTIONS='halt_on_error=1:abort_on_error=1:detect_leaks=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
def run(*args, **options):
    result = subprocess.run(list(map(str, args)), env=environment, capture_output=True, timeout=180, **options)
    assert result.returncode == 0, (args, result.returncode, result.stdout, result.stderr)
    return result.stdout
with tempfile.TemporaryDirectory(prefix='VOID mixed ABI ') as temporary:
    base = Path(temporary)
    project = base/'project'
    run(compiler,'new',project)
    native = project/'native'; native.mkdir()
    target = 'x86_64-pc-windows-msvc msvc-x64' if windows else 'x86_64-unknown-linux-gnu sysv-x64'
    content = (fixtures/'mixed.descriptor.in').read_text()
    for name,value in dict(TARGET=target,LONG='i32' if windows else 'i64',THUNK=sha256((fixtures/'mixed_fixture.h').read_bytes()).hexdigest(),FIXTURE=sha256((fixtures/'mixed_fixture.c').read_bytes()).hexdigest()).items():
        content=content.replace('@'+name+'@',value)
    descriptor=native/'mixed.descriptor';descriptor.write_bytes(content.encode('ascii'))
    generated=base/'mixed.c'
    run(compiler,'native-bind',descriptor,'mixed_fixture.h','-o',generated)
    library='mixed.dll' if windows else 'libmixed.so'
    flags=['-std=c11','-shared','-fuse-ld=lld','-O1','-g','-Wall','-Wextra','-Werror','-I'+str(fixtures)]
    if windows:flags+=['--target=x86_64-pc-windows-msvc','-fms-runtime-lib=static']
    else:flags+=['-fPIC']
    if sanitized:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all']
    run(clang,*flags,fixtures/'mixed_fixture.c',generated,'-o',native/library)
    manifest=project/'void.project.toml'
    manifest.write_text(manifest.read_text()+f'\n[native.{"windows" if windows else "linux"}]\ndescriptor="mixed.descriptor"\nlibrary="{library}"\nadapter="{library}"\n')
    # new creates a manifest-selected source entry.
    source=next((project/'src').glob('*.void'))
    source.write_bytes((fixtures/'mixed.void').read_bytes())
    run(compiler,'check','--project',project)
    for profile in ('debug','release'):
        assert b'Mixed native resources/records/spans passed' in run(compiler,'run','--project',project,'--allow-native','--profile',profile)
        output=base/profile/('app.exe' if windows else 'app')
        run(compiler,'build','--project',project,'--allow-native','--profile',profile,'-o',output)
        shutil.copytree(native,output.parent/'native',dirs_exist_ok=True)
        moved=base/('relocated '+profile);output.parent.rename(moved)
        assert b'Mixed native resources/records/spans passed' in run(moved/output.name,cwd=base)
    source.write_bytes(b'use "/mixed.void" as ffi\nfunc main() -> i32:\n'
                       b'    var owner = ffi.create(1) as ffi.box\n'
                       b'    ffi.close_box(owner)\n'
                       b'    var pair = ffi.combine(owner, owner, ffi.pair(x = 0, y = 0 as f32))\n'
                       b'    return pair.x\n')
    run(compiler,'check','--project',project)
    rejected=subprocess.run([str(compiler),'run','--project',str(project),'--allow-native'],
                            env=environment,capture_output=True,timeout=180)
    assert rejected.returncode==1 and b'C primitive transport rejected' in rejected.stderr, rejected
    assert b'AddressSanitizer' not in rejected.stderr and b'UndefinedBehaviorSanitizer' not in rejected.stderr, rejected
print('Mixed conventional ABI JIT/AOT and relocation passed')
