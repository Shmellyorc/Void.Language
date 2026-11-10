"""Test-owned ELF image with a corrupt application record must fail before eager output."""
from pathlib import Path
import hashlib, json, struct, subprocess, sys, tempfile

compiler=Path(sys.argv[1]).resolve()
source="""use os
func prepare() -> result<string, os.error>:
    print("eager entered")
    return os.data_dir()
var initialized = prepare()
func main():
    if initialized is Ok(var path):
        print("main entered", path)
"""
policy="""schema=1
[project]
id='org.test.startup'
name='Startup'
version='1.0.0'
kind='application'
[application]
id='org.test.admission'
name='Admission'
version='2.0.0'
[build.release]
debug_info='none'
"""
fields=['org.test.startup','Startup','1.0.0','org.test.admission','Admission','2.0.0']
encoded=struct.pack('<I',1)+b''.join(struct.pack('<I',len(f))+f.encode() for f in fields)
rows=[]
try:
    with tempfile.TemporaryDirectory(prefix='VOID S4 malformed AOT ') as tmp:
        folder=Path(tmp)
        (folder/'void.project.toml').write_text(policy)
        (folder/'startup.void').write_text(source)
        binary=folder/'program'
        def run(argv,status):
            result=subprocess.run(list(map(str,argv)),cwd=folder,capture_output=True,text=True,timeout=120)
            rows.append({'argv':list(map(str,argv)),'status':result.returncode,'stdout':result.stdout,'stderr':result.stderr})
            assert result.returncode==status,rows[-1]
            return result
        run([compiler,'build','--project',folder,'-o',binary],0)
        unmodified=run([binary],0)
        assert unmodified.stdout.startswith('eager entered\nmain entered '),unmodified
        raw=binary.read_bytes()
        assert raw.count(encoded)==1
        at=raw.index(encoded)
        changed=raw[:at]+struct.pack('<I',2)+raw[at+4:]
        binary.write_bytes(changed)
        failure=run([binary],1)
        assert not failure.stdout and 'Invalid compiled application metadata.' in failure.stderr,failure
        rows.append({'record_offset':at,'old_sha256':hashlib.sha256(raw).hexdigest(),
            'rejected_sha256':hashlib.sha256(changed).hexdigest()})
    print('AOT startup: corrupt revision rejected before observable eager initialization.')
finally:Path(sys.argv[2]).write_text(json.dumps(rows,indent=2)+'\n')
