"""Focused existing CTest selection with immutable per-profile engineering receipts."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import sys
import tempfile
import time
import xml.etree.ElementTree as ET
import support as h
profile=sys.argv[1]
assert profile in ('debug','release','sanitizers')
build=h.ROOT/'build'/profile
evidence=h.ROOT/'docs/os/evidence'
receipt=Path(tempfile.mkdtemp(prefix='acceptance-'+profile+'-',dir=evidence))
selection='^(os_.*|lsp_os|io_kernel|io_errors_source|io_errors_async|gc_schemas|gc_buffers|stage11_globals|lsp_stage11_globals|debug_cache|automatic_cache_processes|documentation_execution|lsp_documentation)$'
env=dict(os.environ,VOID_CRASH_EVIDENCE=str(evidence/'crashes'))
env['PYTHONPATH']=str(h.ROOT/'tests/text_encoding')+os.pathsep+env.get('PYTHONPATH','')
if profile=='sanitizers':
    env['ASAN_OPTIONS']='detect_leaks=1:halt_on_error=1:abort_on_error=0:exitcode=87'
    env['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1:exitcode=86'
command=['ctest','--test-dir',str(build),'-R',selection,'-j','2','--output-on-failure','--output-junit',str(receipt/'tests.xml')]
start=time.perf_counter()
result=subprocess.run(command,cwd=h.ROOT,env=env,capture_output=True,timeout=7200)
(receipt/'stdout').write_bytes(result.stdout);(receipt/'stderr').write_bytes(result.stderr)
tests=[]
if (receipt/'tests.xml').exists():
    for case in ET.parse(receipt/'tests.xml').getroot().iter('testcase'):
        tests.append({'name':case.attrib['name'],'seconds':float(case.attrib.get('time',0)),
            'failed':case.find('failure') is not None,'skipped':case.find('skipped') is not None})
paths=[h.ROOT/'CMakeLists.txt',h.ROOT/'cmake/NativeBuild.cmake']
for directory in ('stdlib','src','include','tests/os','examples/os'):
    paths += [p for p in (h.ROOT/directory).rglob('*') if p.is_file() and '__pycache__' not in p.parts]
record={'profile':profile,'command':command,'status':result.returncode,'seconds':time.perf_counter()-start,
    'tests':tests,'passed':sum(not t['failed'] and not t['skipped'] for t in tests),
    'failed':sum(t['failed'] for t in tests),'skipped':sum(t['skipped'] for t in tests),
    'environment':{k:env[k] for k in ('ASAN_OPTIONS','UBSAN_OPTIONS','PYTHONPATH') if k in env},
    'build_config':(build/'CMakeCache.txt').read_text(),
    'source_hashes':{str(p.relative_to(h.ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},
    'tool_hashes':{name:hashlib.sha256((build/name).read_bytes()).hexdigest() for name in ('voidc','voidls','void_os_host','libvoid_runtime.a','void-native.txt')},
    'hard_stack_bytes':1048576,'stack_mechanism':'existing harness sets both RLIMIT_STACK soft and hard in designated child processes'}
(receipt/'receipt.json').write_text(json.dumps(record,indent=2)+'\n')
print(result.stdout.decode(errors='replace'),end='');print(result.stderr.decode(errors='replace'),end='')
print(f'{profile}: {record["passed"]}/{len(tests)} passed, {record["failed"]} failed, {record["skipped"]} skipped; {receipt}')
assert result.returncode==0,receipt
