"""G86 source contracts, generics, coordination and limited-stack recovery."""
from pathlib import Path
import hashlib,json,os,re,subprocess,sys,tempfile
repo=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(repo/'tools'))
from test_output import output_directory
compiler,runner,report=(Path(p).resolve() for p in sys.argv[1:]);records=[];inputs={}
def limits():
    import resource
    resource.setrlimit(resource.RLIMIT_STACK,(1048576,1048576));resource.setrlimit(resource.RLIMIT_CORE,(0,0))
def run(args,status=0,error=None):
    args=list(map(str,args));r=subprocess.run(args,capture_output=True,text=True,timeout=90,
        preexec_fn=limits if os.name=='posix' else None)
    row=dict(command=args,expected=status,actual=r.returncode,stdout=r.stdout,stderr=r.stderr);records.append(row)
    report.parent.mkdir(parents=True,exist_ok=True)
    report.write_text(json.dumps(dict(stack_bytes=1048576,compiler_sha256=hashlib.sha256(compiler.read_bytes()).hexdigest(),
        inputs=inputs,records=records),indent=2)+'\n')
    assert r.returncode==status,row
    if error:assert error.lower() in r.stderr.lower() and re.search(r'\.void:\d+:\d+:',r.stderr),row
    assert not any(x in r.stderr for x in ('AddressSanitizer','runtime error:','Assertion','std::','Missing owned')),row
    return r
API=(repo/'examples/async/advanced/sequence.void').read_text()
MAIN='''async func entry(value: i32) -> i32:
    var total = 0
    for await item, key in sequence(values = [1, 2, 4]):
        total += item
    return total
'''
NEGATIVE={
    'sync_context':(API+MAIN.replace('async func entry','func entry'),'for await requires an async'),
    'sync_next':(API.replace('async func (self cursor<t>) next','func (self cursor<t>) next').replace('await identity(self.values[self.position])','self.values[self.position]')+MAIN,"ordinary async 'next"),
    'sync_loop_async_next':(API.replace('async_iterator()','iterator()')+MAIN.replace('for await','for'),'ordinary synchronous'),
    'wrong_next':(API.replace('next() -> option<t>','next() -> t').replace('return None','return self.values[0]').replace('return Some(value)','return value')+MAIN,'must return option'),
    'default_next':(API.replace('next() ->','next(unused: i32 = 0) ->')+MAIN,'no parameters'),
    'no_next':(API.replace('next() ->','advance() ->')+MAIN,"next"),
    'no_index':(API.replace('index() ->','key() ->')+MAIN,"index"),
    'async_index':(API.replace('func (self cursor<t>) index()', 'async func (self cursor<t>) index()')+MAIN,'ordinary synchronous'),
    'async_factory':(API.replace('func (self sequence<t>) async_iterator()', 'async func (self sequence<t>) async_iterator()')+MAIN,'ordinary synchronous'),
    'no_reverse':(API.replace('reverse_async_iterator()', 'backwards()')+MAIN.replace('in sequence(', 'in reverse sequence('),'reverse_async_iterator'),
    'range':(MAIN.replace('item, key in sequence(values = [1, 2, 4])','item in 0..3'),'not a range'),
    'builtin':(MAIN.replace('sequence(values = [1, 2, 4])','[1, 2, 4]'),'custom async iterable'),
    'invalid_await_condition':('async func entry(value: i32) -> i32:\n    for await true:\n        pass\n    return value\n','await requires'),
    'no_binding':('async func entry(value: i32) -> i32:\n    for await :\n        pass\n    return value\n','Expected an expression'),
    'nullable_state':(API.replace('async_iterator() -> cursor<t>','async_iterator() -> cursor<t>?')+MAIN,'non-nullable concrete'),
    'scalar_state':('struct sequence:\n    unused: i32\nfunc (self sequence) async_iterator() -> i32:\n    return 1\n'+MAIN.replace('sequence(values = [1, 2, 4])','sequence()'),'concrete iterator struct'),
    'value_receiver':(API.replace('struct cursor<t>:', 'value struct cursor<t>:')+MAIN,'value receivers'),
    'readonly_state':(API+'async func entry(value: i32) -> i32:\n    const input = sequence(values = [1, 2])\n    for await item in input:\n        pass\n    return value\n','readonly'),
    'async_value':(API+'async func entry(value: i32) -> i32:\n    var callback = identity<i32>\n    return value\n','async callable'),
    'async_method_value':(API+'async func entry(value: i32) -> i32:\n    var state = cursor(values = [1])\n    var callback = state.next\n    return value\n','async callable'),
    'worker_managed':(API+'func process(input: sequence<i32>) -> i32:\n    return 7\nasync func entry(value: i32) -> i32:\n    return await spawn process(sequence(values = [1]))\n','spawn transfers'),
    'deferred_loop':(API+'async func entry(value: i32) -> i32:\n    defer {\n        for await item in sequence(values = [1]):\n            pass\n    }\n    return value\n','for await requires an async'),
}
with tempfile.TemporaryDirectory(prefix='g86-',dir=output_directory()) as directory:
    root=Path(directory)
    for name,(text,error) in NEGATIVE.items():
        p=root/(name+'.void');p.write_text(text);inputs[name]=text;run([compiler,'check',p],1,error)
    positives={'basic':API+MAIN,
        'empty':API+MAIN.replace('[1, 2, 4]','[]').replace('sequence(values = [])','sequence<i32>(values = [])').replace('return total','return total + 7'),
        'reverse':API+MAIN.replace('in sequence(', 'in reverse sequence('),
        'suspended_body':API+MAIN.replace('total += item','var read = func() -> i32:\n            return item\n        total += await identity(read())'),
        'generic_interface':API+'''async func via_interface(input: iterable<i32>) -> i32:
    var sum = 0
    for await item in input:
        sum += item
    return sum
async func entry(value: i32) -> i32:
    return await via_interface(sequence(values = [1, 2, 4]))
''',
        'typed_errors':API+'''async func checked(value: i32) -> result<i32, string>:
    if value == 0:
        return Err("typed error")
    return Ok(value)
async func entry(value: i32) -> i32:
    with tasks.group() as jobs:
        var first = checked(0)
        var second = checked(7)
        var items = sequence(values = [await first, await second])
        var sum = 0
        for await result in items:
            match result:
                Ok(var number):
                    sum += number
                Err(var failure):
                    pass
        return sum
'''}
    positives['fixed_copy']=API+'''value struct packet:
    cells: [i32, 2]
async func entry(value: i32) -> i32:
    var original = packet(cells = [3, 4])
    var input = sequence(values = [original])
    var job = identity(original)
    var first = await job
    first.cells[0] = 99
    var again = await job
    for await item in input:
        item.cells[0] = 100
    return input.values[0].cells[0] + again.cells[1]
'''
    positives['once_and_keys']='''struct ledger:
    sources: i32
    factories: i32
    advances: i32
    keys: i32
struct cursor:
    owner: ledger
async func (self cursor) next() -> option<i32>:
    self.owner.advances += 1
    if self.owner.advances > 2:
        return None
    return Some(self.owner.advances)
func (self cursor) index() -> string:
    self.owner.keys += 1
    return "key"
struct sequence:
    owner: ledger
func source(owner: ledger) -> sequence:
    owner.sources += 1
    return sequence(owner = owner)
func (self sequence) async_iterator() -> cursor:
    self.owner.factories += 1
    return cursor(owner = self.owner)
async func entry(value: i32) -> i32:
    var owner = ledger()
    for await item, key in source(owner):
        if key != "key":
            return -1
    if owner.sources != 1 or owner.factories != 1 or owner.advances != 3 or owner.keys != 2:
        return -2
    return 7
'''
    positives['promoted_managed_receiver']=API+'''value struct wrapper<t>:
    using state: cursor<t>
struct wrapped<t>:
    values: [t]
func (self wrapped<t>) async_iterator() -> wrapper<t>:
    return wrapper(state = cursor<t>(values = self.values))
async func entry(value: i32) -> i32:
    var sum = 0
    for await item, key in wrapped(values = [3, 4]):
        sum += item
    return sum
'''
    positives['await_condition_call']='struct counter:\n    value: i32\nasync func ready(owner: counter) -> bool:\n    owner.value += 1\n    return owner.value < 3\nasync func entry(value: i32) -> i32:\n    var owner = counter()\n    for await ready(owner):\n        pass\n    return owner.value + 4\n'
    positives['await_condition_handle']='''async func ready() -> bool:
    return true
async func entry(value: i32) -> i32:
    var pending = ready()
    var total = 0
    for await pending:
        total += 1
        if total == 3:
            break
    return total + 4
'''
    # Retain the same depth contract and original 1 MiB stack as synchronous loops.
    for depth in (24,48):
        text=API+'async func entry(value: i32) -> i32:\n'
        for n in range(depth):text+='    '*(n+1)+f'for await item{n} in sequence(values = [1]):\n'
        text+='    '*(depth+1)+'var ignored = await identity(0)\n    return 7\n'
        positives['nested'+str(depth)]=text
    for name,text in positives.items():
        p=root/(name+'.void');p.write_text(text);inputs[name]=text
        run([compiler,'check',p]);r=run([runner,'--async',p]);assert r.stdout=='Debug source execution passed\nO2 source execution passed\n',r
    file=repo/'examples/async/advanced/workflow.void';inputs['workflow']=file.read_text()
    run([compiler,'check',file]);run([runner,'--async',file])
    # Visibility failures must resolve at the imported use site, including cleanup.
    source='use "./api.void" as api\n'+MAIN.replace('sequence(', 'api.sequence(')
    for name,change in [('factory',('func (self sequence<t>) async_iterator','private func (self sequence<t>) async_iterator')),
                        ('next',('async func (self cursor<t>) next','private async func (self cursor<t>) next')),
                        ('cleanup',('', ''))]:
        api=API.replace(*change) if name!='cleanup' else API+'private func (self cursor<t>) deinit():\n    pass\n'
        (root/'api.void').write_text(api);p=root/('private_'+name+'.void');p.write_text(source);inputs[name]=api
        run([compiler,'check',p],1,'accessible')
print('G86 source: '+str(len(records))+' records; async protocol, generic coordination and recovery passed at 1 MiB')
