"""Consolidated local JSON selection; original attempts are never overwritten."""
from pathlib import Path
import sys, os, json, time, hashlib, re, subprocess
import xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tests/text_encoding'))
from crash import run
profile=sys.argv[1]
build=ROOT/'build'/('ci-official-'+profile)
label=profile+'-'+str(time.time_ns())
evidence=ROOT/'docs/json/evidence/implementation'
xml=evidence/('acceptance-'+label+'.xml')
selection='^(json_.*|core|numeric|frontend|modules|growable_arrays_source|patterns_source|reference_values_source|generics_source|collections_source|lsp_documentation|text_collections_sprint3|numeric_sprint2|text_encoding_crash_preservation)$'
if len(sys.argv)>2:selection=sys.argv[2]
env=dict(os.environ)
env['VOID_CRASH_EVIDENCE']=str(evidence/'incidents')
env['PYTHONPATH']=str(ROOT/'tests/text_encoding')+os.pathsep+env.get('PYTHONPATH','')
if profile=='san':
    env['ASAN_OPTIONS']='detect_leaks=1:halt_on_error=1:abort_on_error=0:exitcode=87'
    env['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1:exitcode=86'
command=['ctest','--test-dir',str(build),'-R',selection,'-j','2','--output-on-failure','--output-junit',str(xml)]
start=time.time()
r=run(command,cwd=ROOT,env=env,capture_output=True,timeout=3600,provenance_inputs=list(Path(__file__).parent.glob('*.*')))
(evidence/('acceptance-'+label+'.log')).write_bytes(r.stdout+r.stderr)
tests=[]
if xml.exists():
    for item in ET.parse(xml).getroot().iter('testcase'):
        tests.append({'name':item.attrib['name'],'seconds':float(item.attrib.get('time',0)),'failed':item.find('failure') is not None,'skipped':item.find('skipped') is not None})
files=[Path('CMakeLists.txt'),*Path('src/frontend').glob('*'),*Path('stdlib').rglob('*.void'),*Path('tests/json').glob('*.*')]
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
record={'profile':profile,'command':command,'seconds':time.time()-start,'status':r.returncode,'passed':sum(not t['failed'] and not t['skipped'] for t in tests),'failed':sum(t['failed'] for t in tests),'tests':tests,'environment':{k:env[k] for k in ['ASAN_OPTIONS','UBSAN_OPTIONS','VOID_CRASH_EVIDENCE','PYTHONPATH'] if k in env},'build_hashes':{name:digest(build/name) for name in ['voidc','voidls','void_json_host']},'source_hashes':{str(p):digest(p) for p in files if p.is_file()},'cmake_configuration':(build/'CMakeCache.txt').read_text()}
(evidence/('acceptance-'+label+'.json')).write_text(json.dumps(record,indent=2)+'\n')
print(r.stdout.decode(errors='replace'),end='')
print(r.stderr.decode(errors='replace'),end='')
print(f'{profile}: {record["passed"]}/{len(tests)} selected CTests passed; receipt {label}')
assert r.returncode==0,record
