"""One local frozen-binary verification; existing source and incident bytes are immutable."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib, json, os, re, shutil, signal, subprocess, time
import xml.etree.ElementTree as ET

ROOT=Path('/home/shmellyorc/Documents/Void.Language')
RUN=Path(__file__).resolve().parent
EVIDENCE=ROOT/'docs/math/evidence'
BUILD=ROOT/'build/ci-official-san'

def sha(path):
    with Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()

def write(name,value):
    path=RUN/name
    with path.open('x') as stream:json.dump(value,stream,indent=2);stream.write('\n')

def context(pid):
    base=Path('/proc')/str(pid)
    fields=dict(line.split(':',1) for line in (base/'status').read_text().splitlines())
    return {'pid':pid,'ppid':int(fields['PPid']), 'name':fields['Name'].strip(),
            'executable':str((base/'exe').resolve()),
            **{key:fields.get(key,'unavailable').strip() for key in ('TracerPid','NSpid','NoNewPrivs','Seccomp','Seccomp_filters')}}

def ancestry(pid):
    result=[]
    while pid>0 and len(result)<20:
        try:item=context(pid)
        except (FileNotFoundError,PermissionError,ProcessLookupError):break
        result.append(item);pid=item['ppid']
    return result

def outside(item):
    return item['TracerPid']=='0' and item['Seccomp']=='0' and item['NoNewPrivs']=='0'

def git(*args):
    env=dict(os.environ);env['GIT_OPTIONAL_LOCKS']='0'
    return subprocess.check_output(['git',*args],cwd=ROOT,env=env,text=True)

prior=json.loads((EVIDENCE/'acceptance-sanitizers.json').read_text())
final=json.loads((EVIDENCE/'FINAL.json').read_text())['profiles']['sanitizers']
selected=[case['name'] for case in prior['tests'] if case['failed']]
assert len(selected)==28
selection='^('+'|'.join(map(re.escape,selected))+')$'
settings={key:prior['environment'][key] for key in ('ASAN_OPTIONS','UBSAN_OPTIONS')}
assert settings=={'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1:abort_on_error=0:exitcode=87','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1:exitcode=86'}
env=dict(os.environ);env.update(settings)
env['VOID_CRASH_EVIDENCE']=str(RUN/'new-incidents')
env['PYTHONPATH']=str(ROOT/'tests/text_encoding')+os.pathsep+env.get('PYTHONPATH','')
assert env.get('LSAN_OPTIONS') is None, 'Unexpected LSAN_OPTIONS; do not silently change settings'
process=context(os.getpid());parents=ancestry(os.getpid())
assert outside(process)
assert not any('codex-linux-sandbox' in item['executable'] or item['name'].startswith('codex-linux-san') for item in parents)

frozen={}
for name,expected in final['binaries'].items():
    path=BUILD/name;assert sha(path)==expected,(name,'frozen binary mismatch')
    frozen[str(path.relative_to(ROOT))]=expected
assert sha(BUILD/'CMakeCache.txt')==final['cmake_cache_sha256']
assert sha(BUILD/'void-native.txt')==final['native_manifest_sha256']
query=['ctest','--test-dir',str(BUILD),'-R',selection,'--show-only=json-v1']
listing=subprocess.check_output(query,cwd=ROOT,env=env)
(RUN/'selection.json').write_bytes(listing)
listed=json.loads(listing);assert {x['name'] for x in listed['tests']}==set(selected)
for case in listed['tests']:
    for argument in case.get('command',[]):
        path=Path(argument)
        if path.is_file() and path.is_relative_to(BUILD):frozen[str(path.relative_to(ROOT))]=sha(path)
for name in ('CMakeCache.txt','CTestTestfile.cmake','void-native.txt','libvoid_runtime.a','libvoid_native_fixture.a'):
    path=BUILD/name;frozen[str(path.relative_to(ROOT))]=sha(path)
tracked=git('ls-files','-z').split('\x00');tracked=[x for x in tracked if x]
tracked_before={name:sha(ROOT/name) for name in tracked if (ROOT/name).is_file()}
archive_before={str(path.relative_to(ROOT)):sha(path) for path in sorted((EVIDENCE/'crashes').rglob('*')) if path.is_file()}
archive_dirs_before={str(path.relative_to(ROOT)) for path in (EVIDENCE/'crashes').iterdir() if path.is_dir()}
old_receipts={}
backups=RUN/'original-receipts';backups.mkdir()
for path in EVIDENCE.iterdir():
    if path.is_file():
        old_receipts[path.name]={'sha256':sha(path),'mode':path.stat().st_mode & 0o777}
        shutil.copy2(path,backups/path.name)
command=['ctest','--test-dir',str(BUILD),'-R',selection,'-j','2','--output-on-failure','--output-junit',str(RUN/'results.xml')]
start=time.monotonic();started=datetime.now(timezone.utc).isoformat()
write('baseline.json',{'started_at_utc':started,'cwd':str(ROOT),'git_head':git('rev-parse','HEAD').strip(),'git_status':git('status','--short'),
      'execution_policy':'Codex danger-full-access; approval never; no sandbox_permissions override used',
      'root_process':process,'ancestry':parents,'outside_sandbox_checks':True,
      'sanitizer_environment':{key:env.get(key) for key in ('ASAN_OPTIONS','UBSAN_OPTIONS','LSAN_OPTIONS','PYTHONPATH','VOID_CRASH_EVIDENCE')},
      'original_selection_receipt':'docs/math/evidence/acceptance-sanitizers.json','selected_tests':selected,'command':command,
      'frozen_files':frozen,'tracked_files':tracked_before,'original_incident_files':archive_before,'original_incident_directories':sorted(archive_dirs_before),'original_receipts':old_receipts})
print('Outside sandbox verified: TracerPid=0 Seccomp=0 NoNewPrivs=0; no codex-linux-sandbox ancestor.',flush=True)
print('Frozen binaries verified; unchanged ASan/UBSan settings; LeakSanitizer enabled; LSAN_OPTIONS unset.',flush=True)
print('Running exactly 28 previously failed tests.',flush=True)
observed=[];seen=set();timed_out=False
with (RUN/'stdout.txt').open('wb') as stdout,(RUN/'stderr.txt').open('wb') as stderr:
    child=subprocess.Popen(command,cwd=ROOT,env=env,stdout=stdout,stderr=stderr,start_new_session=True)
    child_context=context(child.pid)
    while child.poll() is None:
        queue=[child.pid]
        for _ in range(48):
            if not queue:break
            pid=queue.pop(0)
            try:
                item=context(pid)
                if pid==child.pid or item['executable'].startswith(str(BUILD)+'/'):
                    signature=(item['executable'],item['name'])
                    if signature not in seen:seen.add(signature);observed.append(item)
                children=(Path('/proc')/str(pid)/'task'/str(pid)/'children').read_text().split()
                queue.extend(map(int,children))
            except (FileNotFoundError,PermissionError,ProcessLookupError):pass
        if time.monotonic()-start>7200:
            timed_out=True;os.killpg(child.pid,signal.SIGKILL);break
        time.sleep(.5)
    status=child.wait()
write('execution-context.json',{'runner':process,'ancestry':parents,'ctest_process':child_context,'observed_test_processes':observed,
      'outside_sandbox':outside(process) and outside(child_context) and all(outside(item) for item in observed),
      'policy':'danger-full-access; no Codex process sandbox, no tracing, no seccomp filter in observed runner/CTest/test processes'})

tests=[]
if (RUN/'results.xml').exists():
    for case in ET.parse(RUN/'results.xml').getroot().iter('testcase'):
        failed=case.find('failure') is not None or case.find('error') is not None
        tests.append({'name':case.attrib['name'],'seconds':float(case.attrib.get('time',0)),'failed':failed,'skipped':case.find('skipped') is not None})
# Tests can emit their own derived receipts. Retain this run's actual bytes in a
# separate directory and restore prior receipt bytes without touching incidents.
generated=RUN/'generated-evidence';generated.mkdir()
restored=[]
for name,record in old_receipts.items():
    path=EVIDENCE/name
    if not path.exists() or sha(path)!=record['sha256']:
        if path.exists():shutil.copy2(path,generated/name)
        shutil.copyfile(backups/name,path);path.chmod(record['mode']);restored.append(name)
for path in EVIDENCE.iterdir():
    if path.is_file() and path.name not in old_receipts:shutil.copy2(path,generated/path.name)
for name in ('execution-ci-official-san.json','execution-ci-official-san-stack.json','install-ci-official-san.json','lsp-ci-official-san.json'):
    if not (generated/name).exists():shutil.copy2(EVIDENCE/name,generated/name)

archive_after={str(path.relative_to(ROOT)):sha(path) for path in sorted((EVIDENCE/'crashes').rglob('*')) if path.is_file()}
archive_changed=[name for name,expected in archive_before.items() if archive_after.get(name)!=expected]
frozen_changed=[name for name,expected in frozen.items() if not (ROOT/name).is_file() or sha(ROOT/name)!=expected]
tracked_changed=[name for name,expected in tracked_before.items() if not (ROOT/name).is_file() or sha(ROOT/name)!=expected]
new_archives=sorted({str(path.relative_to(ROOT)) for path in (EVIDENCE/'crashes').iterdir() if path.is_dir()}-archive_dirs_before)
passed=sum(not item['failed'] and not item['skipped'] for item in tests)
result={'started_at_utc':started,'completed_at_utc':datetime.now(timezone.utc).isoformat(),'duration_seconds':time.monotonic()-start,'command':command,'cwd':str(ROOT),'exit_status':status,'timed_out':timed_out,
        'passed':passed,'failed':sum(item['failed'] for item in tests),'skipped':sum(item['skipped'] for item in tests),'tests':tests,
        'outside_sandbox':outside(process) and outside(child_context) and all(outside(item) for item in observed),
        'sanitizer_environment':{key:env.get(key) for key in ('ASAN_OPTIONS','UBSAN_OPTIONS','LSAN_OPTIONS')},
        'frozen_files_before':frozen,'frozen_files_after':{name:sha(ROOT/name) for name in frozen},'frozen_files_changed':frozen_changed,
        'original_incident_count':len(archive_dirs_before),'original_incident_files':len(archive_before),'original_incident_files_changed':archive_changed,'new_canonical_incidents':new_archives,
        'original_receipts_restored_if_child_rewrote':restored,'tracked_files_changed':tracked_changed,'git_head_after':git('rev-parse','HEAD').strip(),'git_status_after':git('status','--short'),
        'remaining_failures':[item['name'] for item in tests if item['failed'] or item['skipped']],
        'notes':['No build/configure/source/Git mutation or hosted CI command executed.','Child-generated AOT executables and relocated installs use existing frozen compiler/runtime files.','All prior canonical incident files were compared byte-for-byte by SHA-256; original failure receipts were not replaced.']}
write('verification.json',result)
assert len(tests)==28 and {item['name'] for item in tests}==set(selected)
assert result['outside_sandbox']
assert not archive_changed and not frozen_changed and not tracked_changed
assert status==0 and passed==28 and not result['remaining_failures'],result
print(f'PASS: {passed}/28; no skipped tests; outside sandbox; frozen files and all {len(archive_dirs_before)} original incident archives unchanged.',flush=True)
