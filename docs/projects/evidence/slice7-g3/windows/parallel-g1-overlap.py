from pathlib import Path
from hashlib import file_digest
import json
import os
import shutil
import subprocess
import sys
import time

share=Path(r'\\vboxsvr\VM\project-slice7-g3')
home=Path.home()
result={'os_build':sys.getwindowsversion().build,'free_bytes_before':shutil.disk_usage(home).free,'runs':[]}
assert result['os_build']==19045
def sha(path):
    with path.open('rb') as f:return file_digest(f,'sha256').hexdigest()
def save():
    (share/'parallel-g1-overlap-results.json').write_text(json.dumps(result,indent=2)+'\n')
from concurrent.futures import ThreadPoolExecutor
prior=json.loads((share.parent/'project-slice7-g1'/'native-results.json').read_text())
def run(profile):
    kind=profile['profile']
    build=home/f'VoidS7-G1-{kind}'
    assert profile['status']=='PASS' and sha(build/'void_tests.exe')==profile['binaries']['void_tests']
    argv=[r'C:\gcc\bin\ctest.exe','--test-dir',str(build),'--output-on-failure','-j','2',
          '-R','^(analysis|reload|numeric)$','--repeat','until-fail:24',
          '--output-junit',str(share/f'parallel-g1-overlap-{kind}.xml')]
    with (share/f'parallel-g1-overlap-{kind}.log').open('wb') as log:
        code=subprocess.run(argv,stdout=log,stderr=subprocess.STDOUT,timeout=600).returncode
    return {'profile':kind,'argv':argv,'exit':code,'void_tests_sha256':profile['binaries']['void_tests']}
with ThreadPoolExecutor(max_workers=2) as pool:
    result['runs']=list(pool.map(run,prior['profiles']))
result['status']='complete'
save()
