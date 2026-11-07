"""Source application normal/O2 JIT and independent AOT with real HTTPS trust."""
from pathlib import Path
import argparse,json,os,tempfile
from support import ROOT,EVIDENCE,Runner,Service
parser=argparse.ArgumentParser();parser.add_argument('compiler',type=Path)
parser.add_argument('--name',required=True);options=parser.parse_args()
compiler=options.compiler.resolve();runner=Runner(options.name)
work=Path(tempfile.mkdtemp(prefix='VOIDUP matrix 雪 # % '))
with Service(ROOT/'build/voidup-fixtures') as server:
 for optimized in (False,True):
  flags=[] if optimized else ['--no-opt']
  label='o2' if optimized else 'normal'
  literal=['--root',work/label,'--catalog',server.url,'--ca-file',server.ca,'list','--available']
  source=ROOT/'apps/voidup/main.void'
  runner.run([compiler,'run',source,*flags,'--no-cache','--',*literal])
  binary=work/('voidup-'+label)
  runner.run([compiler,'build',source,'-o',binary,*flags])
  runner.run([binary,*literal])
  runner.run([binary,'--root',work/label,'doctor'])
  runner.run([binary,'--root',work/label,'--root',work/label,'doctor'],expected=2)
  runner.run([binary,'run','default','voidc','not-separator'],expected=2)
 (EVIDENCE/(options.name+'-summary.json')).write_text(json.dumps(dict(schema=1,compiler=str(compiler),work=str(work),normal_o2_jit_and_independently_linked_aot=True,hard_stack_bytes=1048576,requests=server.requests),indent=2)+'\n')
print('Normal/O2 source JIT, independent AOT, real HTTPS trust and CLI rejection passed')
