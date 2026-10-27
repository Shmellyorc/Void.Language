"""Inherited checked abs/clamp failure, cleanup and original source traces in four modes."""
from pathlib import Path
import os
import sys
import tempfile
import bootstrap as h
compiler=Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='VOID math checked errors ') as directory:
    work=Path(directory)
    bodies=[f'var value = math.abs<{t}>({t}.min_value)' for t in ('i8','i16','i32','i64')]
    bodies += ['var value = math.clamp(1, 2, 0)']
    for t in ('f32','f64'):
        # Invalid bound validation precedes NaN value propagation, including NaN bounds.
        bodies += [f'var nan: {t} = 0 / (0 as {t})\nvar value = math.clamp(nan, 2 as {t}, 1 as {t})',f'var nan: {t} = 0 / (0 as {t})\nvar value = math.clamp(nan, nan, 1 as {t})',f'var nan: {t} = 0 / (0 as {t})\nvar value = math.clamp(nan, 0 as {t}, nan)']
    for serial,body in enumerate(bodies):
        source=work/f'checked{serial}.void';source.write_text('use math\nfunc finish():\n    print("cleanup")\nfunc main():\n    defer finish()\n'+''.join('    '+line+'\n' for line in body.splitlines()))
        for mode,flags in [('O0',['--no-opt']),('O2',[])]:
            result=h.run([compiler,'run',source,*flags],work,b'cleanup\n',status=1)
            assert b'runtime error:' in result.stderr and str(source).encode() in result.stderr and b'Called from' in result.stderr,result.stderr
            exe=work/f'checked{serial}{mode}';exe=exe.with_suffix('.exe') if os.name=='nt' else exe
            h.run([compiler,'build',source,'-o',exe,*flags],work)
            result=h.run([exe],work,b'cleanup\n',status=1,sources=(source,))
            assert b'runtime error:' in result.stderr and b'Called from' in result.stderr,result.stderr
print(f'Math inherited checked failures: {len(bodies)} cases x four modes; {h.checks} processes passed')
