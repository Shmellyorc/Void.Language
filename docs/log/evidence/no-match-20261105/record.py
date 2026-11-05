from pathlib import Path
import json,hashlib,shutil,statistics,subprocess
root=Path.cwd();d=Path(Path('/tmp/void-no-match-current').read_text());out=root/'docs/log/evidence/no-match-20261105';out.mkdir()
shutil.copy2(__file__,d/'record.py')
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
rows=json.loads((d/'samples.json').read_text());confirm=json.loads((d/'confirm-heavy-samples.json').read_text())
def summary(rows):
 groups={}
 for r in rows:
  if r['sample']<0:continue
  cell=(r['workload'],r.get('command'),r.get('shape'),r.get('stage'),r['version'])
  groups.setdefault(str(cell),[]).append(r)
 return {key:{'samples_ms':[r['wall_ns']/1e6 for r in a],'median_ms':statistics.median(r['wall_ns']/1e6 for r in a),'min_ms':min(r['wall_ns']/1e6 for r in a),'max_ms':max(r['wall_ns']/1e6 for r in a),'cpu_median_ms':statistics.median(r['cpu_seconds']*1000 for r in a)} for key,a in groups.items()}
receipts=[]
for path in sorted((d/'receipts').glob('*/receipt.json')):
 rec=json.loads(path.read_text());rec['private_receipt']=str(path);receipts.append(rec)
identity=json.loads((d/'identity.json').read_text());identity['env']={k:v for k,v in identity['env'].items() if k in ['PATH','LANG','LC_ALL','LC_CTYPE','VOID_STDLIB_ROOT','LD_LIBRARY_PATH','CXX','CC']}
files=[p for p in d.iterdir() if p.is_file() and p.suffix in ['.cpp','.hpp','.py','.patch','.void','.stdout','.stderr']]
# Preserve the exact initial failed binary's identity, all final binary identities,
# libraries/toolchain identities and sources; large binaries remain private.
binaries=[p for p in d.iterdir() if p.is_file() and (p.stat().st_mode & 0o111) and p.suffix not in ['.py']]
build=Path(identity['retained_build']);binaries += [build/n for n in ['libvoid_runtime.a','libvoid_llvm.a','libvoid_core.a','libvoid_native_fixture.a','libvoid_cache_identity.a']]
binaries += [Path(identity['flags'][0]),root/'.toolchains/llvm-23.1.4/bin/ld.lld']
result={'date':'2026-11-05','timezone':'America/Vancouver','decision':'DISCARDED: material file200 improvement, but no-heavy-regression gate not established; confirmatory candidate console block exceeds 20 ms. Production restored exactly. Stop after one candidate. LOG-PERF OPEN.',
 'private_evidence':str(d),'identity':identity,'stack':{'soft':1048576,'hard':1048576,'checked_by_hosts':True},'gc_budget':256,
 'protocol':{'primary_order':['original','candidate','candidate','original'],'primary_workloads':['complete original file200 including acquisition,200 writes,required fsync,close','prepared direct file200 comparator','short quote200 and frame200','heavy escaped 1KiB x100 framing and console'],'per_cell':'one warmup, three measured samples; compilation excluded; all clocks inside invoked workloads','confirmatory_order':['candidate','original','original','candidate'],'confirmatory_reason':'Primary heavy console +6.4% slower pooled with moving control medians; resolve heavy regression gate using same binaries, no candidate changes.','instrumentation':'plain Runtime TUs, no translation profiling/allocation counters; existing source construction observer and bounded byte/resource oracles retained','host_limits':'active desktop; no CPU isolation/frequency pinning/randomization/confidence intervals'},
 'primary_samples':rows,'primary_summary':summary(rows),'primary_events':json.loads((d/'events.json').read_text()),
 'confirmatory_heavy_samples':confirm,'confirmatory_heavy_summary':summary(confirm),'confirmatory_events':json.loads((d/'confirm-heavy-events.json').read_text()),
 'commands':receipts,'command_status_counts':{str(status):sum(r['returncode']==status for r in receipts) for status in {r['returncode'] for r in receipts}},
 'failure':{'status':1,'kind':'harness setup error, not a signal or sanitizer finding','reason':'Native test used uint64 all-ones for i32 -1; list_write rejects invalid scalar bits. Corrected test uses 0xffffffffu. Initial source, driver, binary hash and incident retained.','incident':str(d/'crashes/incident-5afhd0rl')},
 'correctness':{'focused_native_checks':879,'additional_native_checks':876,'strict_native_checks':876,'existing_ownership_checks':31224,'logging_native_checks':1688,'jit_and_aot':['normal JIT','O2 JIT','normal linked AOT','O2 linked AOT'],'fixtures':['translation_no_match.void','tests/text_encoding/text.void'],'allocation_failures_no_match':{'0_rules':4,'1_rule':4,'81_rules':4,'256_rules':4,'257_rules_fallback':262},'strict_scope':'Current candidate TU and native-only host overlaid on retained ASan/UBSan Runtime. Older strict frontend intentionally not invoked; no full current strict-source qualification claimed.'},
 'restoration':{'runtime_source_sha256':sha(root/'src/runtime/text_case.cpp'),'head_runtime_source_sha256':hashlib.sha256(subprocess.check_output(['git','show','HEAD:src/runtime/text_case.cpp'])).hexdigest(),'candidate_test_removed_from_tests':not (root/'tests/text_encoding/translation_no_match.void').exists()},
 'binary_hashes':{str(p):sha(p) for p in binaries},'preserved_artifact_hashes':{p.name:sha(p) for p in files},'limitations':['file200 <10 ms remains FAIL in every candidate sample','Heavy thresholds only bounded host/JIT O2 screens; candidate no-regression not demonstrated','No causal claim from unrandomized active-host medians','No full current Debug/strict regression suite or performance matrix across JIT/AOT modes','No tail/pause-time, peak native-live memory or cross-platform qualification','Existing full-input reserve and final immutable-string publication pause limitations unchanged']}
(out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
for p in files:shutil.copy2(p,out/p.name)
# Store the recorder's failed process output and incident manifest as reviewable
# small artifacts; complete private incident directory is preserved in place.
incident=d/'crashes/incident-5afhd0rl';incident_out=out/'initial-harness-failure';incident_out.mkdir()
for p in incident.iterdir():
 if p.is_file() and p.stat().st_size<2_000_000:shutil.copy2(p,incident_out/p.name)
(out/'SHA256SUMS.json').write_text(json.dumps({str(p.relative_to(out)):sha(p) for p in out.rglob('*') if p.is_file()},indent=2)+'\n')
print('Recorded',len(rows)+len(confirm),'rows;',len(receipts),'commands;',len(files),'small artifacts;',out)
for key,value in result['primary_summary'].items():print(key,value)
for key,value in result['confirmatory_heavy_summary'].items():print('confirmation',key,value)
