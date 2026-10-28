from pathlib import Path
import os
import sys
import tempfile
import support as h
compiler=Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='VOID OS owner ') as directory:
    work=Path(directory);source=work/'main.void'
    source.write_bytes((h.ROOT/'tests/os/owner_async.void').read_bytes())
    for flags in (['--no-opt'],[]):
        h.run([compiler,'run',source,*flags,'--no-cache'],work,b'true\ntrue\n')
        native=work/('owner'+str(bool(flags))+('.exe' if os.name=='nt' else ''))
        h.run([compiler,'build',source,'-o',native,*flags],work)
        h.run([native],work,b'true\ntrue\n',sources=(source,))
print(f'OS owner async JIT/O2/AOT: {h.checks} process checks passed')
