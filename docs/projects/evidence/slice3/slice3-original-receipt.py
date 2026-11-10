from pathlib import Path
import hashlib,json,shutil,subprocess,xml.etree.ElementTree as ET
r=Path.cwd();build=r/'build';e=r/'docs/projects/evidence';raw=e/'slice3';w=Path('/home/shmellyorc/VM/project-slice3')
start=json.loads((build/'slice3-start/checkpoint.json').read_text());previous=json.loads((e/'slice2-final-review.json').read_text());patches=json.loads((build/'slice3-patches.json').read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def ref(p):p=Path(p);return {'path':str(p.relative_to(r)),'sha256':sha(p),'bytes':p.stat().st_size}
def read(p):return json.loads(Path(p).read_text())
def suite(p,count):
 tree=ET.parse(p).getroot();assert int(tree.get('tests'))==count and tree.get('failures')=='0' and tree.get('skipped')=='0',tree.attrib
 cases=[]
 for c in tree.findall('testcase'):
  assert c.find('failure') is None and c.find('error') is None and c.get('status')=='run',c.attrib
  cases.append({'name':c.get('name'),'seconds':float(c.get('time')),'stdout':c.findtext('system-out',''),'result':'PASS'})
 assert len(cases)==count
 return {'passed':count,'failed':0,'cases':cases}
regex='^(project_manifest.*|project_selection|project_sessions|project_profiles|project_protocol|project_discovery_cli|project_nested_cli|cli_contract|cli_install_resources|frontend|modules|numeric|analysis|tooling|sessions|source_cli|modules_cli|analysis_cli|numeric_cli|local_libraries|application_workflow_stage9|debug_cache_processes|automatic_cache_processes|os_frontend|lsp_os|os_errors|lsp_stage7|lsp_stage8|lsp_protocol|lsp_sprint2|stage11_globals|stage11_globals_stack|stage11_globals_aot|lsp_stage11_globals|generics_source|generics_hint_execution|generics_hint_lsp|generics_cli)$'
server_regex='^(project_protocol|lsp_.*|generics_hint_lsp)$';profiles=[]
for name,n in [('release',33),('debug',33),('strict',30)]:
 b=build/('llvm23-'+name);base=suite(build/f'slice3-{name}-verified.xml',42);server=suite(build/f'slice3-{name}-final-server.xml',n)
 assert next(c['stdout'] for c in base['cases'] if c['name']=='project_sessions').strip()=='project_sessions: 157 checks passed'
 assert '43 process checks passed' in next(c['stdout'] for c in base['cases'] if c['name']=='project_profiles')
 assert next(c['stdout'] for c in server['cases'] if c['name']=='project_protocol').strip()=='project_protocol: 74 checks passed'
 profiles.append({'profile':name,'build_type':'RelWithDebInfo' if name=='strict' else name.title(),'sanitizers':name=='strict','core_cli_matrix':base,'final_server_matrix':server,'build_argv':['cmake','--build',str(b),'--target','void_project_sessions','void_project_selection','void_project_manifest','void_project_header','void_tests','voidc','voidls','void_globals','void_generics_tests','-j2'],'core_cli_test_argv':['ctest','--test-dir',str(b),'-R',regex,'--output-on-failure','--output-junit','../slice3-'+name+'-verified.xml'],'final_server_build_argv':['cmake','--build',str(b),'--target','voidls','-j2'],'final_server_test_argv':['ctest','--test-dir',str(b),'-R',server_regex,'--output-on-failure','--output-junit','../slice3-'+name+'-final-server.xml'],'final_binary_sha256':{n:sha(b/n) for n in ['voidc','voidls','void_tests','void_project_manifest','void_project_header','void_project_selection','void_project_sessions','void_globals','void_generics_tests']}})
native=read(w/'final-native-results.json');native_server=read(w/'final-server-native-results.json');assert native['status']==native_server['status']=='PASS'
assert native['platform']=='Windows-10-10.0.19045-SP0'
assert all(c['returncode']==0 for c in native['commands']+native['retained_original_successes']+native['retained_corrected_release_successes']+native_server['commands'])
for name,h in native_server['source_inputs_sha256'].items():assert sha(r/name)==h,name
assert set(n for n,h in native['source_inputs_sha256'].items() if h!=native_server['source_inputs_sha256'][n])=={'src/lsp/main.cpp','tests/project/protocol.py','tools/lsp_client.py'}
for profile in ['Release','Debug']:
 pre='continued-' if profile=='Release' else 'final-'
 for file,check in [('manifest','8788 checks passed'),('selection','353 checks passed'),('sessions','157 checks passed'),('profiles','43 process checks passed'),('project-cli','88 process checks passed'),('nested','11 process checks passed'),('legacy','17 checks passed')]:assert check in (w/(pre+profile+'-'+file+'.log')).read_text(errors='replace'),file
 assert '74 checks passed' in (w/('server-final-'+profile+'-project-protocol.log')).read_text()
editor=read(build/'slice3-editor-project-host-verified.json');assert editor['status']=='PASS' and len(editor['events'])==13
legacy=(build/'slice3-editor-legacy-host-verified.log').read_text(errors='replace');assert 'PASS real editor restricted trust' in legacy and 'PASS real editor multi-root' in legacy and 'PASS real editor:' in legacy
assert 'pass 7' in (build/'slice3-editor-verified-tests.log').read_text()
installed=read(build/'slice3-installed/results.json');tooling=read(build/'slice3-installed-final/results.json');assert installed['status']==tooling['status']=='PASS'
assert tooling['server_sha256']==sha(build/'llvm23-release/voidls')
assert next(h for n,h in installed['installed_binary_sha256'].items() if n.endswith('/voidc'))==sha(build/'llvm23-release/voidc')
suite(build/'slice3-cli-install.xml',1)
assert sha(r/'.git/index')==start['index_sha256'];assert subprocess.check_output(['git','diff','--cached','--name-only'])==b''
assert subprocess.check_output(['git','rev-parse','HEAD']).decode().strip()==start['head']
amended={n:{'starting_sha256':h,'final_sha256':sha(r/n)} for n,h in start['files_sha256'].items() if sha(r/n)!=h};assert len(amended)==13,amended
preserved={n:h for n,h in start['files_sha256'].items() if n not in amended};assert len(preserved)==234 and not any(n.startswith('docs/projects/') for n in amended)
assert len(patches[0]['changed_paths'])==29 and len(patches[1]['changed_paths'])==57
raw.mkdir(exist_ok=True)
for p in sorted(build.glob('slice3-*')):
 if p.is_file() and p.suffix in ['.log','.xml','.json','.cpp','.ts','.py']:
  shutil.copyfile(p,raw/(p.name+('.txt' if p.suffix in ['.log','.cpp','.ts'] else '')))
shutil.copyfile(build/'slice3-start/checkpoint.json',raw/'starting-checkpoint.json')
for folder,prefix in [('slice3-installed','installed'),('slice3-installed-final','installed-final')]:
 for name in ['results.json','protocol.json']:shutil.copyfile(build/folder/name,raw/(prefix+'-'+name))
for p in sorted(w.iterdir()):
 if p.is_file() and p.suffix in ['.log','.json','.py','.cmd','.rsp','.txt','.cpp']:
  shutil.copyfile(p,raw/('windows-'+p.name+('.txt' if p.suffix in ['.log','.cpp'] else '')))
for profile in ['release','debug','strict']:
 b=build/('llvm23-'+profile)
 for n in ['project-discovery','project-nested','project-protocol','project-profiles']:shutil.copyfile(b/(n+'-evidence.json'),raw/('linux-'+profile+'-'+n+'.json'))
for profile in profiles:
 name=profile['profile'];profile['core_cli_matrix']['evidence']=ref(raw/f'slice3-{name}-verified.xml');profile['final_server_matrix']['evidence']=ref(raw/f'slice3-{name}-final-server.xml')
report=r/'docs/projects/SLICE3.md';receipt=e/'slice3-local.json';inventory=e/'slice3-inventory.txt';decision=e/'slice3-profile-decision.txt'
decision.write_text('''Boss decision, Slice 3: approved Option 1 with strict qualification.
Apply actual optimization and cache-profile behavior. Warn visibly and structurally
only when the selected profile requests unsupported line/full debug information;
none stays quiet. Preserve the requested value in validated metadata. Cover both
schema defaults, authored overrides, warnings, actual optimization and cache conflicts.
Standalone compilation and compiler checks remain unchanged. Actual mode-specific
emission is deferred to its separate review milestone. No hosted CI or Git writes.

Source audit clarification: the unchanged baseline DIBuilder already emits
source/type/location metadata. VOID5012 identifies unsupported profile-controlled
emission. It does not claim the output lacks baseline debug records; none does not
newly strip those records. No emitter implementation is included in S3.
''')
shutil.copyfile(build/'slice3-report.md',report);receipt.touch();inventory.touch()
rows=[v for v in subprocess.check_output(['git','status','--porcelain=v1','-uall','-z']).decode().split('\0') if v];old_layers={v['path']:v['layer'] for v in previous['git_inventory']['rows']};source_scope=patches[0]['source_scope'];items=[]
for row in rows:
 n=row[3:];layer=old_layers.get(n,'S3 new source/documentation' if n in source_scope else 'S3 new report/evidence')
 if n in amended:layer='S3 amendment; original layer: '+layer
 items.append({'status':row[:2],'path':n,'layer':layer})
counts={'total':len(items),'modified':sum(v['status']==' M' for v in items),'untracked':sum(v['status']=='??' for v in items),'starting_paths':247,'starting_preserved':234,'starting_amended':13,'new_s3_source_paths':16,'new_s3_evidence_paths':len(items)-247-16}
inventory.write_text('HEAD '+start['head']+'\nUnchanged empty index. Expanded working-tree inventory.\n'+json.dumps(counts,sort_keys=True)+'\n\nSTATUS | LAYER | EXACT PATH\n'+'\n'.join(v['status']+' | '+v['layer']+' | '+v['path'] for v in items)+'\n')
report.write_text(report.read_text()+f"\nFinal expanded Git inventory: {counts['total']} paths ({counts['modified']} modified,\n{counts['untracked']} untracked), including {counts['new_s3_evidence_paths']} new S3 report/evidence paths.\n")
failures=[]
for label,log,repair in [
 ('npm PATH','slice3-editor-first-typecheck.log.txt','Use existing pinned Node 24.19.0 bin directory'),
 ('nullable entry typing','slice3-editor-corrected-typecheck.log.txt','Narrow legacy fixture entry values without removing path assertions'),
 ('old default test server','slice3-editor-first-tests.log.txt','Select matching built server; separately verify actual old-server rejection and explicit legacy support'),
 ('TOML source invalidation hint','slice3-initial-tests.log.txt','Exclude TOML/D-only IDs from completed E graph comparison'),
 ('unstable initial cache fixture','slice3-initial-tests.log.txt','Bootstrap excluded cache directory before cold/warm comparison'),
 ('editor Trash unavailable','slice3-editor-project-host-original.log.txt','Disable Trash only in disposable editor fixture profile; test actual deletion'),
 ('coalesced project restart lost','slice3-editor-project-switch-original.log.txt','Carry pending restart intent through readiness request coalescing'),
 ('native source inventory omitted LSP','windows-original-native-results.json','Require exact nine intended native baseline source differences; fresh extracted root'),
 ('Windows generic path oracle','windows-protocol-original-Release-protocol.log.txt','Compare exact generic path spelling with Path.as_posix; hash-check same binaries'),
 ('native legacy protocol argv','windows-legacy-argv-original-continued-Release-legacy-lsp-protocol.log.txt','Pass required fixture directory; retain failed invocation'),
 ('relative legacy entry startup','slice3-relative-entry-probe.json','Absolutize the entry before taking its parent; real relative LSP regression and final server rebuild')]:
 failures.append({'stage':label,'original':ref(raw/log),'repair':repair})
logging=[ref(p) for p in sorted((r/'docs/log/evidence').glob('lsp-llvm23-*/receipt.json')) if str(p.relative_to(r)) in {x['path'] for x in items if x['status']=='??'}]
assert len(logging)==3
archives=[{'path':str(p),'sha256':sha(p),'bytes':p.stat().st_size,'local_only':True} for p in sorted(w.glob('*source.zip'))]
record={'date':'2026-11-08','baseline':start['head'],'status':'LOCAL S3 QUALIFICATION COMPLETE; GOLDEN WRENCH REVIEW PENDING; STOP BEFORE S4','host':previous['host'],'fatal_environment':previous['fatal_environment'],'scope':'Compiler-owned project sessions, additive voidls/editor protocol and project CLI optimization/cache profiles; no S4 Runtime metadata','starting_checkpoint':ref(raw/'starting-checkpoint.json'),'starting_files_preserved_sha256':preserved,'starting_files_amended':amended,'source_inputs_sha256':{n:sha(r/n) for n in source_scope},'profile_decision':ref(decision),'linux_profiles':profiles,'selected_linux_outcomes':{'core_cli':126,'final_server':96,'passed':222,'failed':0,'entire_repository_suite':False,'strict_unregistered_small_stack_cases':['lsp_const_recovery_stack','lsp_io_stack','lsp_text_encoding_stack']},'windows_main_qualification':native,'windows_final_server_qualification':native_server,'windows_source_archives':archives,'windows_baseline_ninja_sha256':{p.name:sha(p) for p in w.glob('*-build.ninja')},'editor':{'project_host':editor,'legacy_host_passed':['trusted','restricted trust','multi-root'],'node_tests_passed':7,'actual_older_server_sha256':sha(build/'slice3-older-voidls'),'server_sha256':sha(build/'llvm23-release/voidls')},'installed_components':installed,'final_installed_tooling':tooling,'cli_install':suite(build/'slice3-cli-install.xml',1),'new_failures_and_repairs':failures,'logging_test_receipts':logging,'patches':patches,'report':ref(report),'git_inventory':{'evidence':ref(inventory),'counts':counts,'rows':items},'raw_evidence':[ref(p) for p in sorted(raw.iterdir()) if p.is_file()],'limits':['Windows 10 incremental native builds; no full clean Windows CMake qualification','Windows symlink/reparse fixture privilege and persistent cache warm hits unavailable','Windows fatal sanitizer, Server 2022 and macOS not executed','Windows/macOS editor host, VSCodium and installed VSIX not executed','Actual profile-specific debug-information emission deferred; baseline metadata retained','No S4 or later feature implementation, no hosted CI'],'operational_boundaries':{'head_unchanged':True,'index_empty':True,'index_unchanged':True,'index_sha256':start['index_sha256'],'no_staging_commit_push_dispatch_publication':True,'no_dependency_or_security_relaxation':True,'hosted_gate':'STRICT HOLD until complete dynamic-default raylib milestone; one Boss-controlled integrated seven-job acceptance'}}
receipt.write_text(json.dumps(record,indent=2)+'\n');print(json.dumps({'counts':counts,'raw_evidence':len(record['raw_evidence']),'failed_runs_preserved':len(failures),'linux_outcomes':222,'windows_final_server_commands':len(native_server['commands'])},indent=2))
