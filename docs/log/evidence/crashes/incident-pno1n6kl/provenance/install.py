"""Installed and relocated source discovery, build route, and independent native linking."""
from pathlib import Path
import os,sys,tempfile
import support as h
compiler=Path(sys.argv[1]).resolve();checks=0
with tempfile.TemporaryDirectory(prefix='VOID-log-installed-') as d:
    work=Path(d);prefix=work/'original'
    for component in ('compiler','tooling'):
        h.run(['cmake','--install',compiler.parent,'--prefix',prefix,'--component',component],cwd=work)
    relocated=work/'relocated 雪 # %';prefix.rename(relocated)
    installed=relocated/'bin'/compiler.name;library=relocated/'share/void/stdlib'
    source=work/'main.void';source.write_text('''use log
use convert
func main():
    convert.unwrap(log.configure(log.config(timestamps = false, console = log.console_sink.stdout)))
    convert.unwrap(log.info("installed"))
    convert.unwrap(log.shutdown())
''')
    for name in ('log.void','io.void','console.void','time.void'):
        assert (library/name).read_bytes()==(h.ROOT/'stdlib'/name).read_bytes(),name
    env=dict(os.environ);env.pop('VOID_STDLIB_ROOT',None)
    for flags in (['--no-opt'],[]):
        expected=b'INFO "" "installed"\n'
        h.run([installed,'check',source],cwd=work,env=env,small=True)
        h.run([installed,'run',source,'--no-cache',*flags],cwd=work,env=env,small=True,expected=expected)
        built=work/'built.app';h.run([installed,'build',source,'-o',built,*flags],cwd=work,env=env,small=True)
        h.run([built],cwd=work,env=env,small=True,expected=expected,sources=[source])
        obj=work/'emitted.o';exe=work/'linked.app'
        h.run([installed,'emit',source,'-o',obj,*flags],cwd=work,env=env,small=True)
        h.link(installed,obj,exe,cwd=work,env=env,sources=[source])
        h.run([exe],cwd=work,env=env,small=True,expected=expected,sources=[source]);checks+=7
    env['VOID_TEST_STDLIB']=str(library)
    server=relocated/'bin'/('voidls.exe' if os.name=='nt' else 'voidls')
    h.run([sys.executable,Path(__file__).with_name('lsp.py'),server],cwd=work,env=env,sources=[source],context={'mode':'installed-lsp'});checks+=1
print(f'Logging installation: {checks} relocated check/JIT/O2/build/independent-AOT/LSP checks')
