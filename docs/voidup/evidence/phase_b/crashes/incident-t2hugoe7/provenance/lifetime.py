from pathlib import Path
import argparse,json,tempfile
from support import ROOT,EVIDENCE,Runner,Service
parser=argparse.ArgumentParser();parser.add_argument('runner',type=Path)
parser.add_argument('--name',default='lifetime-1');options=parser.parse_args()
runner=Runner(options.name);executable=options.runner.resolve()
work=Path(tempfile.mkdtemp(prefix='VOIDUP lifetime 雪 # % '))
with Service(ROOT/'build/voidup-fixtures') as server:
 for mode in ('normal','o2'):
  args=['--root',work/mode,'--catalog',server.url,'--ca-file',server.ca,'list','--available']
  runner.run([executable,ROOT/'apps/voidup/main.void',mode,'gc',*args],timeout=900)
  runner.run([executable,ROOT/'apps/voidup/main.void',mode,'faults','--root',work/mode,'doctor'],timeout=900)
  state=json.loads((work/mode/'control.json').read_bytes())
  assert state['serial']==10 and state['current']==''
 (EVIDENCE/(options.name+'-summary.json')).write_text(json.dumps(dict(schema=1,retained_work=str(work),one_runtime=True,one_worker=True,hard_stack_bytes=1048576),indent=2)+'\n')
