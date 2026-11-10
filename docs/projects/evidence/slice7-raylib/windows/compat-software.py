from pathlib import Path
import hashlib, json, shutil, subprocess, tarfile

share=Path(r'\\vboxsvr\VM\slice7-raylib')
local=Path.home()/'VoidS7-Raylib-Compatibility'
local.mkdir(exist_ok=True)
src=local/'raylib-6.0'
build=local/'build-win32-software-linked'
baseline=Path.home()/'VoidCI42377496702-Native'
sdk=(baseline/'.toolchains/llvm-23.1.4/bin').resolve(strict=True)
cmake=Path(r'C:\gcc\bin\cmake.exe')
ninja=cmake.with_name('ninja.exe')
archive=local/'raylib-6.0-source.tar.gz'
if not archive.is_file(): shutil.copyfile(share/archive.name,archive)
sha=lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(archive)=='2b3ee1e2120c7a0796b33062c7e9a694dd8a8caa56a96319ac8c8ecf54a90d0b'
if not (src/'src/raylib.h').is_file():
    with tarfile.open(archive) as tf:
        tf.extractall(local,filter='data')
shutil.copyfile(share/'compat.c',local/'compat.c')

def run(label, argv, timeout=300):
    with (share/('software-'+label+'.txt')).open('wb') as output:
        args=list(map(str,argv))
        output.write((json.dumps(args)+'\n').encode())
        output.flush()
        completed=subprocess.run(args,cwd=local,stdout=output,stderr=subprocess.STDOUT,timeout=timeout)
        output.write(f'exit={completed.returncode}\n'.encode())
    completed.check_returncode()

# Use the exact platform and renderer macros documented by raylib 6.0's
# upstream Makefile, with the installed Clang MSVC driver and LLD.
build.mkdir(exist_ok=True)
sources=[src/'src'/f'{name}.c' for name in ('rcore','rshapes','rtextures','rtext','rmodels','raudio')]
dll=build/'raylib.dll'
implib=build/'raylib.lib'
run('build',[sdk/'clang.exe','--target=x86_64-pc-windows-msvc','-fuse-ld=lld','-fms-runtime-lib=static',
    '-std=c11','-O2','-shared','-D_CRT_SECURE_NO_WARNINGS','-DBUILD_LIBTYPE_SHARED',
    '-DPLATFORM_DESKTOP_WIN32','-DGRAPHICS_API_OPENGL_SOFTWARE',*sources,
    '-lopengl32','-lgdi32','-lwinmm','-lshcore','-luser32','-lshell32','-Wl,/implib:'+str(implib),'-o',dll])
package=local/'software-native-control'
package.mkdir(exist_ok=True)
shutil.copy2(dll,package/dll.name)
exe=package/'compat.exe'
run('link',[sdk/'clang.exe','--target=x86_64-pc-windows-msvc','-fuse-ld=lld',
    '-std=c11','-DUSE_LIBTYPE_SHARED','-I'+str(src/'src'),local/'compat.c',implib,'-o',exe])
run('imports',[sdk/'llvm-readobj.exe','--coff-imports',dll])
run('visible',[exe],40)
receipt={'status':'PASS','dll':str(dll),'dll_sha256':sha(dll),'import_library':str(implib),
         'import_sha256':sha(implib),'header':str(src/'src/raylib.h'),'header_sha256':sha(src/'src/raylib.h')}
(share/'software-result.json').write_text(json.dumps(receipt,indent=2)+'\n')
