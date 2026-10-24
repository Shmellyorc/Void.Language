"""Aggregate completed focused acceptance only; never certifies native platforms."""
from pathlib import Path
import datetime,hashlib,json,shutil,sys,time,xml.etree.ElementTree as ET
root=Path(__file__).resolve().parents[4];base=root/'docs/stage10/engineering/ci96'
def sha(p):
    with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def read(p):return json.loads(Path(p).read_text())
if '--wait' in sys.argv:
    while True:
        try:pending=read(base/'linux-untraced-commands.json')
        except (FileNotFoundError,json.JSONDecodeError):pending=[]
        if any(phase['returncode'] for phase in pending):raise SystemExit('Strict phase failed; preserve evidence before repairs')
        if pending and pending[-1]['label']=='untraced-san-tests':break
        time.sleep(10)
freeze=read(base/'final-source-freeze.json')
assert all(sha(root/f)==s for f,s in freeze.items()),'Source changed during acceptance'
strict=read(base/'linux-untraced-commands.json')
assert len(strict)==3 and strict[-1]['label']=='untraced-san-tests' and all(p['returncode']==0 for p in strict),'Strict acceptance incomplete'
debug=[p for p in read(base/'linux-commands.json') if p['label'] in ('debug-configure','debug-build','debug-tests')]
assert len(debug)==3 and all(p['returncode']==0 for p in debug)
stack=read(base/'stack-final-commands.json');assert len(stack)==2 and all(p['returncode']==0 and p['stack_soft']==1048576 and p['stack_hard']==1048576 for p in stack)
configurations={}
expected=None
for name,tag,phases in [('debug','debug',debug),('san','untraced-san',strict)]:
    junit=ET.parse(base/(tag+'-junit.xml')).getroot();cases=junit.findall('.//testcase')
    names={c.attrib['name'] for c in cases}
    assert len(cases)==18 and all(c.find('failure') is None and c.find('error') is None and c.find('skipped') is None for c in cases)
    if expected is None:expected=names
    else:assert names==expected
    build=root/'build'/('step42-ci96-'+name)
    commands=read(build/'compile_commands.json')
    console_compile=next(c['command'] for c in commands if c['file'].endswith('/src/runtime/console.cpp'))
    assert '_LIBCPP_ENABLE_EXPERIMENTAL' not in console_compile,'Apple-only opt-in leaked into Linux'
    if name=='san':assert '-fsanitize=address,undefined' in console_compile and '-fno-omit-frame-pointer' in console_compile and '-O2' in console_compile
    binaries=['voidc','voidls','void_tests','void_operator_reload','void_temporal','void_temporal_ir','void_console_source','libvoid_core.a','libvoid_runtime.a','libvoid_native_fixture.a','CMakeFiles/void_core.dir/src/frontend/compiler.cpp.o']
    identities={f:sha(build/f) for f in binaries}
    raw=build/'step30-output-results.json';payload=read(raw);shutil.copyfile(raw,base/(name+'-console-output-results.json'))
    configurations[name]={'build_directory':str(build.relative_to(root)),'configuration':'Debug' if name=='debug' else 'RelWithDebInfo','sanitizers':'OFF' if name=='debug' else 'ASan/UBSan/leaks ON','compiler':'Pinned LLVM/Clang19.1.7','console_compile_command':console_compile,'CMakeCache_sha256':sha(build/'CMakeCache.txt'),'compile_commands_sha256':sha(build/'compile_commands.json'),'generator':'Unix Makefiles','phases':phases,'passed':18,'executed':18,'failed':0,'skipped':0,'case_seconds':{c.attrib['name']:float(c.attrib['time']) for c in cases},'binary_archive_object_sha256':identities,'junit':tag+'-junit.xml','console_raw':name+'-console-output-results.json','console_driver_checks':payload['checks'],'console_output_records':len(payload['outputs']),'combined_cache_cases':[r['case'] for r in payload['combined_cache']],'scope':'Focused Linux only, including normal/O2 JIT, independently linked AOT and production native applications'}
old=read(root/'docs/stage10/engineering/step41-results.json')
for f,s in old['artifact_hashes'].items():assert sha(root/'docs/stage10/engineering'/f)==s,f
previous=read(root/'docs/stage10/engineering/step41-after-stack-artifact-checkpoint.json');previous_count=0
for name,files in previous['acceptance_artifact_sha256'].items():
    for f,s in files.items():assert sha(root/old['configurations'][name]['build_directory']/f)==s,f;previous_count+=1
assert sha(root/'README.md')==old['root_README_sha256']
assert sha(root/'src/frontend/compiler.cpp')=='9a54df7f1778446fc3d593829978b4180df2ffd4a55f92ebaae156fc37f81d32'
r=read(root/'docs/stage10/engineering/step42-results.json')
r['updated_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat()
r['status']='CI96 repairs focused-tested on Linux; repaired Windows/macOS source UNVERIFIED; Step42 INCOMPLETE'
r['local_verification']={'status':'PASS: Debug18/18 and strict18/18, plus both original genuine1MiB probes','configurations':configurations,'stack_probes':stack,'probes':'ci96/probes-results.json','source_freeze':'ci96/final-source-freeze.json','source_freeze_inputs':len(freeze),'source_freeze_sha256':sha(base/'final-source-freeze.json'),'original_recorder_failure':{'log':'ci96/recorder-failure.log','cause':'Path not serialized to str after first frontend probe; recorder repaired, both1MiB probes rerun with complete exit records; no production defect','Debug_tests_already_passed':True,'traced_strict_build_or_test_started':False},'strict_profile':'Established untraced exec profile from the start of this strict build/test; no disabled/suppressed checks','ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1:exitcode=87','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1:exitcode=86','preserved_Step41_artifacts':len(old['artifact_hashes']),'preserved_Step41_binary_archive_object_identities':previous_count,'native_results':'UNVERIFIED','full_597_suite_matrix_repeated':False}
r['remaining_native_gates'][:0]=['Actual macOS SDK standard/opt-in stop-token compile/link/run and native CMake detection','Native Windows chrono aliases and exact old/new component probe','Both native configurations: rerun four CI96 failures, then complete unfiltered suites and remaining host/editor/packaging gates']
r['next_concrete_action']='Golden Wrench reviews these eight source/test/build changes and local evidence. Boss controls commits and native execution: run actual SDK/chrono probes, four-suite Debug/Release repair checks, then remaining Step42 acceptance. No further local production repair is warranted without a demonstrated defect.'
p=root/'docs/stage10/IMPLEMENTATION.md';s=p.read_text();s=s.replace('repairs implemented; Linux focused verification is running.','repairs implemented; Linux Debug18/18 and strict18/18 plus both original1MiB\nprobes pass.').replace('## Step 42 CI96 native repairs — 23 October 2026 (verification in progress)','## Step 42 CI96 native repairs — 23 October 2026 (focused Linux verified)')
s=s.replace('Focused Linux verification is running sequentially; exact\nphase commands/statuses are in engineering/ci96/linux-commands.json.','Focused Linux Debug18/18 and strict18/18 pass on matching sources; exact phase\ncommands/statuses are in engineering/ci96/linux-commands.json and\nlinux-untraced-commands.json. Both unchanged genuine1MiB probes pass. The evidence\nrecorder Path serialization error after the first frontend probe is preserved;\nits repair reran both probes with complete exit records. No strict build/test\nstarted under the traced profile.')
s=s.replace('Next: finish focused Debug/strict and original\n1MiB checks, record final hashes/results, then return for Wrench review and Boss-\ncontrolled native probes/four-suite reruns and remaining native acceptance.','Final local evidence: Debug18/18, strict18/18;1311 temporal core,248 compatibility/\ncache/reload and144 source-formatting LSP checks per configuration, plus both1MiB\nprobes. All767 frozen inputs,256 historical Step41 artifacts and24 accepted Linux\nbinary/archive/object identities match; original compiler.cpp and root README are\nunchanged. Next: Wrench review, then Boss-controlled actual native SDK/chrono probes,\nfour-suite reruns and remaining native acceptance.')
s=s.replace('| 42 | Native inventory, platform gaps and exact Debug/Release commands prepared; native Windows/macOS execution UNVERIFIED. Boss authorization/hosts required; engineering/step42-results.json. |','| 42 | CI96 native Windows190/194 each and macOS build failures preserved; eight source/test/build changes focused-tested: Linux Debug18/18, strict18/18 and both1MiB probes. Repaired native source UNVERIFIED; Wrench review and Boss-controlled native reruns required. |')
s=s.replace('### CI96 verification recovery checkpoint','### Historical CI96 verification recovery checkpoint (superseded below)')
s+='\n\n### CI96 final bounded repair handoff\n\nFocused Linux Debug18/18 and strict18/18 are complete, zero final failures/skips.\nBoth original1MiB probes pass. The active acceptance runner has finished; no new\nbuild/test should be started to repeat these results. Final source/binary/evidence\nhashes and native limitations are in engineering/step42-results.json. All767\nfrozen inputs,256 Step41 artifacts and24 original binary/archive/object hashes\nmatch their appropriate current/historical manifests. Repaired Windows/macOS\nsource remains UNVERIFIED. Next: Golden Wrench review, then Boss-controlled native\nSDK/chrono probes, four-suite reruns and remaining Step42 gates. Step42 and Stage10\nINCOMPLETE; Steps43–44 remain separate.\n'
p.write_text(s)
recovery=read(base/'recovery.json');recovery.update(status='Focused Linux acceptance complete; native repaired source UNVERIFIED',active_session_id=None,next='Wrench review, then Boss-controlled native probes and reruns; Step42 INCOMPLETE')
(base/'recovery.json').write_text(json.dumps(recovery,indent=2)+'\n')
handoff=base/'REPAIR_HANDOFF.md'
handoff.write_text(handoff.read_text()+'\n\n## Final focused Linux results\n\nDebug18/18 and strict ASan/UBSan/leaks18/18 pass, zero final failures/skips.\nBoth original genuine1MiB probes pass. Each configuration retains1311 temporal\ncore,248 operator/cache/reload and144 source-formatting LSP checks; full console\nnormal/O2 JIT, linked AOT and production native comparisons pass. The evidence\nrecorder PosixPath error after the original Debug pass is preserved and repaired;\nboth standalone probes were rerun with complete exit records. No traced strict\nbuild/test was started. Step41 originals remain unchanged; final hashes and exact\ncommands are in ../step42-results.json. Repaired native platforms UNVERIFIED.\n')
p=root/'docs/WORK_LOG.md';p.write_text(p.read_text()+'''\n\n## 23 October 2026 — Step42 CI96 native repairs, focused Linux acceptance\n\nPreserved CI96 original Windows190/194 each and both macOS stop-token build\nfailures; seven pre-repair files match committed ef6017613ecf92de31380476ed5d9c3fc8cf3dbb.\nFixed portable civil-time intermediate narrowing (sys_days promoted before time\naddition), added actual SDK stop-token compile/link gates and verified pinned\nlibc++19 opt-in, preserved Windows unavailable cache behavior in tests, exact\nlegacy CRT result trailers and raw source print, and explicit UTF8 fixtures.\nDebug18/18 and strict ASan/UBSan/leaks18/18 pass, plus original fullfrontend and\nsemantic-depth soft/hard1MiB probes. Local cp1252 model passes144 repaired LSP\nchecks; pinned libc++19 callbacks compile/link/run; native Windows/macOS repaired\nsource remains UNVERIFIED. Step41's256 artifacts and24 binary/archive/object\nidentities are unchanged, as are compiler.cpp, source limits and root README.\nThe evidence recorder Path serialization failure was repaired and both probes\nrerun; the small traced LSan failure is preserved, strict build/test ran from the\nstart in the established untraced profile with all settings unchanged. Source\ndiff, exact commands/hashes and pending native probes: stage10/engineering/ci96/.\nNo Git mutation, hosted rerun, policy change, publication or new dependency.\nStep42 and Stage10 INCOMPLETE; Wrench review and Boss-controlled native execution\nnext, Steps43–44 separate. Existing unexplained/deferred observations preserved.\n''')
p=root/'docs/stage10/engineering/step42-native-commands.md';s=p.read_text();s=s.replace('**PREPARED, NOT EXECUTED.', '**Historical preparation, before CI96. Current repairs and replacement source\nreceipt: [CI96 handoff](ci96/REPAIR_HANDOFF.md). The original766-input receipt below\ncorrectly rejects the subsequently authorized repairs.**\n\n**PREPARED, NOT EXECUTED.',1);p.write_text(s)
m=read(root/'docs/stage10/engineering/step41-source-hashes.json')
r['baseline']['current_historical_source_manifest_differences']=[f for f,s in m.items() if sha(root/f)!=s]
r['baseline']['scope']='Step41 evidence/freeze are historical and unmodified. Current authorized repair/documentation deltas are explicitly listed; see ci96/source-changes.json.'
r['current_documentation_hashes']={f:sha(root/f) for f in ['docs/stage10/IMPLEMENTATION.md','docs/stage10/TEST_PLAN.md','docs/stage10/CONSOLE_IO.md','docs/WORK_LOG.md','docs/stage10/engineering/step42-native-commands.md']}
r['deliverable_hashes']={str(p.relative_to(root/'docs/stage10/engineering')):sha(p) for p in sorted(base.rglob('*')) if p.is_file()}
(root/'docs/stage10/engineering/step42-results.json').write_text(json.dumps(r,indent=2)+'\n')
print('Finalized focused Linux36/36 + original1MiB gates; repaired native platforms UNVERIFIED.')
