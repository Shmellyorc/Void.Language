"""G83 content equality and unchanged data-only key contracts on 1 MiB stacks."""
from pathlib import Path
import json,os,subprocess,sys,tempfile
compiler,evidence=(Path(p).resolve() for p in sys.argv[1:])
root=Path(__file__).resolve().parents[2];records=[]
inputs={str(p.relative_to(Path(__file__).parent)):p.read_text() for p in Path(__file__).parent.rglob('*.void')}
def stack():
    import resource
    resource.setrlimit(resource.RLIMIT_STACK,(1048576,1048576))
def run(args,status=0,output=None,error=None):
    command=list(map(str,args));r=subprocess.run(command,capture_output=True,text=True,timeout=90,preexec_fn=stack if os.name=='posix' else None)
    row=dict(command=command,expected=status,actual=r.returncode,stdout=r.stdout,stderr=r.stderr,stack_bytes=1048576 if os.name=='posix' else None)
    records.append(row);evidence.parent.mkdir(parents=True,exist_ok=True);evidence.write_text(json.dumps(dict(schema=1,inputs=inputs,records=records),indent=2)+'\n')
    assert r.returncode==status,row
    if output is not None:assert r.stdout==output,row
    if error is not None:assert error.lower() in r.stderr.lower(),row
    assert '$collection_equal' not in r.stderr and '$equality$' not in r.stderr and '/@pkg/' not in r.stderr,row
negatives={
 'plain_value':('value struct item:\n    n: i32\nfunc main():\n    var a = [item()]\n    print(a == a)\n','Value struct equality'),
 'plain_value_dict':('value struct item:\n    n: i32\nfunc main():\n    var a = {1: item()}\n    print(a == a)\n','Value struct equality'),
 'wrong_element':('func main():\n    var a = [1]\n    var b = ["one"]\n    print(a == b)\n','matching collection and element types'),
 'wrong_kind':('func main():\n    var a: [i32, 1] = [1]\n    var b = [1]\n    print(a == b)\n','matching collection and element types'),
 'recursive_shape':('enum tree:\n    branch(children: [tree])\n    leaf(value: i32)\nfunc main():\n    var a: [tree] = []\n    print(a == a)\n','non-recursive structural element shape'),
 'list_key':('func main():\n    var d: {[i32]: i32}\n','dictionary key'),
 'fixed_key':('func main():\n    var d: {[i32, 1]: i32}\n','dictionary key'),
 'map_key':('func main():\n    var d: {{i32: i32}: i32}\n','dictionary key'),
 'float_key':('func main():\n    var d: {f64: i32}\n','dictionary key'),
 'custom_key':('value struct key:\n    value: i32\n    operator ==(const left: key, const right: key) -> bool:\n        return true\nfunc main():\n    var d: {key: i32}\n','custom operator =='),
 'nested_custom_key':('value struct key:\n    value: i32\n    operator ==(const left: key, const right: key) -> bool:\n        return true\nvalue struct outer:\n    child: key\nfunc main():\n    var d: {outer: i32}\n','custom operator =='),
 'managed_custom_key':('struct key:\n    value: i32\n    operator ==(const left: key, const right: key) -> bool:\n        return true\nfunc main():\n    var d: {key?: i32}\n','custom operator =='),
 'constant_custom':('value struct item:\n    n: i32\n    operator ==(const left: item, const right: item) -> bool:\n        return true\nconst a = [item()] == [item()]\nfunc main():\n    print(a)\n','custom equality operator'),
 'constant_managed':('struct cell:\n    n: i32\nconst a = [cell()] == [cell()]\nfunc main():\n    print(a)\n','compile-time expression'),
}
positive={
 'constant_nullable':('const a: [i32]? = null\nconst b: [i32]? = []\nconst same = a == a\nconst different = a != b\nconst present = b == b\nfunc main():\n    print(same, different, present)\n','true true true\n'),
 'nullable_fixed':('func main():\n    var a: [i8, 2]? = [1, 2]\n    var b: [i8, 2]? = null\n    print(a == [1, 2], a != b, b == null)\n','true true true\n'),
 'constant_null_identities':('struct cell:\n    n: i32\nconst a: [cell?] = [null]\nconst b: [cell?] = [null]\nconst maps: {cell?: i32} = {null: 1}\nconst result = a == b\nconst same_maps = maps == maps\nfunc main():\n    print(result, same_maps)\n','true true\n'),
 'empty_nested':('func main():\n    var a: [i32, 0] = []\n    var b: [i32, 0] = []\n    var c: [[i32]] = [[], [1]]\n    var d: [[i32]] = [[], [1]]\n    print(a == b, c == d, c != [[], [2]], [1] != [1, 2])\n','true true true true\n'),
 'nullable_elements':('func main():\n    var a: [string?] = [null, "", "a"]\n    var b: [string?] = [null, "", "a"]\n    var c: [bool?] = [null, false, true]\n    var d: [bool?] = [null, false, true]\n    print(a == b, c == d, a != ["", "", "a"], c != [false, false, true])\n','true true true true\n'),
 'generic':('func same<t>(const left: t, const right: t) -> bool:\n    return left == right\nfunc main():\n    print(same([[1], [2]], [[1], [2]]), same({1: ["a"]}, {1: ["a"]}))\n','true true\n'),
 'non_reflexive_float':('func nan(value: f64) -> f64:\n    return value / value\nfunc main():\n    var a = [nan(0)]\n    var b = {1: nan(0)}\n    print(a == a, a != a, b == b, [-0.0] == [0.0])\n','false true false true\n'),
 'callable':('func work() -> i32:\n    return 7\nfunc other() -> i32:\n    return 7\nfunc main():\n    var a: [func() -> i32] = [work]\n    var b: [func() -> i32] = [work]\n    var c: [func() -> i32] = [other]\n    print(a == b, a != c)\n','true true\n'),
 'constants':('value struct block:\n    data: [i32, 4]\nconst a = block()\nconst b = block()\nconst fixed_same = a.data == b.data\nconst arrays = [[1], [2]] == [[1], [2]]\nenum choice:\n    data(value: [i32])\nconst enums = [choice.data([1])] == [choice.data([1])]\nconst maps = {"a\\0雪": [1], "b": [2], "b": [3]} == {"b": [3], "a\\0雪": [1]}\nfunc main():\n    print(fixed_same, arrays, enums, maps)\n','true true true true\n'),
 'key_types':('func main():\n'+''.join(f'    var d{i}: {{{t}: i32}} = {{1: 7}}\n    if d{i}[1] != 7:\n        print("wrong")\n' for i,t in enumerate(['i8','i16','i32','i64','u8','u16','u32','u64']))+'    print("all integer keys")\n','all integer keys\n'),
 'custom_order':('var calls = 0\nvalue struct item:\n    value: i32\n    operator ==(const left: item, const right: item) -> bool:\n        calls = calls * 10 + left.value\n        return left.value == right.value\nfunc main():\n    var a = {1: item(value = 1), 2: item(value = 2)}\n    var b = {2: item(value = 2), 1: item(value = 1)}\n    print(a == b, calls)\n    calls = 0\n    print([item(value = 1), item(value = 2)] != [item(value = 9), item(value = 2)], calls)\n','true 12\ntrue 1\n'),
}
# Force bucket collisions in the current private FNV key schema without
# assuming collision implies equality or exposing a public hash API.
collision_keys=[]
for value in range(100000):
    h=14695981039346656037
    for byte in value.to_bytes(4,'little'):h=((h^byte)*1099511628211)&((1<<64)-1)
    if h&255==0:collision_keys.append(value)
    if len(collision_keys)==40:break
assert len(collision_keys)==40
collision_source='func main() -> i32:\n    var keys: [i32] = ['+', '.join(map(str,collision_keys))+']\n    var a: {i32: i32} = {}\n    var b: {i32: i32} = {}\n    for key in keys:\n        a[key] = key + 1\n    for key in reverse keys:\n        b[key] = key + 1\n    if a != b:\n        return 1\n    for key in keys:\n        if a[key] != key + 1:\n            return 2\n    a.remove(keys[0])\n    if a.contains(keys[0]):\n        return 3\n    a[keys[0]] = keys[0] + 1\n    print(a == b, a.count)\n    return 0\n'
positive['bucket_collisions']=(collision_source,'true 40\n')
mutation='var shared: [item] = []\nvalue struct item:\n    value: i32\n    operator ==(const left: item, const right: item) -> bool:\n        shared.clear()\n        return true\nfunc main():\n    shared = [item()]\n    var a = [item()]\n    print(a == shared)\n'
with tempfile.TemporaryDirectory(prefix='void-g83-') as directory:
    tmp=Path(directory)
    fixtures=[(Path(__file__).with_name('main.void'),'collection equality preserved\n'),(Path(__file__).with_name('keys.void'),'dictionary key contracts preserved\n'),(Path(__file__).with_name('private.void'),'true\n'),(Path(__file__).parent/'imported/main.void','true\n')]
    for name,(source,output) in positive.items():
        file=tmp/(name+'.void');file.write_text(source);inputs[file.name]=source;fixtures.append((file,output))
    for file,output in fixtures:
        run([compiler,'check',file])
        for i,flags in enumerate((['--no-opt'],[])):
            run([compiler,'run',file,'--no-cache',*flags],output=output)
            image=tmp/(file.stem+str(i));run([compiler,'build',file,'-o',image,*flags]);run([image],output=output)
    private_escape = Path(__file__).with_name('private.void').read_text().replace('print(secret.same(a, a))', 'print(secret.same(a, a), a == a)')
    file=tmp/'private_escape.void';file.write_text(private_escape);inputs[file.name]=private_escape
    run([compiler,'check',file],1,error='not accessible')
    for name,(source,error) in negatives.items():
        file=tmp/(name+'.void');file.write_text(source);inputs[file.name]=source;run([compiler,'check',file],1,error=error)
    file=tmp/'mutation.void';file.write_text(mutation);inputs[file.name]=mutation
    for i,flags in enumerate((['--no-opt'],[])):
        run([compiler,'run',file,'--no-cache',*flags],1,error='modified')
        image=tmp/('mutation'+str(i));run([compiler,'build',file,'-o',image,*flags]);run([image],1,error='modified')
print(f'G83: {len(records)} source/application checks passed with original 1 MiB child stacks')
