from pathlib import Path
import json,hashlib,subprocess,shutil,xml.etree.ElementTree as ET
r=Path.cwd();e=r/'docs/projects/evidence';raw=e/'slice2-final-review';w=Path('/home/shmellyorc/VM/project-slice2-final-review')
checkpoint=json.loads((r/'build/slice2-final-review-start/checkpoint.json').read_text());old=json.loads((e/'slice2-local.json').read_text());patches=json.loads((r/'build/slice2-final-review-patches.json').read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def ref(p):p=Path(p);return {'path':str(p.relative_to(r)),'sha256':sha(p),'bytes':p.stat().st_size}
regex='^(project_manifest.*|project_selection|project_discovery_cli|project_nested_cli|cli_contract|cli_install_resources|frontend|modules|numeric|analysis|tooling|sessions|source_cli|modules_cli|analysis_cli|numeric_cli|local_libraries|application_workflow_stage9|debug_cache_processes|automatic_cache_processes|os_frontend|lsp_os|os_errors|lsp_stage7|stage11_globals|stage11_globals_stack|stage11_globals_aot|lsp_stage11_globals|generics_source|generics_hint_execution|generics_hint_lsp|generics_cli)$'
profiles=[]
for name in ['release','debug','strict']:
 tree=ET.parse(r/f'build/slice2-final-review-{name}.xml').getroot();assert tree.get('tests')=='36' and tree.get('failures')=='0'
 cases=[]
 for c in tree.findall('testcase'):
  assert c.find('failure') is None and c.find('error') is None and c.get('status')=='run',c.attrib
  cases.append({'name':c.get('name'),'seconds':float(c.get('time')),'stdout':c.findtext('system-out',''),'result':'PASS'})
 assert len(cases)==36
 assert next(c['stdout'] for c in cases if c['name']=='project_selection')=='project_selection: 476 checks passed\n'
 b=r/('build/llvm23-'+name)
 profiles.append({'profile':name,'build_type':'RelWithDebInfo' if name=='strict' else name.title(),'sanitizers':name=='strict','passed':36,'failed':0,'cases':cases,'build_argv':['cmake','--build',str(b),'--target','void_project_selection','void_project_manifest','void_project_header','void_tests','voidc','voidls','void_globals','void_generics_tests','-j2'],'test_argv':['ctest','--test-dir',str(b),'-R',regex,'--output-on-failure','--output-junit','../slice2-final-review-'+name+'.xml'],'binaries_sha256':{n:sha(b/n) for n in ['voidc','voidls','void_tests','void_project_selection','void_project_manifest','void_project_header','void_globals','void_generics_tests']}})
native=json.loads((w/'native-results.json').read_text());assert native['status']=='FAIL' and native['commands'][-1]['label']=='Debug-nested' and native['commands'][-1]['returncode']==2,native
assert all(c['returncode']==0 for c in native['commands'][:-1])
supp=json.loads((w/'nested-corrective-native-results.json').read_text());assert supp['status']=='PASS',supp
for n,h in native['source_inputs_sha256'].items():assert sha(r/n)==h,n
for profile in ['Release','Debug']:
 assert '353 checks passed' in (w/(profile+'-selection.log')).read_text()
 assert '88 process checks passed' in (w/(profile+'-project-cli.log')).read_text()
 assert '17 checks passed' in (w/(profile+'-legacy.log')).read_text()
 assert '11 process checks passed' in (w/(profile+'-nested-corrective.log')).read_text()
assert subprocess.check_output(['git','rev-parse','HEAD']).decode().strip()==checkpoint['head']
assert sha(r/'.git/index')==checkpoint['index_sha256']
assert subprocess.check_output(['git','diff','--cached','--name-only'])==b''
amended={n:{'starting_sha256':h,'final_sha256':sha(r/n)} for n,h in checkpoint['files_sha256'].items() if sha(r/n)!=h}
assert set(amended)=={'src/frontend/compiler.cpp','src/frontend/project_selection.inc','tests/project/selection.cpp'}
preserved={n:h for n,h in checkpoint['files_sha256'].items() if n not in amended};assert len(preserved)==160
raw.mkdir(exist_ok=True)
for p in sorted((r/'build').glob('slice2-final-review-*')):
 if p.is_file() and p.suffix in ['.log','.xml']:shutil.copyfile(p,raw/(p.name+('.txt' if p.suffix=='.log' else '')))
for n in ['original-oracle.cpp','regressions-before-repair.cpp','original-focused-source.cpp','original-focused.json','patches.json']:
 p=r/('build/slice2-final-review-'+n);shutil.copyfile(p,raw/(n+('.txt' if p.suffix=='.cpp' else '')))
shutil.copyfile(r/'build/slice2-final-review-start/checkpoint.json',raw/'starting-checkpoint.json')
for p in sorted(w.iterdir()):
 if p.is_file() and p.suffix in ['.log','.json','.py','.cmd','.rsp','.txt']:
  shutil.copyfile(p,raw/('windows-'+p.name+('.txt' if p.suffix=='.log' else '')))
for n in ['patches.py','artifacts.py','audit.py']:shutil.copyfile(r/('build/slice2-final-review-'+n),raw/n)
report=r/'docs/projects/SLICE2_FINAL_CLOSEOUT.md';receipt=e/'slice2-final-review.json';inventory=e/'slice2-final-review-inventory.txt'
report.write_text('''# Slice 2 final correctness closeout

8 November 2026. HEAD remains `2f667ea338127b8f91c3136804377ef3fa5218d6`.
Local qualification is complete; **Golden Wrench final S2 review remains pending.
Stop before S3. Hosted CI remains on strict hold.**

The [original S2 report](SLICE2.md), [S1 closeout](SLICE1_CLOSEOUT.md), all original
receipts and raw evidence are retained byte-for-byte. This review changes exactly
three source files; 160 of the 163 starting inventory files remain byte-for-byte
unchanged. New report/evidence files are separately inventoried.

## Tombstone proof

The existing `SourceGraph::resolve` guard in
[compiler.cpp](../../src/frontend/compiler.cpp#L5206) checks
`overlay != overrides.end() && !overlay->second` and throws the owned `VOID3002`
"Source is explicitly unavailable" diagnostic. It runs before physical traversal,
reader invocation and either unit-cache lookup. No source-reader repair was needed;
a comment records this invariant.

- `parse` resolves before its sole `parse_resolved` call. `load` also resolves
  before its unit-cache lookup, depth guard and `parse_resolved` call. These are
  the only calls to `parse_resolved`; its optional dereference and `graph.reader`
  callback therefore require a present overlay value.
- In [ProjectCapture](../../src/frontend/project_selection.inc), the reader calls
  `capture`; D inventory skips tombstones, then resolves before direct capture.
  `recapture_source` resolves before capture. A capture cache hit cannot introduce
  a deleted source because the same frozen source map governs the entire call.
- The selected TOML optional is separately guarded by `boundary("")`: a root
  manifest tombstone fails admission before the constructor dereferences it.

Regressions cover explicit overrides and manifest-authored entries, automatic
missing-main behavior, dirty imported modules, excluded-but-imported E sources,
physical files hidden by tombstones, cyclic imports, bare-file legacy roots,
later overlay revisions and owned diagnostics after disk/overlay removal. Valid
physical bytes never substitute for a tombstone. These synchronous APIs consume
immutable overlay revisions; concurrently mutating a C++ map is not a supported
snapshot operation. Disk changes during original capture/recapture are tested
separately below.

## Recapture correction

The old blanket `catch (const std::exception&)` converted validation and permission
failures to `VOID5011`. The retained original reproduction expected `VOID5009` for
a same-name source replaced by a symlink but received `VOID5011`.

The fresh capture now observes the original capture through a call-local const
pointer. It compares successfully observed root identity, directory membership,
boundaries, raw manifest bytes and local source bytes before downstream validation
can reinterpret a changed snapshot. Explicit final E-byte/map comparisons remain.
Observed mismatches produce owned `VOID5011`; failed admission/read operations keep
their original diagnostic. The generic exception conversion and mismatch-as-
`invalid_argument` exceptions are removed. No parser, semantic, VFS authority,
source limit, dependency pin, LLVM path or application behavior was replaced.

Eight Linux notification-driven regressions mutate files after the entry and an
E-only helper are loaded, while an excluded E source is being read:

| Observation/failure | Required and observed result |
|---|---|
| Changed D bytes, changed E-only bytes | `VOID5011` |
| Manifest replaced by malformed `[=roject]` | `VOID5011`, before schema parsing |
| Deleted source / changed membership | `VOID5011` |
| Same-name symlink replacement | `VOID5009` admission error |
| Source or manifest becomes unreadable | `VOID3002` read error |
| Source directory becomes inaccessible | `VOID5009` enumeration error |

The pre-existing concurrent writer and nested-boundary regressions remain enabled.
No failed or incomplete verification publishes a successful project. This is
bounded recapture/detection, not an atomic filesystem snapshot; a validation
failure without a completed differing observation is not proof of a byte change.

## Executed final qualification

| Configuration | Selected local qualification |
|---|---|
| Linux Release | 36/36 CTests; 476 selection assertions |
| Linux Debug | 36/36 CTests; 476 selection assertions |
| Linux fatal ASan/UBSan, RelWithDebInfo | 36/36 CTests; 476 selection assertions |
| Windows 10 x64 build 19045 Release | 353 selection assertions; 88 project, 11 nested-project and 17 legacy checks |
| Windows 10 x64 build 19045 Debug | 353 selection assertions; 88 project, 11 nested-project and 17 legacy checks |

Linux totals 108/108 selected CTest outcomes, not the entire repository suite.
Each Linux profile includes the 88 project CLI processes, 11 nested-project
processes and 17 manifest/legacy checks. Actual normal/O2 JIT and independent AOT,
imports, D/E initializer isolation, argument forwarding, cache invalidation,
standalone execution, library checks, negative admission, ownership, depth/bounds,
frontend/tooling, numeric/generic and globals regressions pass. Exact commands,
per-test output, binaries/input hashes and raw logs are in the receipt below.

Strict configuration retains `VOID_SANITIZERS=ON`, address+undefined instrumentation,
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:exitcode=87` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1:exitcode=86`.

Windows builds recompile the changed frontend, manifest, main and selection
translation units, generate fresh cache identities, and link inventoried unchanged
backend/Runtime/provider objects. Native HTTP/host dependency verification and
manifest/resource admission remain enabled. Both profiles use the final 126-file
source packet plus the hash-checked shared nested-project test script. This is
incremental native qualification, not a full clean CMake build. The original nested
invocation failed because that script was omitted from the packet; a corrective supplement executes it against both unchanged native binaries.

New failure history is retained: (1) the first legacy test used the project-root
overlay identity for a bare-file adapter; corrected to its entry-parent identity;
(2) the first mutation fixture deleted the entry before E load and correctly got
`VOID3002`; the notifier was moved to an excluded E source to target recapture;
(3) the corrected original-source reproduction exposed the blanket-catch defect;
(4) the native harness omitted nested.py from its packet; its original exit 2 and
FAIL receipt remain intact, followed by passing shared-script reruns on both native
binaries. Final corrected executions above pass. No tests were disabled or weakened.
Unchanged vendored TOML literal-operator deprecation warnings remain visible in
native compile logs.

Remaining limits: Windows symlink/reparse fixtures lack guest privilege; Linux
exercises those negative cases. The new notification/permission recapture tests
are Linux-only. Windows persistent native-cache warm hits remain unavailable.
Windows fatal sanitizers, Windows Server 2022, macOS and a full clean Windows build
were not executed. Existing project editor-protocol/live-reload limitations in
the original S2 report remain; S3, application metadata, FFI and raylib are untouched.

## Exact source and Git inventory

- [Review-only source patch](evidence/slice2-final-review-source.patch): three files,
  against the accepted starting S2 tree.
- [Current combined baseline source patch](evidence/slice2-final-baseline-source.patch):
  all 41 source/architecture paths against HEAD, including S1 and S2.
- [Final receipt](evidence/slice2-final-review.json): source, binary, preserved-input
  and raw-evidence hashes, executed commands, results and explicit limitations.
- [Complete layered Git inventory](evidence/slice2-final-review-inventory.txt): every
  modified/untracked path, its original S1/S2 layer and this review's additions.

Both patches pass reverse checks and exact forward reconstruction in temporary
directories. Reports/evidence are excluded from source patches to prevent recursive
patch contents, and are covered by the full inventory and receipts. HEAD and index
are unchanged; nothing was staged, committed, pushed, dispatched or published.
Security settings and dependency pins remain unchanged. Stop for Golden Wrench.
''')
# Create all inventory members before taking the complete porcelain snapshot.
receipt.touch();inventory.touch()
rows=[s for s in subprocess.check_output(['git','status','--porcelain=v1','-uall','-z']).decode().split('\0') if s]
old_layers={v['path']:v['layer'] for v in old['git_inventory']['rows']};items=[]
for row in rows:
 n=row[3:];layer=old_layers.get(n,'S2 final review: new report/evidence')
 if n in amended:layer='S2 final review amendment; original layer: '+layer
 items.append({'status':row[:2],'path':n,'layer':layer})
counts={'total':len(items),'modified':sum(x['status']==' M' for x in items),'untracked':sum(x['status']=='??' for x in items),'starting_paths':163,'starting_preserved':160,'starting_amended':3,'new_review_paths':len(items)-163}
inventory.write_text('HEAD '+checkpoint['head']+'\nIndex unchanged and empty. No Git mutations.\n'+json.dumps(counts,sort_keys=True)+'\n\nSTATUS | LAYER | EXACT PATH\n'+'\n'.join(v['status']+' | '+v['layer']+' | '+v['path'] for v in items)+'\n')
failures=[{'stage':'original regression fixture','result':1,'failure':'legacy dirty import tombstone: bare-file identity used the wrong root','repair':'Use /module.void for legacy entry-parent overlays; retain /src/module.void for manifest mode','log':ref(raw/'slice2-final-review-original-tests.log.txt'),'fixture':ref(raw/'original-oracle.cpp.txt')},{'stage':'corrected legacy oracle, original early mutation fixture','result':1,'failure':'Expected VOID5011 but received VOID3002 before E entry admission','repair':'Trigger mutation during the read of excluded E-only zz-slow.void, after entry admission','log':ref(raw/'slice2-final-review-original-corrected-oracle-tests.log.txt'),'fixture':ref(raw/'regressions-before-repair.cpp.txt')},{'stage':'original production core with corrected focused regression','result':1,'failure':'Expected VOID5009 for same-name symlink replacement during recapture, got VOID5011','repair':'Compare successful observations; preserve admission/read exceptions; retain final fail-closed checks','log':ref(raw/'slice2-final-review-original-focused-tests.log.txt'),'compile_and_run':json.loads((raw/'original-focused.json').read_text()),'fixture':ref(raw/'original-focused-source.cpp.txt')}]
report.write_text(report.read_text()+'\nFinal expanded Git inventory: '+str(counts['total'])+' paths ('+str(counts['modified'])+' modified, '+str(counts['untracked'])+' untracked);\n'+str(counts['new_review_paths'])+' new review report/evidence paths. The complete per-path inventory above\ndistinguishes every original S1/S2 layer.\n')
failures.append({'stage':'Windows native nested harness','result':2,'failure':'Debug nested Python script omitted from 126-file source packet; first 26 native build/verification/test commands passed','repair':'Hash-check shared nested.py and execute against both identical native binaries; retain original FAIL receipt','log':ref(raw/'windows-Debug-nested.log.txt'),'original_receipt':ref(raw/'windows-native-results.json'),'corrective_receipt':ref(raw/'windows-nested-corrective-native-results.json')})
record={'date':'2026-11-08','status':'LOCAL FINAL S2 QUALIFICATION COMPLETE; GOLDEN WRENCH FINAL REVIEW PENDING; STOP BEFORE S3','baseline':checkpoint['head'],'scope':'Tombstone invariant proof/regressions and recapture diagnostic classification only','host':old['host'],'fatal_environment':old['fatal_environment'],'starting_checkpoint':ref(raw/'starting-checkpoint.json'),'source_inputs_sha256':{n:sha(r/n) for n in old['source_inputs_sha256']},'review_amended':amended,'starting_files_preserved_sha256':preserved,'linux_profiles':profiles,'selected_linux_outcomes':{'passed':108,'failed':0,'entire_repository_suite':False},'windows_native':native,'windows_final_qualification':{'result':'PASS','successful_original_core_commands':26,'preserved_original_harness_failure':'Debug-nested exit 2: missing packet script','corrective_nested_profiles':['Release','Debug'],'selection_assertions_per_profile':353,'project_processes_per_profile':88,'nested_processes_per_profile':11,'legacy_checks_per_profile':17},'windows_nested_corrective_supplement':supp,'windows_scope':'Windows 10 x64 19045 incremental native rebuild/link, not a clean current-tree CMake build; no native admission or provider checks weakened','new_failures_and_repairs':failures,'final_results':'PASS on all recorded corrected profiles','patches':patches,'report':ref(report),'git_inventory':{'evidence':ref(inventory),'counts':counts,'rows':items},'raw_evidence':[ref(p) for p in sorted(raw.iterdir()) if p.is_file()],'remaining_limits':['New inotify/permission recapture mutations are Linux-only','Windows symlink/reparse fixture privilege unavailable','Windows persistent native-cache warm hits unavailable','Windows fatal sanitizers not executed','Windows Server 2022 and macOS not executed','Full clean current-tree Windows CMake build not executed','Project editor protocol/live reload limits unchanged from original S2 report','S3 and later roadmap features not started'],'operational_boundaries':{'head_unchanged':True,'index_sha256':checkpoint['index_sha256'],'index_unchanged':True,'index_empty':True,'staging_commit_push_dispatch_publication':False,'dependency_pin_or_security_relaxation':False,'hosted_ci':'STRICT HOLD until full project/raylib milestone; Boss owns final integrated gate'}}
receipt.write_text(json.dumps(record,indent=2)+'\n');print(json.dumps({'counts':counts,'raw_evidence':len(record['raw_evidence']),'linux_outcomes':108,'windows_commands':len(native['commands'])+len(supp['commands'])},indent=2))
