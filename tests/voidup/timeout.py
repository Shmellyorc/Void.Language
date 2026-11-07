"""Actual bounded partial transfer timeout, exact progress and subsequent lock use."""
from pathlib import Path
import argparse,json,tempfile
from support import ROOT,EVIDENCE,Runner,Service
parser=argparse.ArgumentParser();parser.add_argument('application',type=Path);parser.add_argument('--name',default='timeout-1')
options=parser.parse_args();runner=Runner(options.name);app=options.application.resolve()
work=Path(tempfile.mkdtemp(prefix='VOIDUP deadline 雪 # % '));root=work/'private'
with Service(ROOT/'build/voidup-fixtures') as server:
 server.stall='/packages/engineering-a.zip';server.stall_after=1048576
 base=[app,'--root',root,'--catalog',server.url,'--ca-file',server.ca,'--timeout-ms','500']
 result=runner.run([*base,'install','engineering-a','--activate'],expected=1,timeout=30)
 diagnostic=json.loads(result.stderr)
 assert diagnostic['progress']==1048576,diagnostic
 assert diagnostic['kind'].endswith('.timeout') and diagnostic['phase'],diagnostic
 assert server.started.is_set()
 state=json.loads((root/'control.json').read_bytes())
 assert state['serial']==10 and state['current']=='' and state['installed']==[]
 partial=list((root/'transactions').glob('*/package.zip'));assert len(partial)==1 and partial[0].stat().st_size==1048576
 server.resume.set();runner.run([*base,'doctor'])
 (EVIDENCE/(options.name+'-summary.json')).write_text(json.dumps(dict(schema=1,root=str(root),diagnostic=diagnostic,retained_partial_bytes=1048576,lock_reusable_after_timeout=True),indent=2)+'\n')
print('Actual partial transfer deadline, exact 1 MiB progress, retained floor and lock reuse passed')
