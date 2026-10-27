"""JSON tests reuse the established preserving process boundary."""
from pathlib import Path
import sys, os, json, hashlib, resource, time
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tests/text_encoding'))
from crash import run as preserving_run, preserve, crashed
os.environ.setdefault('VOID_CRASH_EVIDENCE',str(ROOT/'docs/json/evidence/implementation/incidents'))
compiler=Path(sys.argv[1]).resolve()
profile=sys.argv[2] if len(sys.argv)>2 else 'adhoc'
receipt=ROOT/'docs/json/evidence/implementation'/f'{Path(sys.argv[0]).stem}-{profile}-{time.time_ns()}.json'
records=[]

def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def stack():
    resource.setrlimit(resource.RLIMIT_STACK,(1048576,1048576))
    assert resource.getrlimit(resource.RLIMIT_STACK)==(1048576,1048576)

def run(args,source,expected=None,status=0,cwd=None,env=None,timeout=180):
    args=list(map(str,args));cwd=Path(cwd or ROOT)
    inputs={p.resolve() for p in (ROOT/'stdlib').rglob('*.void')}
    inputs.update(p.resolve() for p in Path(source).parent.glob('*.void'))
    before={str(p):digest(p) for p in inputs}
    r=preserving_run(args,cwd=cwd,capture_output=True,timeout=timeout,preexec_fn=stack,env=env,
        source_inputs=[source],provenance_inputs=list(Path(__file__).parent.glob('*.py')))
    internal=b'VOID IR:' in r.stderr
    if internal or (r.returncode not in (0,1) and not crashed(r.returncode)):
        preserve(args,cwd,r,[source],list(Path(__file__).parent.glob('*.py')))
    after={str(p):digest(p) for p in inputs}
    records.append({'command':args,'cwd':str(cwd),'status':r.returncode,'stdout_hex':r.stdout.hex(),'stderr':r.stderr.decode(errors='replace'),'source':str(source),'source_sha256':digest(source),'input_hashes':before,'inputs_stable':before==after,'stack_bytes_verified_in_child_preexec':1048576})
    receipt.write_text(json.dumps({'profile':profile,'compiler':str(compiler),'compiler_sha256':digest(compiler),'records':records,'environment':{k:os.environ[k] for k in ['ASAN_OPTIONS','UBSAN_OPTIONS'] if k in os.environ}},indent=2)+'\n')
    assert r.returncode==status,(args,r.returncode,r.stdout,r.stderr)
    assert before==after,'Source changed during the process; retain this attempt and rerun against stable inputs.'
    assert not internal and b'warning' not in r.stderr.lower(),r.stderr
    if expected is not None:assert r.stdout==expected,(r.stdout,expected,r.stderr)
    return r

def literal(text):
    out='"'
    for ch in text:
        if ch=='"':out+='\\"'
        elif ch=='\\':out+='\\\\'
        elif ch=='{':out+='{{'
        elif ch=='}':out+='}}'
        elif ord(ch)<32 or ord(ch)==127:out+='\\u{'+format(ord(ch),'x')+'}'
        else:out+=ch
    return out+'"'
