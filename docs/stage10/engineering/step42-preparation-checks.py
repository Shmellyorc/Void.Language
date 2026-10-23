import datetime,hashlib,json,re,shlex,subprocess,sys
from pathlib import Path
base=Path('docs/stage10/engineering')
def sha(p):
    with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
checks=[]
def record(name,ok,**details):
    checks.append(dict(check=name,passed=bool(ok),**details))
    assert ok,checks[-1]
r=json.loads((base/'step41-results.json').read_text())
freeze=json.loads((base/'step41-full-source-freeze.json').read_text())
bad=[f for f,s in freeze.items() if not Path(f).is_file() or sha(f)!=s]
record('Accepted build/test/editor inputs unchanged',not bad,entries=len(freeze),mismatches=bad)
bad=[f for f,s in r['artifact_hashes'].items() if not (base/f).is_file() or sha(base/f)!=s]
record('All Step41 evidence artifacts unchanged',not bad,entries=len(r['artifact_hashes']),mismatches=bad)
a=json.loads((base/'step41-after-stack-artifact-checkpoint.json').read_text())
bad=[];n=0
for cfg,items in a['acceptance_artifact_sha256'].items():
    b=Path(r['configurations'][cfg]['build_directory'])
    for f,s in items.items():
        n+=1
        if not (b/f).is_file() or sha(b/f)!=s:bad.append(str(b/f))
record('Accepted Linux binary/archive/object identities unchanged',not bad,entries=n,mismatches=bad)
m=json.loads((base/'step41-source-hashes.json').read_text())
bad=[f for f,s in m.items() if not Path(f).is_file() or sha(f)!=s]
expected=['docs/WORK_LOG.md','docs/stage10/IMPLEMENTATION.md','docs/stage10/TEST_PLAN.md']
record('Historical source/document manifest differences limited to current authorized documentation',set(bad)==set(expected),entries=len(m),expected_documentation_changes=bad,historical_manifest_rewritten=False)
i=json.loads((base/'step42-inventory.json').read_text()); old=json.loads((base/'step41-acceptance-inventory.json').read_text())
win=i['windows']['projected_names'];mac=i['macos']['projected_names']; excluded=i['windows']['POSIX_only_registrations_absent']
record('Projected native inventory exactly preserves platform-eligible suites',len(win)==194 and len(set(win))==194 and mac==old['all_registered'] and set(mac)-set(win)==set(excluded) and len(mac)==199,windows_projected=194,macos_projected=199,native_observed=False)
doc=(base/'step42-native-commands.md').read_text()
text_blocks=re.findall(r'```text\n(.*?)\n```',doc,re.S)
shared=[line for block in text_blocks for line in block.splitlines() if line.startswith('python -c ')]; assert len(shared)==2
for line in shared:compile(shlex.split(line)[2],'<runbook python>','exec')
args=shlex.split(shared[0]);args[0]=sys.executable;args[-1]='/tmp/void-step42-source-receipt.json'
p=subprocess.run(args,capture_output=True,text=True)
record('Portable source receipt snippet compiles and verifies current Linux inputs; binary receipt snippet compiles',p.returncode==0,returncode=p.returncode,stderr=p.stderr,native_command_execution=False)
blocks=re.findall(r'```bash\n(.*?)\n```',doc,re.S);assert len(blocks)==1
p=subprocess.run(['bash','-n'],input=blocks[0],capture_output=True,text=True)
record('macOS bash command block syntax',p.returncode==0,returncode=p.returncode,stderr=p.stderr,macos_execution=False,powershell_syntax_execution='UNVERIFIED: PowerShell unavailable')
record('Root README and Step41 result identities unchanged',sha('README.md')==r['root_README_sha256'] and sha(base/'step41-results.json')=='e7eb5375aea4eb705ac275aa6043fb9a0bd3c0336cd71ef39869c8750db9b688',root_README_sha256=sha('README.md'),step41_results_sha256=sha(base/'step41-results.json'))
result={'status':'PASS: local preparation checks only','utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'command':'python3 docs/stage10/engineering/step42-preparation-checks.py (preparation checks, not native tests)','checks':checks,'passed':len(checks),'failed':0,'native_tests_executed':0}
(base/'step42-preparation-checks.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
