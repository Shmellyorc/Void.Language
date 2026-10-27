from pathlib import Path
import tempfile
import harness as h

valid=['null','true','false','0','-0','18446744073709551615','1e400','1.00e+02','""','"a\\u0000b"','[]','{}','[1,true,null,"x"]',' {"a":[],"b":{}} \r\n\t','"\\ud800\\udc00"','"\\udbff\\udfff"','"\ufeff\uffff\U0010ffff"','"\\\"\\\\\\/\\b\\f\\n\\r\\t"','{"a":1,"A":2}','{"é":1,"é":2}','"\x7f"']
bad=[('',1,0,''),(' ',1,1,''),('+1',1,0,''),('01',1,1,''),('.5',1,0,''),('1.',1,2,''),('1e',1,2,''),('1e+',1,3,''),('nan',1,1,''),('true false',1,5,''),('[1,]',1,3,'/1'),('{"a":}',1,5,'/a'),('{a:0}',1,1,''),('/*x*/0',1,0,''),('\ufeff0',1,0,''),('"\n"',1,1,''),('"\\q"',1,1,''),('"\\ud800"',2,1,''),('"\\udc00"',2,1,''),('"\\ud800\\u0000"',2,1,''),('"\\uXXXX"',2,1,''),('{"a":0,"\\u0061":9}',3,7,'/a'),('{"~/":0,"~/":1}',3,8,'/~0~1')]
source='''use json as json
func probe(text: string) -> string:
    var outcome = json.parse(text)
    if outcome is Ok(var node):
        return "ok"
    if outcome is Err(var failure):
        return "{failure.code}@{failure.offset}:{failure.path}"
    return "unreachable"
func shortened(text: string) -> string:
    if json.parse(text) is Err(var failure):
        return "{failure.code}@{failure.offset}"
    return "ok"
func main():
'''
expected=[]
for text in valid:source+='    print(probe('+h.literal(text)+'))\n';expected.append('ok')
for text,code,offset,path in bad:source+='    print(probe('+h.literal(text)+'))\n';expected.append(f'{code}@{offset}:{path}')
truncated=['{"a":[true,false,null,"x\\u0041",123.5e+2]}', '"\\ud800\\udc00"']
extra=0
for document in truncated:
    for stop in range(len(document)):
        text=document[:stop]
        source+='    print(shortened('+h.literal(text)+'))\n'
        expected.append('1@'+str(len(text.encode())))
        extra+=1
with tempfile.TemporaryDirectory(prefix='void-json-dynamic-') as tmp:
    source_path=Path(tmp)/'grammar.void';source_path.write_text(source)
    expected=('\n'.join(expected)+'\n').encode()
    for opt in [False,True]:
        flags=[] if opt else ['--no-opt']
        h.run([h.compiler,'run',source_path,*flags],source_path,expected)
        native=Path(tmp)/('grammar-O2' if opt else 'grammar-O0')
        h.run([h.compiler,'build',source_path,'-o',native,*flags],source_path)
        h.run([native],source_path,expected)
print(f'grammar {h.profile}: {len(valid)+len(bad)+extra} vectors in 4 execution modes passed')
