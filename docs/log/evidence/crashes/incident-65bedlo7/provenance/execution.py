"""Execute actual logging source through JIT/O2 and separately linked native AOT."""
from pathlib import Path
import sys, tempfile, json
import support as h
compiler=Path(sys.argv[1]).resolve()
runner=compiler.with_name('void_io_runner')
small='--small-stack' in sys.argv
checks=0
with tempfile.TemporaryDirectory(prefix='VOID-log-execution-') as d:
    work=Path(d)
    for name,expected in [('smoke', 'INFO "" "hello\\n雪"\n7\n'.encode()),
                          ('semantics', b'WARN "" "independent"\nINFO "" "once"\nERROR "" "new default"\n7\n'),
                          ('async', b'7\n')]:
        source=Path(__file__).with_name(name+'.void').resolve()
        h.run([compiler,'check',source],small=small)
        for optimized in (False,True):
            mode=('o2' if optimized else 'o0')
            cwd=work/(name+mode);cwd.mkdir()
            if name=='async':
                # The explicit root runner compiles both modes; each source owns only its test files.
                if not optimized:
                    h.run([runner,'--async',source],cwd=cwd,small=small)
                    checks+=2
            else:
                h.run([compiler,'run',source,'7','--no-cache',*([] if optimized else ['--no-opt'])],cwd=cwd,expected=expected,small=small)
                checks+=1
            obj=work/(name+mode+'.o');exe=work/(name+mode+'.app')
            if name=='async':
                h.run([runner,'--emit-async',source,obj,'optimized' if optimized else 'plain'],small=small)
            else:
                h.run([compiler,'emit',source,'-o',obj,*([] if optimized else ['--no-opt'])],small=small)
            h.link(compiler,obj,exe,sources=[source])
            h.run([exe,'7'],cwd=cwd,expected=expected,small=small,sources=[source])
            checks+=3
    # Explicit enum field defaults work in pure/nested/managed construction;
    # missing enum initialization remains independently rejected by frontend.py.
print(f'Logging execution: {checks} checks; hard stack={1048576 if small else "default"}')
