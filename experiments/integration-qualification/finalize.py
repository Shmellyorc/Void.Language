import qualify as h
import json,xml.etree.ElementTree as ET,re,subprocess,shutil
# Run only after the fixed qualification commands finish. Expected historical failure is separate.
commands=[json.loads(x) for x in (h.E/'commands.jsonl').read_text().splitlines()];by={r['label']:r for r in commands}
results={}
for profile in ['release','debug','strict']:
 results[profile]={}
 for group,suffix in [('common',''),('boundary','-boundary')]:
  tree=ET.parse(h.E/(profile+suffix+'-junit.xml')).getroot()
  tests=tree.findall('testcase');failed=[t.attrib['name'] for t in tests if t.find('failure') is not None or t.find('error') is not None];skipped=[t.attrib['name'] for t in tests if t.find('skipped') is not None]
  results[profile][group]={'tests':len(tests),'failures':failed,'skipped':skipped,'reported':tree.attrib}
  label=profile+('-combined-ctest' if group=='common' else '-boundary-ctest')
  assert by[label]['stack_bytes']==1048576
  assert by[label]['returncode']==0 and not failed and not skipped,(label,results[profile][group])
 transcript=(h.E/(profile+'-LastTest.log')).read_text()
 for marker in ['1690 logging native checks passed','17834 ownership/root checks passed','828 logging native checks passed','normal with semantic fault=resource-acquisition observed ordinal=2 passed','normal with semantic fault=first-body-allocation observed ordinal=3 passed','O2 with semantic fault=resource-acquisition observed ordinal=2 passed','O2 with semantic fault=first-body-allocation observed ordinal=3 passed']:
  assert marker in transcript,(profile,marker)
 build=h.ROOT/('build/llvm23-'+profile)
 # Preserve final executable/archive identities and the actual recipes/configuration.
 files={p.name:h.digest(p) for p in build.iterdir() if p.is_file() and (p.suffix=='.a' or p.name.startswith('void_') or p.name in ['voidc','voidls','void-native.txt','void-native-resources.txt'])}
 (h.E/(profile+'-final-binaries.json')).write_text(json.dumps(files,indent=2))
 shutil.copy2(build/'CMakeCache.txt',h.E/(profile+'-CMakeCache.txt'))
 for target in ['void_log_host','voidc_objects','void_advanced_tests','void_http_source_runner']:
  folder=build/'CMakeFiles'/(target+'.dir')
  for name in ['flags.make','link.txt']:
   if (folder/name).exists():shutil.copy2(folder/name,h.E/(profile+'-'+target+'-'+name))
editor={}
for label in ['editor-baseline','editor-integrated','editor-debug','editor-strict']:
 assert by[label]['returncode']==0
 folders=sorted((h.E/(label+'-capture')).glob('client-*'));assert len(folders)==12
 receipts=[]
 for folder in folders:
  record=json.loads((folder/'final.json').read_text());assert record['returncode']==0 and not record['stderr'],(label,folder,record)
  limits=(folder/'limits.txt').read_text();stack=next(x for x in limits.splitlines() if x.startswith('Max stack size'));assert '1048576' in stack and stack.split()[3:5]==['1048576','1048576'],stack
  assert not (folder/'failure.json').exists()
  receipts.append({'client':folder.name,'returncode':record['returncode'],'stack':stack})
 editor[label]=receipts
assert by['advanced-original-current-runtime']['returncode']==1
assert 'with allocation failure/root publication' in (h.E/'advanced-original-current-runtime.stderr').read_text()
for label in ['http-integrated','http-baseline-retained','recheck-durability-proof','recheck-logging-ir-retained']:assert by[label]['returncode']==0
assert 'REAL_DURABILITY {"writes":400,"bytes":30000,"fsync":2}' in (h.E/'recheck-durability-proof.stderr').read_text()
assert 'TOTAL fused=0 local_cleanup=18 compact_write=1 invalid_private_operations_rejected=3' in (h.E/'recheck-logging-ir-retained.stdout').read_text()
markers=[]
for p in h.E.glob('*LastTest.log'):
 if re.search(r'ERROR: (AddressSanitizer|LeakSanitizer)|SUMMARY: (AddressSanitizer|UndefinedBehaviorSanitizer)|[^A-Za-z]runtime error: ',p.read_text()):markers.append(p.name)
assert not markers,markers
summary={'qualified':True,'profiles':results,'editor_processes':editor,'strict_sanitizer_markers':markers,'stack_soft_and_hard_bytes':1048576,'original_advanced_failure_preserved':True,'production_source_changed':False,'platform_scope':'local Linux x86-64 only','hosted_CI_run':False,'SIMD_implemented_or_benchmarked':False}
(h.E/'qualification-results.json').write_text(json.dumps(summary,indent=2))
print(json.dumps({k:sum(v['tests'] for v in x.values()) for k,x in results.items()}),flush=True)
