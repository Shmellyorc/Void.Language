"""Relocated real installed sources, normal/O2 JIT and independently linked executables."""
from pathlib import Path
import hashlib
import os
import shutil
import sys
import tempfile
import harness as h

compiler=Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='VOID random relocated ') as directory:
    work=Path(directory)
    prefix=work/'original'
    h.run(['cmake','--install',compiler.parent,'--prefix',prefix,'--component','compiler'],work)
    relocated=work/'relocated install';prefix.rename(relocated)
    installed=relocated/'bin'/compiler.name
    library=relocated/'share/void/stdlib'
    for name in ('random.void','error.void','convert.void'):
        assert (library/name).read_bytes()==(h.ROOT/'stdlib'/name).read_bytes()
    # Ensure no configured environment override hides installed lookup.
    env=dict(os.environ);env.pop('VOID_STDLIB_ROOT',None)
    fixture=work/'main.void';fixture.write_bytes((h.ROOT/'examples/random/gameplay.void').read_bytes())
    expected=b'1\n1\n0.6899902\ncoin\n[key, gem, coin]\n'
    # Get the fixed output from the separately tested algorithm, not a golden
    # runtime invocation. Floats use binary encoding in the main vector suite.
    fixture.write_text('use random\nfunc main():\n    var engine = random.generator(seed = 42)\n    print(engine.rand_u64())\n    print(engine.rand_u8(129))\n    print(engine.rand_i32(-4, 8))\n')
    from reference import Reference
    rng=Reference(42)
    expected=('\n'.join(map(str,[rng.word(),rng.integer(8,False,0,129),rng.integer(32,True,-4,8)]))+'\n').encode()
    for flags in (['--no-opt'],[]):
        h.run([installed,'run',fixture,*flags],work,expected,env=env)
        executable=work/('native'+str(bool(flags))+('.exe' if os.name=='nt' else ''))
        h.run([installed,'build',fixture,'-o',executable,*flags],work,env=env)
        h.run([executable],work,expected,sources=(fixture,),env=env)
        entropy=work/'entropy.void';entropy.write_bytes((h.ROOT/'tests/random/entropy.void').read_bytes())
        h.run([installed,'run',entropy,*flags],work,b'true\n',env=env)
        h.run([installed,'build',entropy,'-o',executable,*flags],work,env=env)
        h.run([executable],work,b'true\n',sources=(entropy,),env=env)
print(f'Random install/real OS entropy: {h.checks} processes passed')
