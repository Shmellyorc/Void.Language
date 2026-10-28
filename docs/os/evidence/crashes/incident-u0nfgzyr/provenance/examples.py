from pathlib import Path
import os
import sys
import tempfile
import support as h
compiler=Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='VOID OS examples ') as directory:
    work=Path(directory);examples=h.ROOT/'examples/os'
    for source in examples.glob('*.void'):
        h.run([compiler,'check',source],work)
    env=dict(os.environ,HOME=str(work/'home'),XDG_DATA_HOME=str(work/'data'),XDG_CONFIG_HOME=str(work/'config'),XDG_CACHE_HOME=str(work/'cache'),XDG_STATE_HOME=str(work/'state'))
    h.run([compiler,'run',examples/'startup.void','--no-cache'],work,env=env)
    if sys.platform=='linux':
        assert not any((work/name).exists() for name in ('home','data','config','cache','state'))
        h.run([compiler,'run',examples/'save.void','--no-cache'],work,b'Saved bytes: 2\n',env=env)
        assert (work/'data/com.studio.game/score.txt').read_bytes()==b'42'
    (work/'assets').mkdir();(work/'assets/hello.txt').write_text('resource fixture')
    for flags in (['--no-opt'],[]):
        native=work/('resources'+str(bool(flags))+('.exe' if os.name=='nt' else ''))
        h.run([compiler,'build',examples/'resources.void','-o',native,*flags],work)
        h.run([native],work,b'resource fixture\n',sources=(examples/'resources.void',))
print(f'OS examples: {h.checks} process checks passed')
