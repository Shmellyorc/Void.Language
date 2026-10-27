"""Retained, one-shot baseline reproduction and reduction. Not an acceptance test."""
from pathlib import Path
import sys, os, json, resource
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tests/text_encoding'))
from crash import run, preserve
os.environ['VOID_CRASH_EVIDENCE']=str(ROOT/'docs/json/evidence/implementation/incidents')
OUT=ROOT/'docs/json/evidence/implementation'

def stack():
    resource.setrlimit(resource.RLIMIT_STACK,(1048576,1048576))
    assert resource.getrlimit(resource.RLIMIT_STACK)==(1048576,1048576)

records=[]
for profile in ['debug','release']:
    compiler=ROOT/f'build/json-baseline/voidc-{profile}'
    for source in sorted((ROOT/'tests/json').glob('*.void')):
        for mode in ['check','run','o2']:
            command=[str(compiler),'run' if mode=='o2' else mode,str(source)]
            if mode=='o2': command+=['--opt']
            r=run(command,cwd=ROOT,capture_output=True,timeout=90,preexec_fn=stack,source_inputs=[source],provenance_inputs=[Path(__file__)])
            if b'VOID IR:' in r.stderr: preserve(command,ROOT,r,[source],[Path(__file__)])
            records.append({'profile':profile,'source':source.name,'mode':mode,'command':command,'status':r.returncode,'stdout':r.stdout.decode(),'stderr':r.stderr.decode(),'stack_bytes':1048576})
            (OUT/'readonly-baseline.json').write_text(json.dumps(records,indent=2)+'\n')
            print(profile,source.name,mode,r.returncode,r.stderr.decode()[:220],flush=True)
