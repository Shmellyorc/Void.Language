"""One focused consolidated local selection with exact commands, JUnit and source receipts."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import sys
import time
import xml.etree.ElementTree as ET
import harness as h

profile=sys.argv[1]
build=(h.ROOT/'build'/('ci-official-'+profile)).resolve()
evidence=h.ROOT/'docs/random/evidence'
selection='^(random_.*|lsp_random|core|numeric|frontend|modules|collections_source|binary_execution|binary_ownership|lsp_binary|stage11_globals|stage11_globals_stack|stage11_globals_aot|lsp_stage11_globals|documentation_execution|lsp_documentation|debug_cache|debug_cache_processes|numeric_sprint2|text_collections_sprint3|text_encoding_crash_preservation|io_crash_preservation)$'
env=dict(os.environ)
env['VOID_CRASH_EVIDENCE']=str(evidence/'crashes')
env['PYTHONPATH']=str(h.ROOT/'tests/text_encoding')+os.pathsep+env.get('PYTHONPATH','')
if profile=='sanitizers':
    env['ASAN_OPTIONS']='detect_leaks=1:halt_on_error=1:abort_on_error=0:exitcode=87'
    env['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1:exitcode=86'
command=['ctest','--test-dir',str(build),'-R',selection,'-j','2','--output-on-failure','--output-junit',str(evidence/('acceptance-'+profile+'.xml'))]
started=time.time()
result=subprocess.run(command,cwd=h.ROOT,env=env,capture_output=True,timeout=3600)
(evidence/('acceptance-'+profile+'.log')).write_bytes(result.stdout+result.stderr)
print(result.stdout.decode(errors='replace'),end='')
print(result.stderr.decode(errors='replace'),end='')
xml=evidence/('acceptance-'+profile+'.xml')
tests=[]
if xml.exists():
    for case in ET.parse(xml).getroot().iter('testcase'):
        tests.append({'name':case.attrib['name'],'seconds':float(case.attrib.get('time',0)),'failed':case.find('failure') is not None,'skipped':case.find('skipped') is not None})
files=[h.ROOT/'CMakeLists.txt',h.ROOT/'cmake/NativeBuild.cmake',h.ROOT/'include/void/ir.hpp',h.ROOT/'include/void/runtime_abi.hpp',h.ROOT/'src/frontend/compiler.cpp',h.ROOT/'src/frontend/random.inc',h.ROOT/'src/frontend/capabilities.inc',h.ROOT/'src/ir/module.cpp',h.ROOT/'src/backend/llvm/backend.cpp',h.ROOT/'src/runtime/random.cpp',h.ROOT/'stdlib/random.void',h.ROOT/'tests/text_encoding/crash.py',*Path(__file__).parent.glob('*.*')]
record={'profile':profile,'command':command,'cwd':str(h.ROOT),'status':result.returncode,'seconds':time.time()-started,'tests':tests,'passed':sum(not x['failed'] and not x['skipped'] for x in tests),'failed':sum(x['failed'] for x in tests),'environment':{x:env[x] for x in ('ASAN_OPTIONS','UBSAN_OPTIONS','VOID_CRASH_EVIDENCE','PYTHONPATH') if x in env},'compiler_sha256':hashlib.sha256((build/'voidc').read_bytes()).hexdigest(),'sources':{str(path.relative_to(h.ROOT)):hashlib.sha256(path.read_bytes()).hexdigest() for path in files if path.is_file()}}
(evidence/('acceptance-'+profile+'.json')).write_text(json.dumps(record,indent=2)+'\n')
assert result.returncode==0,record
print(f'{profile}: {record["passed"]}/{len(tests)} selected CTests passed')
