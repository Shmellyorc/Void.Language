"""G84: the ordinary constructor call returns result when init is fallible."""
from pathlib import Path
import hashlib,json,os,re,subprocess,sys,tempfile

compiler,report=(Path(p).resolve() for p in sys.argv[1:])
repo=Path(__file__).resolve().parents[2];records=[];inputs={}
def limits():
    import resource
    resource.setrlimit(resource.RLIMIT_STACK,(1048576,1048576))
    resource.setrlimit(resource.RLIMIT_CORE,(0,0))
def run(args,status=0,output=None,error=None):
    args=list(map(str,args))
    r=subprocess.run(args,capture_output=True,text=True,timeout=90,
                     preexec_fn=limits if os.name=='posix' else None)
    row=dict(command=args,expected=status,actual=r.returncode,stdout=r.stdout,stderr=r.stderr)
    records.append(row);report.parent.mkdir(parents=True,exist_ok=True)
    report.write_text(json.dumps(dict(schema=1,stack_bytes=1048576,
        compiler_sha256=hashlib.sha256(compiler.read_bytes()).hexdigest(),inputs=inputs,records=records),indent=2)+'\n')
    assert r.returncode==status,row
    if output is not None:assert r.stdout==output,row
    if error:assert error.lower() in r.stderr.lower() and re.search(r'\.void:\d+:\d+:',r.stderr),row
    assert not any(t in r.stderr for t in ('AddressSanitizer','runtime error:','construction::','$result:','/@pkg/','.void::')),row

TYPES='struct child:\n    value: i32\nstruct owner:\n    child: child\n'
INIT='func (o owner) init(fail: bool = false) -> result<bool, string>:\n'
NEGATIVE={
    'generic_result_construction':('value struct item:\n    value: i32\nfunc (i item) init() -> result<bool, string>:\n    return Ok(true)\nfunc default_of<t>(value: t) -> t:\n    return t()\nfunc main():\n    var value = default_of(item())\n','no source constructor'),
    'non_result':(TYPES+'func (o owner) init() -> i32:\n    return 7\n','initializer result must'),
    'generic_error_type':('enum reason<t>:\n    bad(value: t)\nstruct item:\n    value: i32\nfunc (i item) init() -> result<bool, reason<string>>:\n    return Err(7)\n','Type mismatch'),
    'wrong_success_type':(TYPES+'func (o owner) init() -> result<i32, string>:\n    return Ok(7)\n','initializer result must'),
    'wrong_error':(TYPES+INIT+'    return Err(7)\n','Type mismatch'),
    'empty_return':(TYPES+INIT+'    return\n','Type mismatch'),
    'missing_return':(TYPES+INIT+'    o.child = child()\n','Missing return'),
    'incomplete_success':(TYPES+INIT+'    if fail:\n        return Err("bad")\n    return Ok(true)\n','initialized field'),
    'incomplete_match':(TYPES+INIT+'    match fail:\n        true:\n            return Err("bad")\n        false:\n            pass\n    return Ok(true)\n','initialized field'),
    'read_unready':(TYPES+INIT+'    if fail:\n        return Err("bad")\n    print(o.child.value)\n    o.child = child()\n    return Ok(true)\n','before it is read'),
    'self_error':(TYPES+'func (o owner) init() -> result<bool, owner>:\n    return Err(o)\n','Construction views'),
    'alias_error':(TYPES+'func (o owner) init() -> result<bool, owner>:\n    var alias = o\n    return Err(alias)\n','Construction views'),
    'stored_self':(TYPES+'var saved: owner? = null\n'+INIT+'    saved = o\n    return Err("bad")\n','construction view'),
    'helper_unready':(TYPES+'func helper(value: owner):\n    print(value.child.value)\n'+INIT+'    helper(o)\n    return Err("bad")\n','initialized'),
    'closure_escape':(TYPES+INIT+'    o.child = child()\n    var escaped = func() -> owner:\n        return o\n    return Err("bad")\n','construction view'),
    'interface_escape':('interface readable:\n    func get() -> i32\n'+TYPES+'func (o owner) get() -> i32:\n    return o.child.value\n'+INIT+'    o.child = child()\n    var escaped: readable = o\n    return Err("bad")\n','construction view'),
    'identity_before_publication':(TYPES+INIT+'    if o == o:\n        return Err("bad")\n    return Err("other")\n','construction view'),
    'collection_escape':(TYPES+INIT+'    o.child = child()\n    var escaped: [owner] = [o]\n    return Err("bad")\n','construction view'),
    'value_self_error':('value struct item:\n    value: i32\nfunc (i item) init() -> result<bool, item>:\n    i.value = 9\n    return Err(i)\n','construction view'),
    'value_alias_error':('value struct item:\n    value: i32\nfunc (i item) init() -> result<bool, item>:\n    var alias = i\n    return Err(alias)\n','construction view'),
    'value_helper_escape':('value struct item:\n    value: i32\nfunc expose(value: item) -> item:\n    return value\nfunc (i item) init() -> result<bool, item>:\n    return Err(expose(i))\n','Unsafe construction helper'),
    'value_closure_escape':('value struct item:\n    value: i32\nfunc (i item) init() -> result<bool, string>:\n    var escape = func() -> item:\n        return i\n    return Err("bad")\n','construction view'),
    'wrong_propagation':(TYPES+'func step() -> result<i32, i32>:\n    return Err(1)\n'+INIT+'    var value = step()?\n    return Err("bad")\n','matching error'),
    'result_is_not_object':(TYPES+INIT+'    o.child = child()\n    return Ok(true)\nfunc main():\n    var value: owner = owner()\n','Type mismatch'),
    'implicit_value':('value struct item:\n    value: i32\nfunc (i item) init() -> result<bool, string>:\n    return Ok(true)\nfunc main():\n    var value: item\n','explicit constructor'),
    'implicit_child':('value struct item:\n    value: i32\nfunc (i item) init() -> result<bool, string>:\n    return Ok(true)\nvalue struct wrapper:\n    child: item\nfunc main():\n    var value = wrapper()\n','fallible child'),
    'direct_init':(TYPES+INIT+'    o.child = child()\n    return Ok(true)\nfunc consume(value: owner):\n    value.init()\n','Lifecycle'),
    'duplicate_init':(TYPES+INIT+'    return Err("bad")\n'+INIT+'    return Err("bad")\n','Duplicate'),
    'missing_argument':(TYPES+'func (o owner) init(value: i32) -> result<bool, string>:\n    return Err("bad")\nfunc main():\n    var value = owner()\n','Missing required'),
    'bad_label':(TYPES+INIT+'    return Err("bad")\nfunc main():\n    var value = owner(unknown = true)\n','Unknown initializer'),
    'default_call':(TYPES+INIT+'    return Err("bad")\nfunc consume(value: result<owner, string> = owner()):\n    pass\n','Compile-time'),
    'unhandled_base':('struct base:\n    value: i32\nfunc (b base) init() -> result<bool, string>:\n    return Err("base")\nstruct derived extends base:\n    value2: i32\nfunc (d derived) init() -> result<bool, string>:\n    super.init()\n    return Ok(true)\n','super.init(...)?'),
    'base_error_mismatch':('struct base:\n    value: i32\nfunc (b base) init() -> result<bool, string>:\n    return Err("base")\nstruct derived extends base:\n    value2: i32\nfunc (d derived) init() -> result<bool, i32>:\n    super.init()?\n    return Ok(true)\n','matching error'),
    'infallible_derived':('struct base:\n    value: i32\nfunc (b base) init() -> result<bool, string>:\n    return Err("base")\nstruct derived extends base:\n    value2: i32\nfunc (d derived) init():\n    super.init()?\n','fallible derived'),
}
MATCH='''struct child:
    value: i32
struct owner:
    child: child
func (o owner) init(fail: bool) -> result<bool, string>:
    match fail:
        true:
            return Err("match")
        false:
            o.child = child(value = 8)
    return Ok(false)
func get(fail: bool) -> result<i32, string>:
    var o = owner(fail)?
    return Ok(o.child.value)
func main():
    print(get(true) is Err("match"), get(false) is Ok(8))
'''
NESTED='''value struct item:
    value: i32
func (i item) init(value: i32) -> result<bool, string>:
    if value < 0:
        return Err("item")
    i.value = value
    return Ok(true)
value struct wrapper:
    child: item
func build(value: i32) -> result<wrapper, string>:
    return Ok(wrapper(child = item(value)?))
func main():
    print(build(-1) is Err("item"))
    match build(9):
        Ok(var w):
            print(w.child.value)
        Err(var error):
            print(error)
'''
NESTED_MANAGED='''struct child:
    value: i32
func (c child) init(value: i32) -> result<bool, string>:
    if value < 0:
        return Err("child")
    c.value = value
    return Ok(true)
struct parent:
    child: child
func (p parent) init(value: i32) -> result<bool, string>:
    p.child = child(value)?
    return Ok(true)
struct base:
    value: i32
func (b base) init(value: i32):
    b.value = value
struct derived extends base:
    other: i32
func (d derived) init(value: i32) -> result<bool, string>:
    super.init(value)
    if value < 0:
        return Err("derived")
    d.other = value + 1
    return Ok(true)
func main():
    print(parent(-1) is Err("child"), derived(-1) is Err("derived"))
    match parent(4):
        Ok(var p):
            print(p.child.value)
        Err(var error):
            print(error)
    match derived(4):
        Ok(var d):
            print(d.value, d.other)
        Err(var error):
            print(error)
'''
ORDER='''var calls = 0
func argument(value: i32) -> i32:
    calls = calls * 10 + value
    return value
value struct item:
    total: i32
func (i item) init(a: i32, b: i32 = 9) -> result<bool, string>:
    i.total = a + b
    return Ok(true)
func main():
    match item(b = argument(2), a = argument(1)):
        Ok(var made):
            print(made.total, calls)
        Err(var error):
            print(error)
    match item(1):
        Ok(var made):
            print(made.total)
        Err(var error):
            print(error)
'''
with tempfile.TemporaryDirectory(prefix='VOID G84 ') as directory:
    tmp=Path(directory)
    positives=[(repo/'examples/fallible_initialization/main.void','3\npoint\n7 7\nplayer\n'),
        (Path(__file__).with_name('integration.void'),'true\ntrue\n7\ntrue\ntrue\n8 9\n'),
        (Path(__file__).with_name('cleanup.void'),'true 111\ntrue 111\ntrue 0\ntrue 1000\n'),
        (Path(__file__).with_name('assignment.void'),'true 7\ntrue 9\n4\ntrue\n'),
        (Path(__file__).with_name('value_helpers.void'),'5\ntrue\n'),
        (Path(__file__).parent/'imported/main.void','true\ntrue\n9\n')]
    for name,text,output in [('match',MATCH,'true true\n'),('nested',NESTED,'true\n9\n'),('nested_managed',NESTED_MANAGED,'true true\n4\n4 5\n'),('order',ORDER,'3 21\n10\n')]:
        p=tmp/(name+'.void');p.write_text(text);positives.append((p,output))
    for n,(file,output) in enumerate(positives):
        inputs[str(file)]=file.read_text();run([compiler,'check',file])
        for mode,flags in enumerate((['--no-opt'],[])):
            run([compiler,'run',file,'--no-cache',*flags],output=output)
            image=tmp/(str(n)+'-'+str(mode));run([compiler,'build',file,'-o',image,*flags]);run([image],output=output)
    for name,(text,error) in NEGATIVE.items():
        p=tmp/(name+'.void');p.write_text(text);inputs[name]=text;run([compiler,'check',p],1,error=error)
    (tmp/'api.void').write_text((Path(__file__).parent/'imported/api.void').read_text())
    p=tmp/'private.void';p.write_text('use "./api.void" as api\nfunc main():\n    var p = api.point(1)\n')
    inputs['private']=p.read_text();run([compiler,'check',p],1,error='private')
print('G84 source: '+str(len(records))+' checks, ordinary constructors, results, ownership and JIT/AOT O0/O2 passed at 1 MiB')
