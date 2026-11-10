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
    (share/'parallel-results.json').write_text(json.dumps(result,indent=2)+'\n')
for gate in (2,1):
    prior=json.loads((share.parent/f'project-slice7-g{gate}'/'native-results.json').read_text())
    profile=next(p for p in prior['profiles'] if p['profile']=='Release')
    build=home/f'VoidS7-G{gate}-Release'
    assert profile['status']=='PASS' and sha(build/'void_tests.exe')==profile['binaries']['void_tests']
    # Pair the unchanged analysis command with the same neighboring Release tests
    # which were eligible when the original -j2 failure occurred.
    argv=[r'C:\gcc\bin\ctest.exe','--test-dir',str(build),'--output-on-failure','-j','2',
          '-R','^(analysis|reload|numeric)$','--repeat','until-fail:16',
          '--output-junit',str(share/f'parallel-g{gate}.xml')]
    with (share/f'parallel-g{gate}.log').open('wb') as log:
        code=subprocess.run(argv,stdout=log,stderr=subprocess.STDOUT,timeout=450).returncode
    result['runs'].append({'gate':gate,'argv':argv,'exit':code,'void_tests_sha256':profile['binaries']['void_tests']})
    save()
result['status']='complete'
save()
