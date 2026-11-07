"""Native portable provider execution with preserved command receipts."""
from pathlib import Path
import hashlib,json,os,platform,subprocess,sys,tempfile,zipfile
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools'))
from test_platform import small_stack
exe=Path(sys.argv[1]).resolve()
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='VOID portable 雪 # % ') as directory:
 work=Path(directory).resolve()
 with zipfile.ZipFile(work/'package.zip','w',compression=zipfile.ZIP_DEFLATED) as bundle:bundle.writestr('data',b'abc')
 with zipfile.ZipFile(work/'hostile.zip','w') as bundle:bundle.writestr('../escape',b'bad')
 command=[str(exe),str(work),str(exe)]
 result=subprocess.run(command,cwd=work,capture_output=True,preexec_fn=small_stack if os.name!='nt' else None,timeout=120)
 record={'platform':platform.platform(),'command':command,'status':result.returncode,'stdout':result.stdout.decode(errors='replace'),'stderr':result.stderr.decode(errors='replace'),'sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'hard_main_stack_bytes':1048576 if os.name!='nt' else None}
 out=Path(os.environ.get('VOID_HOST_EVIDENCE_DIR',str(root/'docs/voidup/evidence/portability')))/('portable-'+exe.parent.name+'.json');out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(record,indent=2)+'\n')
 assert result.returncode==0,record
 print(record['stdout'],end='')
