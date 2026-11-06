"""Read-only original-evidence verification; write new qualification receipts only."""
from pathlib import Path
import hashlib,json,subprocess,re,sys,xml.etree.ElementTree as ET,platform
import qualify as h
def stream_hash(p):
 s=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1024*1024),b''):s.update(b)
 return s.hexdigest()
initial=json.loads((h.E/'initial-identity.json').read_text())
assert all(h.digest(h.ROOT/p)==v for p,v in initial['source_sha256'].items())
assert h.digest(h.ROOT/'README.md')==initial['root_readme']
assert h.digest(h.ROOT/'docs/VOIDC_AUTOMATIC_SIMD_ROADMAP.md')==initial['existing_simd_note']
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=h.ROOT,text=True).strip()==initial['head']
assert subprocess.check_output(['git','diff','--cached','--stat'],cwd=h.ROOT,text=True)==initial['index_diff']==''
assert subprocess.run(['git','diff','--check'],cwd=h.ROOT,capture_output=True).returncode==0
old=h.ROOT/'experiments/internal-performance';manifest=json.loads((old/'evidence/artifact-manifest.json').read_text());issues=[]
for name,record in manifest.items():
 p=old/name
 if not p.is_file() or p.stat().st_size!=record['bytes'] or stream_hash(p)!=record['sha256']:issues.append(name)
assert not issues,issues
# That manifest includes the refinement protection receipt. Revalidate its protected inputs too.
ref=h.ROOT/'experiments/log-integrated/refinement';prot=json.loads((ref/'evidence/artifact-manifest.json').read_text())
ref_issues=[]
for name,record in prot.items():
 p=(h.ROOT/'docs/log/INTEGRATED_LOGGING_REFINEMENT_RESULT.md') if name=='@report' else (h.ROOT/'experiments/log-integrated/candidate/tests/advanced/tests.cpp') if name=='@candidate-advanced-test' else ref/name
 if not p.is_file() or ('bytes' in record and p.stat().st_size!=record['bytes']) or stream_hash(p)!=record['sha256']:ref_issues.append(name)
assert not ref_issues,ref_issues
# Extract tracked production/test inventory, plus its new IR file.
paths=subprocess.check_output(['git','diff','--name-only'],cwd=h.ROOT,text=True).splitlines()
paths=[p for p in paths if p.startswith(('src/','include/','stdlib/','tests/'))]+['src/ir/local_cleanup.inc']
inventory={p:{'sha256':h.digest(h.ROOT/p),'kind':'fixture' if p.startswith('tests/') else 'production'} for p in paths}
(h.E/'source-inventory.json').write_text(json.dumps(inventory,indent=2))
# Validate changed doc links, including local headings.
docs=['docs/README.md','docs/DEVELOPMENT_STRATEGY.md','docs/WRENCH_CLANKER.md','docs/compiler-health/ISSUES.md','docs/compiler-health/INTEGRATED_PERFORMANCE_QUALIFICATION.md','docs/log/HANDOFF.md','docs/log/PERFORMANCE_REPAIR.md','docs/log/LOG_PERF_DECISION.md','docs/release/PREVIEW_BLOCKERS.md']
# Check only the newly written blocks in historical docs, avoiding inherited broken links.
checks=[]
for name in docs:
 p=h.ROOT/name;s=p.read_text()
 if name not in ['docs/DEVELOPMENT_STRATEGY.md','docs/README.md','docs/compiler-health/INTEGRATED_PERFORMANCE_QUALIFICATION.md']:
  s=s.split('\n## ',2)[1] if '\n## ' in s else s
 elif name=='docs/DEVELOPMENT_STRATEGY.md':s=s.split('## LLVM 23.1.4 migration')[0]
 elif name=='docs/README.md':s=s.split('## MIT licensing implementation')[0]
 for link in re.findall(r'\]\(([^)]+)\)',s):
  if link.startswith(('http:','https:')):continue
  path,_,anchor=link.partition('#');target=p.parent/path
  assert target.exists(),(name,link)
  if anchor:
   headings=re.findall(r'^#+ (.+)$',target.read_text(),re.M)
   slugs=[re.sub(r'[^\w\- ]','',x.lower()).replace(' ','-') for x in headings]
   assert anchor in slugs,(name,link,slugs[:8])
  checks.append([name,link])
(h.E/'documentation-link-check.json').write_text(json.dumps(checks,indent=2))
summary=dict(source_files_verified=len(initial['source_sha256']),production_files=sum(v['kind']=='production' for v in inventory.values()),fixture_files=sum(v['kind']=='fixture' for v in inventory.values()),original_internal_artifacts_verified=len(manifest),original_internal_issues=issues,protected_refinement_artifacts_verified=len(prot),refinement_issues=ref_issues,README_unchanged=True,existing_SIMD_note_unchanged=True,HEAD_unchanged=True,index_unchanged=True,host=platform.platform(),local_doc_links_checked=len(checks))
(h.E/'preservation-verification.json').write_text(json.dumps(summary,indent=2));print(json.dumps(summary,indent=2))
