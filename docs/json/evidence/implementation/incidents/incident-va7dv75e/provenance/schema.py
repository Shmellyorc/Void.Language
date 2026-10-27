from pathlib import Path
import tempfile
import harness as h
header='''use json as json
enum shade:
    red
    blue
struct wrapper:
    item: i32
struct record:
    id: i32 = 17
    child: wrapper?
    tag: shade
func probe<t>(text: string, unknown: json.unknown_fields = json.unknown_fields.reject) -> string:
    var parsed = json.parse_as<t>(text, unknown = unknown)
    if parsed is Ok(var data):
        return "ok"
    if parsed is Err(var failure):
        return "{failure.code}@{failure.offset}:{failure.path}"
    return "unreachable"
func main():
'''
vectors=[('record','{"id":1,"tag":"red"}','ok',False),('record','{}','4@0:/id',False),('record','{"extra":0}','10@1:/extra',False),('record','{"extra":0}','4@0:/id',True),('record','{"id":null,"tag":"red"}','6@6:/id',False),('record','{"tag":"unknown","id":false}','6@22:/id',False),('record','{"id":1,"tag":"unknown"}','9@14:/tag',False),('record','{"id":1,"child":{"item":"bad"},"tag":"red"}','6@24:/child/item',False),('record','{"id":1,"child":null,"tag":"blue","extra":[1,2]}','ok',True),('[i32, 2]','[1]','11@0:',False),('u8','-1','7@0:',False),('bool','0','6@0:',False),('string','false','6@0:',False),('record','{"b":0,"a":0}','10@1:/b',False)]
expected=[];source=header
for t,text,want,ignore in vectors:
    source+='    print(probe<'+t+'>('+h.literal(text)+(', json.unknown_fields.ignore' if ignore else '')+'))\n';expected.append(want)
negatives={
'private':'struct secret:\n    private stored: i32\n',
'internal':'struct secret:\n    internal stored: i32\n',
'init':'struct secret:\n    stored: i32\nfunc (self secret) init():\n    self.stored = 1\n',
'deinit':'struct secret:\n    stored: i32\nfunc (self secret) deinit():\n    self.stored = 0\n',
'property':'struct secret:\nproperty (self secret) computed: i32:\n    get:\n        return 1\n',
'payload':'enum secret:\n    number(item: i32)\n',
'result':'struct secret:\n    stored: result<i32, string>\n',
'map':'struct secret:\n    stored: {i32: string}\n',
'callable':'struct secret:\n    stored: func(i32) -> i32\n',
'interface':'interface readable:\n    func read() -> i32\nstruct secret:\n    stored: readable\n',
'event':'struct secret:\n    event changed: func(i32)\n',
'resource':'use io as io\nstruct secret:\n    stored: io.file\n',
'nested':'struct inner:\n    private stored: i32\nstruct secret:\n    child: inner\n',
'inherited':'struct base:\n    private stored: i32\nstruct secret extends base:\n    shown: i32\n',
}
with tempfile.TemporaryDirectory(prefix='void-json-schema-') as tmp:
    tmp=Path(tmp);src=tmp/'schema.void';src.write_text(source);expected=('\n'.join(expected)+'\n').encode()
    for opt in [False,True]:
        flags=[] if opt else ['--no-opt']
        h.run([h.compiler,'run',src,*flags],src,expected)
        native=tmp/('schema-O2' if opt else 'schema-O0')
        h.run([h.compiler,'build',src,'-o',native,*flags],src)
        h.run([native],src,expected)
    for name,decl in negatives.items():
        src=tmp/(name+'.void');src.write_text('use json as json\n'+decl+'func main():\n    var data = json.parse_as<secret>("{{}}")\n')
        r=h.run([h.compiler,'check',src],src,status=1)
        assert b'JSON does not support' in r.stderr,(name,r.stderr)
        if name in ['nested','inherited']:
            assert str(src).encode() in r.stderr and b'Offending stored field declared here' in r.stderr,r.stderr
    for target in ['bool','string','shade','[i32]']:
        src=tmp/'bad_number.void'
        src.write_text('use json as json\nenum shade:\n    red\nfunc main():\n    if json.parse("0") is Ok(var node):\n        var number = node.as_number<'+target+'>()\n')
        r=h.run([h.compiler,'check',src],src,status=1)
        assert b'ten numeric primitive' in r.stderr,r.stderr
    src=h.ROOT/'tests/json/task_rejected.void'
    r=h.run([h.compiler,'check',src],src,status=1)
    # Existing generic inference selects the task's result type. Ordinary
    # argument checking rejects the task before any JSON adapter can run.
    assert b'VOID2001' in r.stderr and b'Type mismatch' in r.stderr and b'$task:' in r.stderr,r.stderr
    for accessor in ['at(0)','as_array()']:
        src=tmp/'readonly_node.void'
        expression='take(child)' if accessor=='at(0)' else 'take(child[0])'
        src.write_text('use json as json\nfunc take(input: json.value):\n    return\nfunc main():\n    if json.parse("[null]") is Ok(var root):\n        const view = root\n        if view.'+accessor+' is Ok(var child):\n            '+expression+'\n')
        r=h.run([h.compiler,'check',src],src,status=1)
        assert b'readonly' in r.stderr.lower(),r.stderr
print(f'schema {h.profile}: {len(vectors)} error/order vectors in four modes and {len(negatives)+7} admission/authority negatives passed')
