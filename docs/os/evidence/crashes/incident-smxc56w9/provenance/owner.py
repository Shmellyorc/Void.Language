"""Use VOID's established explicit async host and native async object adapter."""
from pathlib import Path
import os
import sys
import tempfile
import support as h
compiler=Path(sys.argv[1]).resolve()
runner=Path(sys.argv[2]).resolve() if len(sys.argv)>2 else compiler.parent/('void_io_runner'+('.exe' if os.name=='nt' else ''))
manifest=(compiler.parent/'void-native.txt').read_text().splitlines()
flags=['--driver-mode=g++',*manifest[5:]]
if manifest[2]:flags+=['--ld-path='+manifest[2]]
if sys.platform=='linux':flags+=['-ldl']
with tempfile.TemporaryDirectory(prefix='VOID OS owner ') as directory:
    work=Path(directory);source=work/'owner.void'
    source.write_bytes((h.ROOT/'tests/os/owner_async.void').read_bytes())
    h.run([compiler,'check',source],work)
    h.run([runner,'--async',source],work,b'Debug source execution passed\nO2 source execution passed\n',sources=(source,))
    for mode in ('normal','optimized'):
        obj=work/('owner-'+mode+'.o');native=work/('owner-'+mode+('.exe' if os.name=='nt' else ''))
        h.run([runner,'--emit-async',source,obj,mode],work,sources=(source,))
        h.run([manifest[1],obj,*manifest[3:5],*flags,'-o',native],work,sources=(source,))
        h.run([native,'7'],work,b'7\n',sources=(source,))
print(f'OS owner async JIT/O2/AOT: {h.checks} process checks passed')
