from pathlib import Path
import hashlib,json,os,resource,subprocess,time,shutil,sys
ROOT=Path(__file__).resolve().parents[2]; AREA=Path(__file__).resolve().parent; E=AREA/'evidence'
def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def stack():
 resource.setrlimit(resource.RLIMIT_STACK,(1048576,1048576))
 assert resource.getrlimit(resource.RLIMIT_STACK)==(1048576,1048576)
def run(label,args,small=True,timeout=10800,check=False,cwd=ROOT,env=None):
 assert not (E/(label+'.stdout')).exists(),label
 start=time.time(); cpu=resource.getrusage(resource.RUSAGE_CHILDREN)
 options={**os.environ,'VOID_STDLIB_ROOT':str(ROOT/'stdlib'),'ASAN_OPTIONS':'detect_leaks=1:abort_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1',**(env or {})}
 with (E/(label+'.stdout')).open('wb') as out,(E/(label+'.stderr')).open('wb') as err:
  p=subprocess.run(list(map(str,args)),cwd=cwd,stdout=out,stderr=err,timeout=timeout,preexec_fn=stack if small else None,env=options)
 after=resource.getrusage(resource.RUSAGE_CHILDREN)
 record=dict(label=label,args=list(map(str,args)),returncode=p.returncode,wall=time.time()-start,cpu=after.ru_utime+after.ru_stime-cpu.ru_utime-cpu.ru_stime,stack_bytes=1048576 if small else None,start=start,ASAN_OPTIONS=options['ASAN_OPTIONS'],UBSAN_OPTIONS=options['UBSAN_OPTIONS'])
 with (E/'commands.jsonl').open('a') as f:f.write(json.dumps(record)+'\n')
 print(label,p.returncode,round(record['wall'],3),flush=True)
 if check and p.returncode:raise RuntimeError(label)
 return p
if __name__=='__main__':
 old=json.loads((ROOT/'experiments/internal-performance/evidence/candidate-source-identity.json').read_text())
 changed=[p for p,v in old.items() if not (ROOT/p).is_file() or digest(ROOT/p)!=v]
 sources={str(p.relative_to(ROOT)):digest(p) for d in ['src','include','stdlib'] for p in (ROOT/d).rglob('*') if p.is_file()}
 info=dict(head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),source_sha256=sources,previous_final_source_differences=changed,root_readme=digest(ROOT/'README.md'),existing_simd_note=digest(ROOT/'docs/VOIDC_AUTOMATIC_SIMD_ROADMAP.md'),index_diff=subprocess.check_output(['git','diff','--cached','--stat'],cwd=ROOT,text=True))
 (E/'initial-identity.json').write_text(json.dumps(info,indent=2));assert not changed,changed
 (E/'initial-working-diff.patch').write_bytes(subprocess.check_output(['git','diff','--binary'],cwd=ROOT))
 shutil.copy2(ROOT/'src/ir/local_cleanup.inc',E/'initial-local_cleanup.inc')
 # Reuse only the test selection, not the old evidence-writing harness.
 original=(ROOT/'experiments/internal-performance/validate.py').read_text()
 import ast
 tree=ast.parse(original); exact=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='exact' for t in n.targets))
 targets=['void_text_performance','void_channel_benchmark','voidc','voidls','void_gc_buffers','void_runtime_tests','void_runtime_failures','void_runtime_invariants','void_gc_schemas','void_tests','void_runtime_generated','void_fixed_arrays_tests','void_growable_arrays_tests','void_dictionaries_tests','void_advanced_tests','void_json_host','void_collections_stdlib_host','void_text_ownership','void_strings_tests','void_io_ownership','void_async_tests','void_retention_current','void_http_io_performance','void_log_host','void_log_probe_normal','void_log_probe_optimized','void_runtime_aot_normal','void_runtime_aot_optimized','void_text_audit']
 for profile in ['release','debug','strict']:
  build=ROOT/('build/llvm23-'+profile)
  result=run(profile+'-build',['cmake','--build',build,'-j2','--target',*targets],small=False)
  if result.returncode:continue
  inventory=json.loads(subprocess.check_output(['ctest','--test-dir',build,'--show-only=json-v1'],text=True))
  selected=[t['name'] for t in inventory['tests'] if t['name'] in exact or t['name'].startswith('json_execution_') or t['name'] in ['text_encoding_'+x for x in ('text','hex','base64','ascii','oracles','interop','large','errors')]]
  (E/(profile+'-selected-tests.json')).write_text(json.dumps(selected,indent=2))
  run(profile+'-combined-ctest',['ctest','--test-dir',build,'--output-on-failure','--output-junit',E/(profile+'-junit.xml'),'-j2','-R','^('+'|'.join(selected)+')$'])
  shutil.copy2(build/'Testing/Temporary/LastTest.log',E/(profile+'-LastTest.log'))
  binaries={p.name:digest(p) for p in build.iterdir() if p.is_file() and (p.name in targets or p.suffix=='.a')}
  (E/(profile+'-binaries.json')).write_text(json.dumps(binaries,indent=2))
