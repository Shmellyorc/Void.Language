"""Check shipped examples and run them in isolated application directories."""
from pathlib import Path
import os, sys, tempfile
import support as h

compiler = Path(sys.argv[1]).resolve()
runner = compiler.with_name('void_io_runner')
checks = 0
with tempfile.TemporaryDirectory(prefix='VOID-log-examples-') as directory:
    work = Path(directory)
    for source in sorted((h.ROOT / 'examples/log').glob('*.void')):
        h.run([compiler, 'check', source], small=True)
        checks += 1
        for optimized in (False, True):
            cwd = work / (source.stem + ('-o2' if optimized else '-normal'))
            cwd.mkdir()
            env = dict(os.environ, XDG_STATE_HOME=str(cwd / 'state'))
            if source.stem == 'async':
                obj, exe = cwd / 'example.o', cwd / 'example.app'
                h.run([runner, '--emit-async', source, obj,
                       'optimized' if optimized else 'normal'], small=True)
                h.link(compiler, obj, exe, sources=[source])
                h.run([exe, '7'], cwd=cwd, env=env, small=True,
                      sources=[source], expected=b'7\n')
                checks += 3
            else:
                h.run([compiler, 'run', source, '--no-cache',
                       *([] if optimized else ['--no-opt'])],
                      cwd=cwd, env=env, small=True)
                checks += 1
    inventory = h.run([runner, '--inventory-log', h.ROOT / 'tests/log/smoke.void'], small=True)
    rows = inventory.stdout.decode().splitlines()
    assert all('\tNO\t' not in row for row in rows), 'Undocumented public parameter'
    (h.EVIDENCE / 'public-api.tsv').write_bytes(inventory.stdout)
    print(f'Logging examples: {checks} check/JIT/O2/independent async-AOT commands; '
          f'{len(rows)-1} compiler-owned public declarations inventoried')
