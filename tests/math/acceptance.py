"""One focused local Debug/Release/strict selection, with durable commands and JUnit.

Usage: python tests/math/acceptance.py debug|release|sanitizers [--failed-only|--numerical-followup|--tooling-followup]
Re-run only recorded affected failures with --failed-only after investigation.
"""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import sys
import time
import xml.etree.ElementTree as ET
import bootstrap as h

profile=sys.argv[1]
assert profile in ('debug','release','sanitizers'),profile
failed_only='--failed-only' in sys.argv
numerical_followup='--numerical-followup' in sys.argv
tooling_followup='--tooling-followup' in sys.argv
cache_followup='--cache-followup' in sys.argv
assert sum((failed_only,numerical_followup,tooling_followup,cache_followup))<=1,'Choose one follow-up mode'
build=(h.ROOT/'build'/('ci-official-san' if profile=='sanitizers' else 'ci-official-'+profile)).resolve()
evidence=h.ROOT/'docs/math/evidence';evidence.mkdir(parents=True,exist_ok=True)
selection='^(math_.*|lsp_math|core|numeric|frontend|modules|numeric_sprint2|binary_execution|binary_ownership|lsp_binary|debug_cache|debug_cache_processes|gc_schemas|gc_buffers|async_ir|documentation_execution|lsp_documentation|lsp_stage8|lsp_stage10_intelligence|lsp_stage10_display|text_encoding_crash_preservation)$'
label=profile+('-cache-followup' if cache_followup else '-numerical-followup' if numerical_followup else '-tooling-followup' if tooling_followup else '-followup' if failed_only else '')
if cache_followup:selection='^math_host$'
if numerical_followup:selection='^(math_execution|math_stack)$'
if tooling_followup:selection='^(lsp_math|lsp_binary|lsp_documentation|lsp_stage8|lsp_stage10_display|lsp_stage10_intelligence|generics_source)$'
if failed_only:
    prior=json.loads((evidence/('acceptance-'+profile+'.json')).read_text())
    selected=[x['name'] for x in prior['tests'] if x['failed']]
    assert selected,'No recorded failures to rerun'
    selection='^('+'|'.join(re.escape(x) for x in selected)+')$'
env=dict(os.environ);env['VOID_CRASH_EVIDENCE']=str(evidence/'crashes')
env['PYTHONPATH']=str(h.ROOT/'tests/text_encoding')+os.pathsep+env.get('PYTHONPATH','')
if profile=='sanitizers':
    env['ASAN_OPTIONS']='detect_leaks=1:halt_on_error=1:abort_on_error=0:exitcode=87'
    env['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1:exitcode=86'
command=['ctest','--test-dir',str(build),'-R',selection,'-j','2','--output-on-failure','--output-junit',str(evidence/('acceptance-'+label+'.xml'))]
started=time.time();result=subprocess.run(command,cwd=h.ROOT,env=env,capture_output=True,timeout=7200)
(evidence/('acceptance-'+label+'.txt')).write_bytes(result.stdout+result.stderr)
print(result.stdout.decode(errors='replace'),end='');print(result.stderr.decode(errors='replace'),end='')
tests=[];xml=evidence/('acceptance-'+label+'.xml')
if xml.exists():
    for case in ET.parse(xml).getroot().iter('testcase'):
        tests.append({'name':case.attrib['name'],'seconds':float(case.attrib.get('time',0)),'failed':case.find('failure') is not None,'skipped':case.find('skipped') is not None})
files=[h.ROOT/'CMakeLists.txt',h.ROOT/'include/void/math_ops.hpp',h.ROOT/'include/void/ir.hpp',h.ROOT/'src/frontend/compiler.cpp',h.ROOT/'src/frontend/math.inc',h.ROOT/'src/frontend/generics.inc',h.ROOT/'src/frontend/generic_inference.inc',h.ROOT/'src/frontend/generic_tooling.inc',h.ROOT/'src/frontend/capabilities.inc',h.ROOT/'src/ir/module.cpp',h.ROOT/'src/backend/llvm/backend.cpp',h.ROOT/'stdlib/math.void',h.ROOT/'tests/random/harness.py',h.ROOT/'tests/text_encoding/crash.py',*Path(__file__).parent.glob('*.*')]
config=build/'CMakeCache.txt';native=build/'void-native.txt'
record={'profile':profile,'failed_only':failed_only,'numerical_followup':numerical_followup,'tooling_followup':tooling_followup,'command':command,'cwd':str(h.ROOT),'status':result.returncode,'seconds':time.time()-started,'tests':tests,'passed':sum(not x['failed'] and not x['skipped'] for x in tests),'failed':sum(x['failed'] for x in tests),'environment':{x:env[x] for x in ('ASAN_OPTIONS','UBSAN_OPTIONS','VOID_CRASH_EVIDENCE','PYTHONPATH') if x in env},'compiler_sha256':hashlib.sha256((build/'voidc').read_bytes()).hexdigest(),'build_config':{'sha256':hashlib.sha256(config.read_bytes()).hexdigest(),'flags':[x for x in config.read_text().splitlines() if x.startswith(('CMAKE_BUILD_TYPE:','CMAKE_CXX_COMPILER:','CMAKE_CXX_FLAGS','VOID_SANITIZERS:','LLVM_DIR:'))]},'native_manifest':native.read_text() if native.exists() else None,'source_hashes':{str(path.relative_to(h.ROOT)):hashlib.sha256(path.read_bytes()).hexdigest() for path in files if path.is_file()},'limits':['Linux host evidence only; native Windows/macOS checks remain pending authorization.','Host sanitizers do not instrument every generated JIT instruction.']}
(evidence/('acceptance-'+label+'.json')).write_text(json.dumps(record,indent=2)+'\n')
assert result.returncode==0,record
print(f'{profile}: {record["passed"]}/{len(tests)} selected CTests passed')
