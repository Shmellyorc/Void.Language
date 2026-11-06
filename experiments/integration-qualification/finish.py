"""Finalize only the fixed complete campaign; never turn a failed check into a pass."""
import qualify as h
import json,time,hashlib
required=[p+'-combined-ctest' for p in ['release','debug','strict']]+[p+'-boundary-ctest' for p in ['release','debug','strict']]+['editor-debug','editor-strict']
while True:
 lines=(h.E/'commands.jsonl').read_text().splitlines();records=[]
 for line in lines:
  try:records.append(json.loads(line))
  except json.JSONDecodeError:pass
 by={r['label']:r for r in records}
 if all(x in by for x in required):break
 time.sleep(5)
assert all(by[x]['returncode']==0 for x in required),{x:by[x]['returncode'] for x in required}
h.run('final-qualification-verification',['python3',h.AREA/'finalize.py'],small=False,check=True)
result=json.loads((h.E/'qualification-results.json').read_text())
p=h.ROOT/'docs/compiler-health/INTEGRATED_PERFORMANCE_QUALIFICATION.md';s=p.read_text()
s=s.replace('**Qualification in progress.** Final counts and disposition will be filled from\nthis run\'s preserved receipts before handoff. No SIMD implementation or SIMD timing\nexperiment is part of this work.','**Recommendation: RETAIN the integrated implementation for Golden Wrench/Boss review.**\nAll selected tests passed: **85 Release, 85 Debug and 84 strict ASan/UBSan/leak**\non this Linux host. No reproducible integration correctness defect was\nfound in the reviewed boundaries or this campaign. Performance regressions and\nunclassified historical failures remain explicit. This is local qualification,\nnot independent review or release acceptance. No SIMD implementation or SIMD\nbenchmark was performed.')
rows=[]
for profile,r in result['profiles'].items():
 c=r['common']['tests'];b=r['boundary']['tests'];rows.append(f'| {profile.title()} | [{c}/{c}](../../experiments/integration-qualification/evidence/{profile}-junit.xml) | [{b}/{b}](../../experiments/integration-qualification/evidence/{profile}-boundary-junit.xml) | 12/12 normal exits |')
new='''The fixed campaign passed **254 CTest executions**: **85 Release, 85 Debug and
84 strict**, with zero failures or skips. These are bounded relevant selections,
not a claim to run every registered repository test. The strict configuration
does not register the duplicate `text_encoding_stack` wrapper (`CMakeLists.txt`
excludes that wrapper under sanitizers). Its ordinary `text_encoding_text`
execution covers the same text fixture/modes with the inherited hard/soft 1 MiB
limit. This is a configuration difference, not a skipped or failed test.

| Current integrated profile | Common tests | Boundary tests | Stage 7 diagnostic children |
| --- | ---: | ---: | --- |
'''+ '\n'.join(rows)+'''

The separately preserved baseline replay also has 12/12 normal child exits:
**48/48 captured editor processes** across baseline and three current profiles,
empty child stderr, each with actual 1 MiB limits. This does not prove the old
uncaptured failure fixed. The original logging host reports **1,690 checks per
profile**; independently linked normal/O2 logging AOT probes report **828 each**.
The original text ownership host reports **17,834 checks per profile**.
[Machine-readable campaign verification](../../experiments/integration-qualification/evidence/qualification-results.json).

All execution commands use actual soft **and hard'''
s=s.replace('Final receipts pending. All execution commands use actual soft **and hard',new)
s=s.replace('The common `advanced_source` results qualify the repaired semantic test; they do','All three current `advanced_source` tests pass. Each reports acquisition ordinal\n**2** and first-body ordinal **3** in both normal/O2 execution, selected by meaning\nrather than new fixed constants. Those results qualify the semantic test; they do')
s=s.replace('Final recommendation and open-issue severity pending final local receipts.','''**Final recommendation: RETAIN for Golden Wrench/Boss review.** Preserve the
accepted cleanup, snapshots, compact success, checked roots, Runtime metadata
reuse and analysis-backed records. Keep the Frame cache removed and automatic
fusion disabled. No new source repair or experimental cleanup was necessary.
The timing/RSS regressions are accepted engineering tradeoffs for retention,
subject to Boss's disposition; a microbenchmark alone supplies no release gate.

[Final preservation verification](../../experiments/integration-qualification/evidence/preservation-verification.json)
checks the unchanged 180 implementation files, all **2,641 original internal
artifacts**, all **769 protected logging artifacts**, root README, existing SIMD
note and Git HEAD/index. New outputs and documentation are indexed by the
[qualification manifest](../../experiments/integration-qualification/evidence/artifact-manifest.json).
[Reproduction and exact command receipts](../../experiments/integration-qualification/README.md)
identify the fixed campaign and preserved helper corrections.''')
p.write_text(s)
# The link verifier requires the manifest target to exist before filling it.
manifest=h.E/'artifact-manifest.json';manifest.write_text('{}\n')
h.run('final-preservation-and-links',['python3',h.AREA/'verify.py'],small=False,check=True)
# Current target recipes and native script receipts remain available under their original unique run paths.
start=(h.E/'initial-identity.json').stat().st_mtime;native_receipts={}
for q in (h.ROOT/'docs').rglob('*'):
 if q.is_file() and 'evidence' in q.parts and q.stat().st_mtime>=start:
  native_receipts[str(q.relative_to(h.ROOT))]={'bytes':q.stat().st_size,'sha256':h.digest(q)}
(h.E/'native-test-receipt-index.json').write_text(json.dumps(native_receipts,indent=2))
# Hash completed new evidence only. Exclude the currently open final driver outputs.
files={}
for q in h.AREA.rglob('*'):
 if q.is_file() and q!=manifest and not q.name.startswith('finish-driver'):
  files[str(q.relative_to(h.AREA))]={'bytes':q.stat().st_size,'sha256':h.digest(q)}
files['@report']={'path':str(p.relative_to(h.ROOT)),'sha256':h.digest(p)}
manifest.write_text(json.dumps(files,indent=2))
print(json.dumps({'complete':True,'recommendation':'RETAIN for review','CTest_executions':sum(v['tests'] for x in result['profiles'].values() for v in x.values()),'editor_processes':48,'new_artifacts':len(files),'native_test_receipts':len(native_receipts),'report':str(p)},indent=2),flush=True)
