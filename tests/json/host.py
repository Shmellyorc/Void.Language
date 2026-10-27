from pathlib import Path
import sys, json
import harness as h
work=Path(sys.argv[3]).resolve();work.mkdir(parents=True,exist_ok=True)
executable_hash=h.digest(h.compiler)
library_hashes={str(p):h.digest(p) for p in (h.ROOT/'stdlib').rglob('*.void')}
r=h.preserving_run([h.compiler,work],cwd=work,capture_output=True,timeout=1500,preexec_fn=h.stack,
    provenance_inputs=list(Path(__file__).parent.glob('*.*')))
if b'VOID IR:' in r.stderr:h.preserve([h.compiler,work],work,r,list(work.glob('*.void')),list(Path(__file__).parent.glob('*.*')))
h.receipt.write_text(json.dumps({'profile':h.profile,'status':r.returncode,'stdout':r.stdout.decode(errors='replace'),'stderr':r.stderr.decode(errors='replace'),'executable_sha256':executable_hash,'executable_stable':executable_hash==h.digest(h.compiler),'generated_snapshots':{str(p):h.digest(p) for p in work.glob('*.void')},'stdlib_hashes':library_hashes,'stdlib_stable':all(h.digest(p)==value for p,value in library_hashes.items()),'stack_bytes_verified_in_child_preexec':1048576},indent=2)+'\n')
assert executable_hash==h.digest(h.compiler),'Host executable changed during verification'
assert all(h.digest(p)==value for p,value in library_hashes.items()),'Loaded standard library changed during verification'
assert r.returncode==0,(r.returncode,r.stdout,r.stderr)
print(r.stdout.decode(),end='')
