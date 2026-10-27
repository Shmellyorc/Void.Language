"""Relocate an actual recursive install; launch installed CLI JIT/O2 and linked O0/O2."""
from pathlib import Path
import json
import os
import sys
import tempfile
import bootstrap as h
compiler=Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='VOID math relocated ') as directory:
    work=Path(directory);original=work/'original'
    h.run(['cmake','--install',compiler.parent,'--prefix',original,'--component','compiler'],work)
    relocated=work/'relocated install';original.rename(relocated);installed=relocated/'bin'/compiler.name
    library=relocated/'share/void/stdlib'
    for name in ('math.void','encoding/binary.void','convert.void','error.void'):
        assert (library/name).read_bytes()==(h.ROOT/'stdlib'/name).read_bytes(),name
    env=dict(os.environ);env.pop('VOID_STDLIB_ROOT',None)
    fixture=work/'installed_math.void';fixture.write_text('''use math
func entry(input: i32) -> i32:
    var value: f64 = input as f64
    print(math.floor(value + 0.75))
    print(math.sqrt(value * value))
    print(math.atan2(value, -value) > math.pi_f64 / 2)
    print(math.pow(-value, 3.0))
    print(math.sin(value) > 0.0)
    return 0
''')
    expected=b'2\n2\ntrue\n-8\ntrue\n0\n'
    for mode,flags in [('O0',['--no-opt']),('O2',[])]:
        h.run([installed,'run',fixture,'2',*flags],work,expected,env=env)
        executable=work/('installed-'+mode+('.exe' if os.name=='nt' else ''))
        h.run([installed,'build',fixture,'-o',executable,*flags],work,env=env)
        h.run([executable,'2'],work,expected,sources=(fixture,),env=env)
    (h.ROOT/'docs/math/evidence'/('install-'+compiler.parent.name+'.json')).write_text(json.dumps({'compiler':str(compiler),'processes':h.checks,'modes':['JIT O0','JIT O2','AOT O0','AOT O2'],'relocated':True},indent=2)+'\n')
print(f'Math relocated install: {h.checks} processes passed')
