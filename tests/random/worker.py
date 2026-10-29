"""Use the existing explicit async host and native object/link infrastructure."""
from pathlib import Path
import os
import sys
import tempfile
import harness as h
compiler=Path(sys.argv[1]).resolve();runner=Path(sys.argv[2]).resolve()
manifest=(compiler.parent/'void-native.txt').read_text().splitlines()
assert manifest[0]=='VOID native 23.1.4 v1'
flags=['--driver-mode=g++',*manifest[5:]]
if manifest[2]:flags+=['--ld-path='+manifest[2]]
if sys.platform.startswith('linux'):flags+=['-ldl']
with tempfile.TemporaryDirectory(prefix='VOID random worker ') as directory:
    work=Path(directory);source=work/'worker.void'
    source.write_bytes(Path(__file__).with_name('worker.void').read_bytes())
    h.run([compiler,'check',source],work)
    h.run([runner,'--async',source],work,b'Debug source execution passed\nO2 source execution passed\n',sources=(source,))
    for mode in ('normal','optimized'):
        obj=work/('worker-'+mode+'.o');exe=work/('worker-'+mode+('.exe' if os.name=='nt' else ''))
        h.run([runner,'--emit-async',source,obj,mode],work,sources=(source,))
        h.run([manifest[1],obj,*manifest[3:5],*flags,'-o',exe],work,sources=(source,))
        h.run([exe,'7'],work,b'7\n',sources=(source,))
print(f'Random worker snapshot execution: {h.checks} processes passed')
