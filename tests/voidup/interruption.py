from pathlib import Path
import argparse,json,subprocess,tempfile,time
from support import ROOT,EVIDENCE,Runner,Service,stack,preserve
parser=argparse.ArgumentParser();parser.add_argument('application',type=Path)
parser.add_argument('--name',default='interruption-1');options=parser.parse_args()
app=options.application.resolve();runner=Runner(options.name)
work=Path(tempfile.mkdtemp(prefix='VOIDUP partial download 雪 # % '));root=work/'private'
with Service(ROOT/'build/voidup-fixtures') as server:
 server.stall='/packages/engineering-a.zip';server.stall_after=1048576
 base=[app,'--root',root,'--catalog',server.url,'--ca-file',server.ca,'--timeout-ms','120000']
 command=[*base,'install','engineering-a','--activate']
 sources={p.resolve():p.read_bytes() for directory in (ROOT/'apps/voidup',ROOT/'stdlib') for p in directory.rglob('*.void')}
 owner=subprocess.Popen(list(map(str,command)),cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.PIPE,preexec_fn=stack)
 assert server.started.wait(60)
 packages=list((root/'transactions').glob('*/package.zip'));assert len(packages)==1
 deadline=time.monotonic()+10
 while packages[0].stat().st_size<1048576 and time.monotonic()<deadline:time.sleep(.01)
 partial=packages[0].stat().st_size;assert partial>=1048576
 runner.run([*base,'doctor'],expected=1)
 owner.kill();out,err=owner.communicate(timeout=30)
 result=subprocess.CompletedProcess(list(map(str,command)),owner.returncode,out,err)
 preserve(command,ROOT,result,(ROOT/'apps/voidup',),(Path(__file__).parent,),failure_kind='intentional_SIGKILL_partial_download',source_snapshots=sources)
 assert owner.returncode==-9
 control=json.loads((root/'control.json').read_bytes());assert control['serial']==10 and control['current']=='' and control['installed']==[]
 server.stall=None;server.resume.set()
 runner.run([*base,'doctor'])
 runner.run([*base,'install','engineering-a','--activate'],timeout=900)
 assert packages[0].stat().st_size==partial
 final=json.loads((root/'control.json').read_bytes());assert final['current']=='engineering-a'
 runner.run([*base,'verify','engineering-a'])
 (EVIDENCE/(options.name+'-summary.json')).write_text(json.dumps(dict(schema=1,root=str(root),actual_partial_bytes=partial,owner_status=-9,abandoned_data_retained=True,retry_actual_real_release=True),indent=2)+'\n')
print('Real 1 MiB partial download, native lock contention, SIGKILL and authenticated retry passed')
