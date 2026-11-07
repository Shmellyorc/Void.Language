"""Credential-free native CLI/entry smoke; Unix also exercises private control IO.

Windows filesystem/installation acceptance is deliberately not inferred from a
CLI check. The separately registered native provider/source tests still run.
"""
from pathlib import Path
import hashlib,json,os,subprocess,sys,tempfile
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from test_platform import small_stack,report_stack
sys.path.insert(0,str(ROOT/'tests/text_encoding'))
from crash import run as preserving_run, preserve

application=Path(sys.argv[1]).resolve()
evidence=Path(sys.argv[2]).resolve();evidence.mkdir(parents=True,exist_ok=True)
os.environ.setdefault('VOID_CRASH_EVIDENCE',str(evidence/'crashes'))
records=[]
def run(args,status=0):
 command=[str(application),*map(str,args)]
 result=preserving_run(command,capture_output=True,timeout=180,
     preexec_fn=small_stack if os.name!='nt' else None,source_inputs=(ROOT/'apps/voidup',))
 records.append(dict(command=command,status=result.returncode,expected=status,
                     stdout=result.stdout.decode('utf-8'),stderr=result.stderr.decode('utf-8')))
 (evidence/'smoke.json').write_text(json.dumps(dict(schema=1,application=str(application),
     sha256=hashlib.sha256(application.read_bytes()).hexdigest(),records=records,
     hard_stack_bytes=1048576 if os.name!='nt' else None,private_credentials=False,
     private_filesystem_exercised=os.name!='nt',
     native_installation_qualified=False),indent=2)+'\n',encoding='utf-8')
 if result.returncode!=status:
  preserve(command,ROOT,result,(ROOT/'apps/voidup',),(Path(__file__),))
  raise AssertionError(records[-1])
 if os.name!='nt':report_stack(result.stderr)
 return result

def diagnostic(result):
 # The test launcher emits a separately validated stack receipt before exec.
 # Keep its raw bytes above; decode only the application's diagnostic here.
 return json.loads(b'\n'.join(line for line in result.stderr.splitlines() if not line.startswith(b'VOID_TEST_STACK ')))

assert b'private' in run(['--version']).stdout
assert b'rollback' in run(['--help']).stdout
failure=run(['--invalid-雪'],2);assert diagnostic(failure)['operation']=='arguments'
run(['run','default','voidc','missing-separator'],2)
if os.name!='nt':
 with tempfile.TemporaryDirectory(prefix='VOIDUP CI 雪 # % ') as temporary:
  root=Path(temporary).resolve()/'private'
  run(['--root',root,'doctor']);run(['--root',root,'list'])
  state=json.loads((root/'control.json').read_bytes());assert state['serial']==0 and state['current']==''
  run(['--root',root,'doctor'])
  failure=run(['--root',root,'which','voidc'],1);assert diagnostic(failure)['operation']=='which'
  assert not list((root/'toolchains').iterdir())
print('VOIDUP native smoke passed; no signing keys, catalog or network used; installation qualification remains separate')
