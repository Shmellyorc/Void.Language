"""Compile exact raylib declarations and resolve every public native symbol."""
from pathlib import Path
import json
import os
import re
import subprocess
import sys
import tempfile

clang, package, catalog = map(lambda p:Path(p).resolve(),sys.argv[1:4])
sanitized=sys.argv[4:]==['ON']
windows=os.name=='nt'
assert re.search(r'\b23\.1\.4\b',subprocess.check_output([str(clang),'--version'],text=True))
inventory=json.loads((package/'inventory.json').read_text())
names=[entry['name'] for entry in inventory['functions']]
assert len(names)==len(set(names))==600
with tempfile.TemporaryDirectory(prefix='VOID 600 raylib symbols ') as temporary:
    root=Path(temporary);source=root/'symbols.c';exe=root/('symbols.exe' if windows else 'symbols')
    source.write_text('#include "raylib_parity.h"\n#include <stdio.h>\n'
        'typedef void (*Function)(void);\nFunction volatile public_api[]={\n'+
        ',\n'.join('(Function)'+name for name in names)+'\n};\n'
        'int main(void){for(unsigned i=0;i<600;++i)if(!public_api[i])return 1;'
        'puts("600 native declarations compiled, linked and resolved");return 0;}\n',encoding='ascii')
    flags=['-std=c11','-O2','-Wall','-Wextra','-Werror','-fuse-ld=lld',
           '-I'+str(package/'upstream'),'-I'+str(package/'native')]
    if windows:
        flags+=['--target=x86_64-pc-windows-msvc','-DUSE_LIBTYPE_SHARED','-fms-runtime-lib=static']
        library=catalog/'raylib.lib'
    else:
        library=catalog/'libraylib.so.600';flags+=['-Wl,-rpath,'+str(catalog)]
    if sanitized:flags+=['-fsanitize=address,undefined','-fno-sanitize-recover=all']
    subprocess.run([str(clang),*flags,str(source),str(library),'-o',str(exe)],check=True,capture_output=True)
    env=dict(os.environ)
    if windows:env['PATH']=str(catalog)+os.pathsep+env['PATH']
    if sanitized:env.update(ASAN_OPTIONS='halt_on_error=1:abort_on_error=1:detect_leaks=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    result=subprocess.run([str(exe)],env=env,capture_output=True,timeout=30)
    assert result.returncode==0,(result.returncode,result.stdout,result.stderr)
    print(result.stdout.decode().strip())
