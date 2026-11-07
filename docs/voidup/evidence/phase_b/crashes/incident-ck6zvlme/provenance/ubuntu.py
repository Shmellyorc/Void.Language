"""Ubuntu 24.04 userspace acceptance; native host kernel is recorded separately."""
from pathlib import Path
import argparse,json,time
from support import ROOT,EVIDENCE,Runner,Service
parser=argparse.ArgumentParser();parser.add_argument('application',type=Path)
parser.add_argument('--name',default='ubuntu-1');options=parser.parse_args()
runner=Runner(options.name);app=options.application.resolve()
container='voidup-portability-ubuntu';work=f'/tmp/voidup-phase-b-{int(time.time())}';root=work+'/private'
exec_base=['podman','exec',container,'/usr/bin/prlimit','--stack=1048576:1048576']
runner.run(['podman','exec',container,'mkdir','-m','700',work])
runner.run(['podman','exec',container,'cat','/etc/os-release'])
with Service(ROOT/'build/voidup-fixtures') as server:
 base=[*exec_base,app,'--root',root,'--catalog',server.url,'--ca-file',server.ca,'--timeout-ms','120000']
 for release in ('engineering-a','engineering-b'):
  runner.run([*base,'install',release,'--activate'],timeout=900)
  runner.run([*base,'verify',release])
  runner.run([*base,'run',release,'voidc','--','--version'])
 # Materialize the source fixture through exec input; no shell evaluation.
 source=b'use os\nfunc main() -> i32:\n    if os.args() is Ok(var values):\n        for value in values:\n            print(value)\n    return 17\n'
 runner.run(['podman','exec','-i',container,'tee',work+'/main.void'],input=source)
 for release in ('engineering-a','engineering-b'):
  for flags in (['--no-opt'],[]):
   runner.run([*base,'run',release,'voidc','--','run',work+'/main.void',*flags,'--no-cache','--','','雪😀','literal $()'],expected=17)
   binary=work+'/'+release+('-normal' if flags else '-o2')
   runner.run([*base,'run',release,'voidc','--','build',work+'/main.void','-o',binary,*flags])
   runner.run([*exec_base,'env','LD_LIBRARY_PATH='+root+'/toolchains/'+release+'/lib/void',binary,'','雪😀','literal $()'],expected=17)
 runner.run([*base,'rollback']);runner.run([*base,'doctor'])
 (EVIDENCE/(options.name+'-summary.json')).write_text(json.dumps(dict(schema=1,scope='actual Ubuntu 24.04 container userspace on native PikaOS Linux kernel',root=root,container=container,hard_stack_bytes=1048576,actual_signed_a_b=True,normal_o2_jit_and_independent_aot=True),indent=2)+'\n')
print('Ubuntu floor: real signed A/B install, compiler normal/O2 JIT and independent AOT passed')
