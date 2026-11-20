"""G83 helper analysis must unwind between functions on the required 1 MiB stack."""
from pathlib import Path
import hashlib,json,os,re,subprocess,sys,tempfile

compiler,report=(Path(p).resolve() for p in sys.argv[1:])
inputs={};records=[]
def limits():
    import resource
    resource.setrlimit(resource.RLIMIT_STACK,(1048576,1048576))
    resource.setrlimit(resource.RLIMIT_CORE,(0,0))
def run(args,status=0,output=None,error=None):
    command=list(map(str,args))
    result=subprocess.run(command,capture_output=True,text=True,timeout=90,
                          preexec_fn=limits if os.name=='posix' else None)
    row=dict(command=command,expected=status,actual=result.returncode,
             stdout=result.stdout,stderr=result.stderr,
             stack_bytes=1048576 if os.name=='posix' else None)
    records.append(row);report.parent.mkdir(parents=True,exist_ok=True)
    report.write_text(json.dumps(dict(schema=1,compiler_sha256=hashlib.sha256(compiler.read_bytes()).hexdigest(),
                                    inputs=inputs,records=records),indent=2)+'\n')
    assert result.returncode==status,row
    if output is not None:assert result.stdout==output,row
    if error is not None:
        assert error.lower() in result.stderr.lower(),row
        assert re.search(r'\.void:\d+:\d+:',result.stderr),row
    assert not any(s in result.stderr for s in ('$equality$','$collection_equal','/@pkg/','AddressSanitizer','runtime error:')),row

def nested(depth,leaf='i32'):
    return '['*depth+leaf+']'*depth
def empty(depth,comparison=True):
    # These are byte-for-byte the independent review's 8/24/48-level inputs.
    return f'func main():\n    var a: {nested(depth)} = []\n    print('+('a == a' if comparison else 'a.count')+')\n'
def mixed(depth):
    # Short source types reach the existing structural limit independently of
    # the parser's source-nesting guard: 63 enums + 62 inner lists + outer lists.
    declarations=''.join(f'enum e{i}:\n    data(value: '+('i32' if i==0 else f'[e{i-1}]')+')\n' for i in range(63))
    return declarations+f'func main():\n    var a: {nested(depth-125,"e62")} = []\n    print(a == a)\n'

with tempfile.TemporaryDirectory(prefix='void-g83-nesting-') as directory:
    tmp=Path(directory)
    def fixture(name,source):
        inputs[name+'.void']=source;file=tmp/(name+'.void');file.write_text(source);return file
    def execute(file,output):
        for index,flags in enumerate((['--no-opt'],[])):
            run([compiler,'run',file,'--no-cache',*flags],output=output)
            image=tmp/(file.stem+str(index))
            run([compiler,'build',file,'-o',image,*flags]);run([image],output=output)

    for depth in (8,23,24,25,47,48,49):
        file=fixture(f'depth_{depth}_equality',empty(depth));run([compiler,'check',file])
        control=fixture(f'depth_{depth}_control',empty(depth,False));run([compiler,'check',control])
        if depth in (8,24,48):
            execute(file,'true\n');execute(control,'0\n')
    for depth in (127,128):
        file=fixture(f'mixed_{depth}',mixed(depth));run([compiler,'check',file])
    run([compiler,'check',fixture('mixed_129',mixed(129))],1,error='Collection equality type nesting limit exceeded')
    run([compiler,'check',fixture('source_129',empty(129))],1,error='Source nesting limit exceeded')

    # Nonempty values exercise actual nested calls, !=, private operator authority,
    # and a concrete generic specialization after the bodies have been deferred.
    leaf='''value struct secret:
    value: i32
    private operator ==(const a: secret, const b: secret) -> bool:
        return a.value == b.value
'''
    typ=nested(24,'secret');value='['*24+'secret(value = 7)'+']'*24
    source=leaf+f'''static func secret.same<t>(const left: t, const right: t) -> bool:
    return left == right
func main():
    var a: {typ} = {value}
    print(secret.same(a, a))
'''
    file=fixture('deep_private_generic',source);run([compiler,'check',file]);execute(file,'true\n')
    escaped=source.replace('print(secret.same(a, a))','print(a == a)')
    run([compiler,'check',fixture('deep_private_escape',escaped)],1,error='not accessible')
    source=f'func main():\n    var a: {nested(24)} = '+nested(24,'1')+'\n    print(a == a, a != a)\n'
    file=fixture('deep_nonempty',source);run([compiler,'check',file]);execute(file,'true false\n')
    for kind in ('dictionary','fixed'):
        typ='i32';value='1'
        for _ in range(24):
            if kind=='dictionary':typ='{i32: '+typ+'}';value='{1: '+value+'}'
            else:typ='['+typ+', 1]';value='['+value+']'
        source=f'func main():\n    var a: {typ} = {value}\n    print(a == a, a != a)\n'
        file=fixture('deep_'+kind,source);run([compiler,'check',file]);execute(file,'true false\n')
    source=f'const a: {nested(24)} = []\nconst same = a == a\nvar b: {nested(24)} = []\nvar same_global = b == b\nfunc main():\n    print(same, same_global)\n'
    file=fixture('deep_constant_global',source);run([compiler,'check',file]);execute(file,'true true\n')

    # Empty constants can fold to true before helper bodies are visited. Their
    # unsupported leaves must still be diagnosed by the ordinary analysis pass.
    declaration='value struct item:\n    value: i32\n'
    for constant in (False,True):
        if constant:
            source=declaration+f'const a: {nested(24,"item")} = []\nconst same = a == a\nfunc main():\n    print(same)\n'
        else:
            source=declaration+f'func main():\n    var a: {nested(24,"item")} = []\n    print(a == a)\n'
        run([compiler,'check',fixture('unsupported_'+str(constant),source)],1,error='Value struct equality')
    custom=declaration+'    operator ==(const a: item, const b: item) -> bool:\n        return true\n'
    source=custom+f'const a: {nested(24,"item")} = []\nconst same = a == a\nfunc main():\n    print(same)\n'
    run([compiler,'check',fixture('constant_custom',source)],1,error='custom equality operator')
    source=f'func main():\n    var a: {nested(24)} = []\n    var b: {nested(24,"bool")} = []\n    print(a == b)\n'
    run([compiler,'check',fixture('mismatch',source)],1,error='matching collection and element types')
print(f'G83 nesting: {len(records)} checks passed with original 1 MiB child stacks')
