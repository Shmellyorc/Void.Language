"""Re-emit on the 1 MiB stack, compare linked bytes, then run native probes."""
from pathlib import Path
import os,subprocess,sys,tempfile
repo=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(repo/'tools'))
from test_output import output_directory
emitter,native,source,linked=map(lambda p:Path(p).resolve(),sys.argv[1:5]);mode,operation=sys.argv[5:]
def limits():
    import resource
    resource.setrlimit(resource.RLIMIT_STACK,(1048576,1048576));resource.setrlimit(resource.RLIMIT_CORE,(0,0))
def run(args):
    result=subprocess.run(list(map(str,args)),capture_output=True,text=True,timeout=90,
        preexec_fn=limits if os.name=='posix' else None)
    assert result.returncode==0,(args,result.returncode,result.stdout,result.stderr)
    assert not any(s in result.stderr for s in ('AddressSanitizer','runtime error:','Assertion')),result
    return result.stdout
with tempfile.TemporaryDirectory(prefix='async-aot-',dir=output_directory()) as directory:
    object_file=Path(directory)/'source.o'
    run([emitter,operation,source,object_file,mode])
    assert object_file.read_bytes()==linked.read_bytes(),'limited-stack AOT object differs from linked object'
    text=run([native,'7'] if operation=='--emit-async' else [native])
    if operation=='--emit-async':assert text=='7\n',text
print('G86 independent AOT: limited-stack emission and native execution passed')
