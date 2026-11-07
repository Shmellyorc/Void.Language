from pathlib import Path
import hashlib,json,os,subprocess,sys,tempfile,zipfile
import support as h
exe=Path(sys.argv[1]).resolve();child=Path(sys.argv[2]).resolve()
disk='--disk' in sys.argv
with tempfile.TemporaryDirectory(prefix='VOID host native 雪 # % ',dir=Path.home() if disk else None) as directory:
 work=Path(directory);package=work/'package.zip'
 with zipfile.ZipFile(package,'w') as z:z.writestr('data.bin',b'package')
 r=h.run([exe,work,child,package],work,small=True,timeout=180)
 print(r.stdout.decode(),end='')
 receipt={'executable':str(exe),'sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'stack_bytes':1048576,'filesystem':subprocess.check_output(['stat','-f','-c','%T',work],text=True).strip(),'stdout':r.stdout.decode(),'stderr':r.stderr.decode()}
 (Path(os.environ.get('VOID_HOST_EVIDENCE_DIR',str(h.ROOT/'docs/voidup/evidence')))/f'{exe.parent.name}-native{"-disk" if disk else ""}.json').write_text(json.dumps(receipt,indent=2)+'\n')
