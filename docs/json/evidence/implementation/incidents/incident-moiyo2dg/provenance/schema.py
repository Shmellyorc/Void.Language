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
}
with tempfile.TemporaryDirectory(prefix='void-json-schema-') as tmp:
    tmp=Path(tmp);src=tmp/'schema.void';src.write_text(source);expected=('\n'.join(expected)+'\n').encode()
    for flags in [['--no-opt'],[]]:h.run([h.compiler,'run',src,*flags],src,expected)
    for name,decl in negatives.items():
        src=tmp/(name+'.void');src.write_text('use json as json\n'+decl+'func main():\n    var data = json.parse_as<secret>("{{}}")\n')
        r=h.run([h.compiler,'check',src],src,status=1)
        assert b'JSON does not support' in r.stderr,(name,r.stderr)
print(f'schema {h.profile}: {len(vectors)} error/order vectors and {len(negatives)} admission negatives passed')
