"""Build only the isolated adapters, reusing pinned libraries from the prior study."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import time

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
OUT=ROOT/'.toolchains/native-awslc-adapters'
COMPILER=ROOT/'.toolchains/native-schannel/sdk/clang+llvm-23.1.4-x86_64-pc-windows-msvc/bin/clang-cl.exe'
OUT.mkdir(exist_ok=True)
receipt=OUT/'build.json'
if receipt.exists():shutil.copyfile(receipt,OUT/('earlier-build-'+hashlib.sha256(receipt.read_bytes()).hexdigest()[:12]+'.json'))
report={'schema':1,'baseline':'5bf1c514cb9c6a52d00a1ee359d5c54af762e8ec','commands':[],'archives':{}}
for profile in ('Debug','Release'):
    for label,command in [
        ('configure',['cmake','-S',HERE,'-B',OUT/profile,'-G','Ninja',f'-DCMAKE_CXX_COMPILER={COMPILER}',f'-DCMAKE_BUILD_TYPE={profile}']),
        ('build',['cmake','--build',OUT/profile,'--parallel','4'])]:
        path=OUT/(profile+'-'+label+'.log')
        if path.exists():shutil.copyfile(path,OUT/('earlier-'+path.stem+'-'+hashlib.sha256(path.read_bytes()).hexdigest()[:12]+'.log'))
        start=time.monotonic()
        with path.open('w') as log:process=subprocess.run(list(map(str,command)),stdout=log,stderr=subprocess.STDOUT)
        report['commands'].append({'profile':profile,'step':label,'exit':process.returncode,'seconds':round(time.monotonic()-start,3),
            'argv':[str(x).replace(str(ROOT),'<repository>') for x in command]})
        receipt.write_text(json.dumps(report,indent=2)+'\n')
        if process.returncode:raise RuntimeError(profile+' '+label+' failed; log preserved')
    base=ROOT/'.toolchains/native-awslc'/profile
    paths=[base/('curl/curl/lib/libcurl-d.lib' if profile=='Debug' else 'curl/curl/lib/libcurl.lib'),
           base/'aws-lc/ssl/ssl.lib',base/'aws-lc/crypto/crypto.lib',base/'prefix/lib/nghttp2.lib']
    report['archives'][profile]={str(p.relative_to(base)).replace('\\','/'):{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in paths}
    receipt.write_text(json.dumps(report,indent=2)+'\n')
    print(profile,'adapter-only build passed',flush=True)
