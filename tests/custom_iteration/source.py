"""G85 protocol, authority, generic/import integration and JIT/AOT O0/O2."""
from pathlib import Path
import hashlib,json,os,re,subprocess,sys,tempfile
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools'))
from test_output import output_directory
compiler,report=(Path(p).resolve() for p in sys.argv[1:]);repo=Path(__file__).resolve().parents[2]
records=[];inputs={}
def limits():
    import resource
    resource.setrlimit(resource.RLIMIT_STACK,(1048576,1048576))
    resource.setrlimit(resource.RLIMIT_CORE,(0,0))
def run(args,status=0,output=None,error=None,runtime_error=False):
    args=list(map(str,args));r=subprocess.run(args,capture_output=True,text=True,timeout=90,
        preexec_fn=limits if os.name=='posix' else None)
    row=dict(command=args,expected=status,actual=r.returncode,stdout=r.stdout,stderr=r.stderr);records.append(row)
    report.parent.mkdir(parents=True,exist_ok=True)
    report.write_text(json.dumps(dict(stack_bytes=1048576,compiler_sha256=hashlib.sha256(compiler.read_bytes()).hexdigest(),
        inputs=inputs,records=records),indent=2)+'\n')
    assert r.returncode==status,row
    if output is not None:assert r.stdout==output,row
    if error:assert error.lower() in r.stderr.lower() and re.search(r'\.void:\d+:\d+:',r.stderr),row
    if not runtime_error:assert 'runtime error:' not in r.stderr,row
    assert not any(x in r.stderr for x in ('AddressSanitizer','Assertion','std::','Missing owned')),row

CURSOR='''value struct cursor:
    position: i32
func (self cursor) next() -> option<i32>:
    if self.position == 2:
        return None
    self.position += 1
    return Some(self.position)
value struct sequence:
    count: i32
func (self sequence) iterator() -> cursor:
    return cursor()
'''
MAIN='func main():\n    for item in sequence():\n        print(item)\n'
NEGATIVE={
    'no_factory':('value struct sequence:\n    count: i32\n'+MAIN,'iterator'),
    'wrong_factory':('value struct sequence:\n    count: i32\nfunc (self sequence) iterator() -> i32:\n    return 1\n'+MAIN,'concrete iterator struct'),
    'optional_factory':(CURSOR.replace('iterator() -> cursor','iterator(unused: i32 = 0) -> cursor')+MAIN,'no parameters'),
    'no_next':(CURSOR.replace('next()', 'advance()')+MAIN,"next"),
    'wrong_next':(CURSOR.replace('option<i32>','i32').replace('return None','return 0').replace('return Some(self.position)','return self.position')+MAIN,'next() must return option'),
    'defaulted_next':(CURSOR.replace('next()','next(unused: i32 = 0)')+MAIN,'no parameters'),
    'no_reverse':(CURSOR+MAIN.replace('in sequence()', 'in reverse sequence()'),'reverse_iterator'),
    'no_index':(CURSOR+MAIN.replace('for item in','for item, key in'),'index'),
    'void_index':(CURSOR+'func (self cursor) index():\n    pass\n'+MAIN.replace('for item in','for item, key in'),'index() must return'),
    'nullable_factory':(CURSOR.replace('iterator() -> cursor','iterator() -> cursor?')+MAIN,'non-nullable concrete'),
    'generic_next':(CURSOR.replace('next()','next<t>()')+MAIN,'infer'),
    'mutating_const_factory':(CURSOR.replace('return cursor()','self.count += 1\n    return cursor()')+'func main():\n    const items = sequence()\n    for item in items:\n        print(item)\n','readonly'),
    'erased_factory':('interface abstract_cursor:\n    func next() -> option<i32>\nstruct cursor:\n    position: i32\nfunc (self cursor) next() -> option<i32>:\n    return None\nstruct sequence:\n    count: i32\nfunc (self sequence) iterator() -> abstract_cursor:\n    return cursor()\n'+MAIN,'concrete iterator struct'),
    'field_factory':('struct callable:\n    count: i32\nfunc (self callable) call() -> cursor:\n    return cursor()\nvalue struct cursor:\n    count: i32\nvalue struct sequence:\n    iterator: callable\nfunc main():\n    for item in sequence(iterator = callable()):\n        print(item)\n','ordinary synchronous'),
    'range_index':('func main():\n    for item, key in 0..2:\n        print(item)\n','one item binding'),
}
# File-private protocol members are exercised below at an imported use site.

CASES={
    'shape':(CURSOR+'''func main():
    var empty = 0
    var nested = 0
    for item in sequence():
        for other in sequence():
            nested += item * other
    for item in sequence():
        empty += 1
    print(nested, empty)
''','9 2\n'),
    'semantic_keys':(CURSOR+'''func (self cursor) index() -> string:
    if self.position == 1:
        return "first"
    return "second"
func main():
    for item, key in sequence():
        print(item, key)
''','1 first\n2 second\n'),
    'value_copy':('''value struct tile:
    value: i32
value struct cursor:
    position: i32
func (self cursor) next() -> option<tile>:
    self.position += 1
    if self.position > 1:
        return None
    return Some(tile(value = 7))
value struct sequence:
    count: i32
func (self sequence) iterator() -> cursor:
    return cursor()
func main():
    var copied = tile()
    for item in sequence():
        copied = item
        item.value = 9
        print(copied.value, item.value)
    print(copied.value)
''','7 9\n7\n'),
    'empty_single':('''value struct cursor:
    remaining: i32
func (self cursor) next() -> option<i32>:
    if self.remaining == 0:
        return None
    self.remaining -= 1
    return Some(8)
value struct sequence:
    count: i32
func (self sequence) iterator() -> cursor:
    return cursor(remaining = self.count)
func main():
    for item in sequence(count = 0):
        print("unreachable")
    for item in sequence(count = 1):
        print(item)
''','8\n'),
    'defer_print':('''var effects = 0
func argument() -> i32:
    effects += 1
    return effects
func main():
    defer print(null, argument(), sep = "/", end = "!\\n")
    print("before", effects)
''','before 1\nnull/1!\n'),
    'builtin':('''func main():
    var values = [4, 5]
    for item, key in reverse values:
        print(item, key)
    var fixed: [i32, 2] = [6, 7]
    for item, key in fixed:
        print(item, key)
    var map: {string: i32} = {"a": 8, "b": 9}
    for item, key in reverse map:
        print(item, key)
    for item in reverse 1..3:
        print(item)
''','5 1\n4 0\n6 0\n7 1\n9 b\n8 a\n3\n2\n1\n'),
}
# The parser's unchanged 64-block guard and native 1 MiB requirement also apply
# when custom iteration expands into nested pattern loops and cleanup scopes.
for kind in ('value', 'managed', 'deferred'):
    prefix="""var closed = 0
value struct cursor:
    done: bool = false
func (self cursor) next() -> option<i32>:
    if self.done:
        return None
    self.done = true
    return Some(1)
value struct sequence:
    count: i32
func (self sequence) iterator() -> cursor:
    return cursor()
func main():
"""
    if kind!='value':
        prefix=prefix.replace('value struct cursor:', 'struct cursor:').replace('value struct sequence:', 'func (self cursor) deinit():\n    closed += 1\nvalue struct sequence:')
    if kind=='deferred':prefix+='    defer print("deferred")\n'
    text=prefix+''.join('    '*(n+1)+'for item'+str(n)+' in sequence():\n' for n in range(48))
    text+='    '*49+'print(item47)\n'
    if kind!='value':text+='    print(closed)\n'
    expected='1\n'+('48\n' if kind!='value' else '')+('deferred\n' if kind=='deferred' else '')
    CASES['nested_'+kind]=(text,expected)

NEGATIVE['nesting_guard']=(CURSOR+'func main():\n'+''.join('    '*(n+1)+'for item'+str(n)+' in sequence():\n' for n in range(65))+'    '*66+'print(item64)\n','Source nesting limit')

# Prefix the imported fixture to exercise readonly leaves through option<T>.
READONLY='''struct tile:
    value: i32
value struct cursor:
    values: [tile]
    position: i32
func (self cursor) next() -> option<tile>:
    if self.position == 1:
        return None
    self.position += 1
    return Some(self.values[0])
struct sequence:
    values: [tile]
func (self sequence) iterator() -> cursor:
    return cursor(values = self.values)
func main():
    const values = sequence(values = [tile(value = 3)])
    for item in values:
        item.value = 9
'''
NEGATIVE['readonly_yield']=(READONLY,'readonly')

with tempfile.TemporaryDirectory(prefix='fixtures-',dir=output_directory()) as directory:
    tmp=Path(directory)
    positives=[(repo/'examples/custom_iteration/main.void','10\n20\n30\n30 2\n20 1\n10 0\n'),
        (repo/'examples/custom_iteration/checked.void','1\ncollection changed\n'),
        (Path(__file__).with_name('cleanup.void'),'deferred 0\n12339 false\ndeferred 0\n12339 false\ndeferred 0\n139 false\ndeferred 0\n139 false\ndeferred 0\n139 true\n112 8\n0 9\n'),
        (Path(__file__).parent/'imported/main.void','6 15\na 0\nb 1\n')]
    for name,(text,expected) in CASES.items():
        p=tmp/(name+'.void');p.write_text(text);positives.append((p,expected))
    for n,(file,expected) in enumerate(positives):
        inputs[str(file)]=file.read_text();run([compiler,'check',file])
        for mode,flags in enumerate((['--no-opt'],[])):
            run([compiler,'run',file,'--no-cache',*flags],output=expected)
            image=tmp/(str(n)+'-'+str(mode)+('.exe' if os.name=='nt' else ''))
            run([compiler,'build',file,'-o',image,*flags]);run([image],output=expected)
    for name,(text,error) in NEGATIVE.items():
        p=tmp/(name+'.void');p.write_text(text);inputs[name]=text;run([compiler,'check',p],1,error=error)
    # Structural mutations are still checked on the next access/completion for
    # built-ins, in both directions; break remains an immediate escape.
    for reverse in ('', 'reverse '):
        p=tmp/('invalidated-'+str(bool(reverse))+'.void')
        text='func main():\n    var values = [1]\n    for item in '+reverse+'values:\n        values.push(2)\n'
        p.write_text(text);inputs[p.name]=text
        for flags in (['--no-opt'],[]):
            run([compiler,'run',p,'--no-cache',*flags],1,error='iteration',runtime_error=True)
    (tmp/'api.void').write_bytes((Path(__file__).parent/'imported/api.void').read_bytes())
    p=tmp/'private.void';p.write_text('use "./api.void" as api\nfunc main():\n    for item in api.secret():\n        print(item)\n')
    inputs['private_import']=p.read_text();run([compiler,'check',p],1,error='accessible')
    (tmp/'api.void').write_text((Path(__file__).parent/'imported/api.void').read_text().replace('func (self cursor<t>) next', 'private func (self cursor<t>) next'))
    p.write_text('use "./api.void" as api\nfunc main():\n    for item in api.sequence(values = [1]):\n        print(item)\n')
    inputs['private_next_import']=p.read_text();run([compiler,'check',p],1,error='accessible')
    (tmp/'api.void').write_text("""struct cursor:
    done: bool
func (self cursor) next() -> option<i32>:
    return None
private func (self cursor) deinit():
    pass
struct sequence:
    count: i32
func (self sequence) iterator() -> cursor:
    return cursor()
""")
    p.write_text('use "./api.void" as api\nfunc main():\n    for item in api.sequence():\n        print(item)\n')
    inputs['private_cleanup_import']=p.read_text();run([compiler,'check',p],1,error='accessible')
print('G85 source: '+str(len(records))+' records; protocol, ownership, imports and JIT/AOT O0/O2 passed at 1 MiB')
