from pathlib import Path
import os
import sys
import support as h
compiler=Path(sys.argv[1]).resolve()
source=h.ROOT/'tests/os/smoke.void'
env=dict(os.environ,VOID_OS_VALUE='snow 雪\n=a')
for flags in (['--no-opt','--no-cache'],['--no-cache']):
    r=h.run([compiler,'run',source,*flags],h.ROOT,env=env)
    print(r.stdout.decode(),end='')
