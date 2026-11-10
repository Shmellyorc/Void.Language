"""Exercise the installed compiler and relocate ordinary and raylib AOT packages."""
from argparse import ArgumentParser
from hashlib import file_digest
from pathlib import Path
import json, os, shutil, subprocess, sys

parser=ArgumentParser()
for name in ('cmake','build','source','clang','llvm-readobj','work','header','library','license'):
    parser.add_argument('--'+name,type=Path,required=True)
parser.add_argument('--config',required=True)
parser.add_argument('--import-library',type=Path)
parser.add_argument('--strict',action='store_true')
args=parser.parse_args()
windows=os.name=='nt'
suffix='.exe' if windows else ''
assert windows == (args.import_library is not None)
sys.stdout.reconfigure(encoding='utf-8')
env=dict(os.environ)
for key in ('VOID_STDLIB_ROOT','LD_PRELOAD','LD_LIBRARY_PATH'):
    env.pop(key,None)
if args.strict:
    env.update(ASAN_OPTIONS='halt_on_error=1:abort_on_error=1:detect_leaks=1',
               UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
args.work.mkdir()
cwd=args.work/'independent cwd'
cwd.mkdir()

def digest(path):
    with path.open('rb') as stream:
        return file_digest(stream,'sha256').hexdigest()

def run(*argv,status=0,expected=None,environment=None):
    cmd=list(map(str,argv))
    print(json.dumps(dict(cwd=str(cwd),argv=cmd),ensure_ascii=False),flush=True)
    process=subprocess.run(cmd,cwd=cwd,env=env if environment is None else environment,
                           capture_output=True,timeout=300)
    output=(process.stdout+process.stderr).decode('utf-8',errors='replace')
    print(f'exit={process.returncode}\n{output}',flush=True)
    assert process.returncode==status,(status,process.returncode,output)
    assert expected is None or expected in output,output
    assert 'AddressSanitizer' not in output and 'UndefinedBehaviorSanitizer' not in output,output
    return output

initial=args.work/'compiler install'
for component in ('compiler','tooling'):
    run(args.cmake,'--install',args.build,'--prefix',initial,'--config',args.config,'--component',component)
prefix=args.work/'relocated compiler 雪'
initial.rename(prefix)
compiler=prefix/'bin'/('voidc'+suffix)
run(compiler,'--version',expected='23.1.4')
run(sys.executable,args.source/'tools/verify_install.py',prefix)
print('Installed compiler SHA-256',digest(compiler),flush=True)

created=args.work/'created project'
run(compiler,'new',created,'--source','src/launch.void','--project-id','org.voidlang.s7-integrated-new')
assert (created/'src/launch.void').is_file()
run(compiler,'check','--project',created)
for options in (['--no-opt'],[]):
    run(compiler,'run','--project',created,*options)

initialized=args.work/'initialized project'
initialized.mkdir()
hello='use console\n\nfunc main() -> i32:\n    print("SLICE7 installed ordinary application")\n    return 0\n'
entry=initialized/'main.void'
entry.write_bytes(hello.encode())
run(compiler,'init',initialized,'--project-id','org.voidlang.s7-integrated-init')
assert entry.read_bytes()==hello.encode()
run(compiler,'check','--project',initialized)
plain=args.work/'ordinary build'/('ordinary'+suffix)
run(compiler,'build','--project',initialized,'-o',plain)
plain_package=args.work/'relocated ordinary 雪'
plain.parent.rename(plain_package)
plain=plain_package/plain.name

gallery=args.work/'gallery project'
shutil.copytree(args.source/'examples/raylib',gallery,ignore=shutil.ignore_patterns('__pycache__'))
prepare=[sys.executable,gallery/'prepare.py','--voidc',compiler,'--clang',args.clang,
         '--header',args.header,'--library',args.library,'--license',args.license]
if args.import_library:
    prepare+=['--import-library',args.import_library]
if args.strict:
    prepare+=['--sanitize']
run(*prepare)
run(compiler,'check','--project',gallery)
run(compiler,'run','--project',gallery,'--','--trace-only',status=2,expected='Pass --allow-native')
run(compiler,'build','--project',gallery,'-o',args.work/('denied'+suffix),status=2,expected='Pass --allow-native')
assert not (args.work/('denied'+suffix)).exists()
payload=('void_raylib.dll','raylib.dll','RAYLIB-LICENSE.txt') if windows else (
         'libvoid_raylib.so','libraylib.so.600','RAYLIB-LICENSE.txt')
gallery_packages=[]
for mode,options in (('normal',['--no-opt']),('optimized',[])):
    run(compiler,'run','--project',gallery,'--allow-native',*options,'--','--trace-only',
        expected='RAYLIB trace invoked VOID')
    out=args.work/('linked '+mode)/('gallery'+suffix)
    run(compiler,'build','--project',gallery,'--allow-native',*options,'-o',out)
    package=args.work/('relocated gallery '+mode+' 雪')
    out.parent.rename(package)
    (package/'native').mkdir()
    for file in payload:
        shutil.copy2(gallery/'native'/file,package/'native'/file)
    assert digest(package/'native/RAYLIB-LICENSE.txt')==digest(args.license)
    gallery_packages.append(package/out.name)
for path in (prefix,created,initialized,gallery):
    path.rename(path.with_name('archived '+path.name))
    assert not path.exists()

runtime_env=env|{'PATH':str(Path(os.environ['SystemRoot'])/'System32') if windows else '/usr/bin:/bin',
                 'VOID_STDLIB_ROOT':str(args.work/'absent stdlib')}
runtime_env.pop('VOID_CLANG',None)
run(plain,expected='SLICE7 installed ordinary application',environment=runtime_env)
for binary in gallery_packages:
    run(binary,'--trace-only',expected='RAYLIB trace invoked VOID',environment=runtime_env)
if not windows:
    for binary in (plain,*gallery_packages,*(p.parent/'native/libvoid_raylib.so' for p in gallery_packages)):
        loaded=run('/usr/bin/ldd',binary,environment=runtime_env)
        assert 'not found' not in loaded
        resolved=[Path(row.split(' => ',1)[1].rsplit(' (',1)[0]).resolve()
                  for row in loaded.splitlines() if ' => /' in row]
        assert resolved
        assert all(not path.is_relative_to(args.source.resolve()) or
                   path.is_relative_to(binary.parent.resolve()) for path in resolved),resolved
        run('/usr/bin/readelf','-d',binary,environment=runtime_env)
else:
    for binary in (plain,*gallery_packages,*(p.parent/'native/void_raylib.dll' for p in gallery_packages)):
        run(args.llvm_readobj,'--coff-imports',binary,environment=runtime_env)
print(json.dumps(dict(
    raylib_input_hashes={str(p):digest(p) for p in (args.header,args.library,args.license,args.import_library) if p},
    packaged_hashes={str(p):digest(p) for p in (plain,*gallery_packages,
        *(binary.parent/'native'/file for binary in gallery_packages for file in payload))}
),indent=2,ensure_ascii=False))
print('Integrated installed workflow passed')
