"""Plain errors follow checked Runtime failure and execute source defers."""
from pathlib import Path
import sys
import tempfile
import harness as h
compiler=Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='VOID random errors ') as directory:
    work=Path(directory)
    for serial,body in enumerate(('var value = random.rand_i32(0)','var value = random.rand_range(3, 1)','var empty: [i32] = []\nvar value = random.choose(empty)','var engine = random.generator(seed = 42)\nvar value = engine.rand_f64(1.0, 1.0)')):
        source=work/f'error{serial}.void'
        source.write_text('use random\nfunc finish():\n    print("cleanup")\nfunc main():\n    defer finish()\n'+''.join('    '+x+'\n' for x in body.splitlines()))
        for flags in (['--no-opt'],[]):
            result=h.run([compiler,'run',source,*flags],work,b'cleanup\n',status=1)
            assert b'convert.unwrap received Err' in result.stderr,result.stderr
            assert str(source).encode() in result.stderr,result.stderr
            exe=work/('error'+str(serial)+str(bool(flags))+('.exe' if sys.platform=='win32' else ''))
            h.run([compiler,'build',source,'-o',exe,*flags],work)
            result=h.run([exe],work,b'cleanup\n',status=1,sources=(source,))
            assert b'convert.unwrap received Err' in result.stderr,result.stderr
print(f'Random checked errors/defers: {h.checks} processes passed')
