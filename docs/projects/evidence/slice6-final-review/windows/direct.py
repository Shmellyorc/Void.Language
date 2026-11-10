"""Replay the exact qualified direct IR binaries without CTest output truncation."""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
import shutil
import subprocess
import time

share=Path(r'\\vboxsvr\VM\project-slice6-final-review')
home=Path.home()
deadline=time.monotonic()+1200
while time.monotonic()<deadline:
    try:
        receipt=json.loads((share/'native-results.json').read_text())
        if receipt['status'] in ('PASS','FAIL'):break
    except (OSError,ValueError):pass
    time.sleep(5)
else:raise TimeoutError('Native qualification did not finish')
assert receipt['status']=='PASS' and receipt['restored_original_source']
sdk=home/'VoidCI42377496702-Native/.toolchains/llvm-23.1.4/bin'


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream,'sha256').hexdigest()


def replay(profile):
    name=profile['profile']
    build=home/('VoidS6-Storage-'+name)
    temp=home/('VoidS6-Storage-tmp-'+name)
    binary=build/'void_numeric_enum_ir.exe'
    assert sha(binary)==profile['binaries']['void_numeric_enum_ir']
    env=dict(os.environ,PATH=str(sdk)+os.pathsep+os.environ['PATH'],TEMP=str(temp),TMP=str(temp))
    result=subprocess.run([str(binary)],cwd=build,env=env,capture_output=True,timeout=120)
    (share/(name+'-direct-ir.log')).write_bytes(result.stdout+result.stderr)
    assert result.returncode==0
    assert b'storage audit: 24 negative, 12 positive, 0 mismatches' in result.stdout
    assert b'early completion: rejected at the live-prefix guard in normal and O2 JIT' in result.stdout
    for artifact in ('project-nested-evidence.json','Testing/Temporary/LastTest.log'):
        path=build/artifact
        if path.is_file():
            shutil.copyfile(path,share/(name+'-'+path.name))
    binary_names=('void_fixed_arrays_tests.exe','void_async_tests.exe','void_stage10_fields.exe')
    return {'profile':name,'returncode':result.returncode,'argv':[str(binary)],
            'direct_ir_binary_sha256':sha(binary),'other_test_binaries':{n:sha(build/n) for n in binary_names}}


with ThreadPoolExecutor(max_workers=2) as pool:
    profiles=list(pool.map(replay,receipt['profiles']))
(share/'direct-results.json').write_text(json.dumps({'status':'PASS','profiles':profiles},indent=2)+'\n')
