"""Relocated compiler/tooling source installs, build execution and separately linked emitted AOT."""
from pathlib import Path
import os
import sys
import tempfile
import support as h
compiler=Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='VOID OS installed ') as directory:
    work=Path(directory);prefix=work/'original'
    for component in ('compiler','tooling'):
        h.run(['cmake','--install',compiler.parent,'--prefix',prefix,'--component',component],work)
    relocated=work/'relocated 雪 # % install';prefix.rename(relocated)
    installed=relocated/'bin'/compiler.name
    library=relocated/'share/void/stdlib'
    for name in ('os.void','path.void','io.void','error.void'):
        assert (library/name).read_bytes()==(h.ROOT/'stdlib'/name).read_bytes(),name
    env=dict(os.environ);env.pop('VOID_STDLIB_ROOT',None)
    fixture=work/'main.void'
    fixture.write_text('''use os
use console
func main():
    print(os.platform())
    if os.env("VOID_OS_EXPECT_PID") is Ok(var expected):
        if expected == "__VOID_CAPTURE_PID__":
            print("__VOID_CAPTURE_PID__", os.process_id())
            var released = console.readline()
        else:
            print("{os.process_id()}" == expected)
    if os.executable_path() is Ok(var image):
        if os.env("VOID_OS_EXPECT_IMAGE") is Ok(var expected):
            print(image == expected)
''')
    expected=({'linux':'linux','darwin':'macos','win32':'windows'}[sys.platform]+'\ntrue\ntrue\n').encode()
    exec_host=h.ROOT/'tests/os/exec_host.py'
    lines=(relocated/'bin/void-native.txt').read_text().splitlines()
    clang,linker=lines[1:3]
    resources=[(installed.parent/p).resolve() if not Path(p).is_absolute() else Path(p) for p in lines[3:5]]
    flags=lines[5:]
    if linker:flags+=['--ld-path='+linker]
    if sys.platform=='linux':flags+=['-ldl']
    for args in (['--no-opt'],[]):
        h.run([sys.executable,exec_host,installed,'run',fixture,*args],work,expected,env=env)
        binary=work/('built'+str(bool(args))+('.exe' if os.name=='nt' else ''))
        h.run([installed,'build',fixture,'-o',binary,*args],work,env=env)
        h.run([sys.executable,exec_host,binary],work,expected,env=env,sources=(fixture,))
        obj=work/('emitted'+str(bool(args))+'.o')
        h.run([installed,'emit',fixture,'-o',obj,*args],work,env=env)
        direct=work/('direct'+str(bool(args))+('.exe' if os.name=='nt' else ''))
        h.run([clang,'--driver-mode=g++',obj,*resources,*flags,'-o',direct],work,env=env)
        h.run([sys.executable,exec_host,direct],work,expected,env=env,sources=(fixture,))
print(f'OS compiler/tooling install and independent AOT: {h.checks} process checks passed')
