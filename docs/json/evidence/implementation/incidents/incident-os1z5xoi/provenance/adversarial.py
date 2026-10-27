"""Deep typed continuations and collision-heavy dictionary work boundaries."""
from pathlib import Path
import tempfile, sys
import harness as h

# Runtime dictionary string hashing is FNV-1a over UTF-8 bytes. Select keys
# sharing the low eight bits, including the length word charged by the policy.
def bucket(text):
    result=14695981039346656037
    data=text.encode()
    for byte in len(data).to_bytes(8,sys.byteorder)+data:result=((result^byte)*1099511628211)&((1<<64)-1)
    return result&255
keys=[]
for n in range(100000):
    key='shared-prefix-'+str(n)
    if bucket(key)==0:keys.append(key)
    if len(keys)==64:break
assert len(keys)==64
collision='{'+','.join('"'+key+'":'+str(n) for n,key in enumerate(keys))+'}'
deep=lambda count:'{"next":'*count+'null'+'}'*count
source='''use json as json
struct link:
    next: link?
func demo() -> result<i32, json.error>:
'''
expected=[]
for depth in [64,65,128,129]:
    budget=64 if depth<100 else 128
    text=deep(depth)
    source+='    var outcome'+str(depth)+' = json.parse_as<link>('+h.literal(text)+', json.limits(max_depth = '+str(budget)+'))\n'
    source+='    if outcome'+str(depth)+' is Ok(var root):\n'
    source+='        print(json.stringify(root, json.limits(max_depth = '+str(budget)+'))? == '+h.literal(text)+')\n'
    source+='    else if outcome'+str(depth)+' is Err(var failure):\n        print(failure.code)\n'
    expected.append('true' if depth in [64,128] else '12')
source+='    var node = json.parse('+h.literal(collision)+')?\n'
source+='''    var loaded = json.from_value<{string: i32}>(node)?
    print(loaded.count)
    print(json.stringify(loaded)? == '''+h.literal(collision)+''')
    if json.from_value<{string: i32}>(node, json.limits(max_work = 1500)) is Err(var failure):
        print(failure.code)
    if json.parse('''+h.literal(collision)+''', json.limits(max_work = 1500)) is Err(var failure):
        print(failure.code)
    var shared = link()
    var aliases = [shared, shared]
    print(json.stringify(aliases)? == "[{{\\"next\\":null}},{{\\"next\\":null}}]")
    if json.to_value(aliases, json.limits(max_nodes = 3)) is Err(var failure):
        print(failure.code)
'''
expected+=['64','true','12','12','true','12']
scalars=[('i8','-128','-128'),('u8','255','255'),('i16','-32768','-32768'),('u16','65535','65535'),('i32','-2147483648','-2147483648'),('u32','4294967295','4294967295'),('i64','-9223372036854775808','-9223372036854775808'),('u64','18446744073709551615','18446744073709551615'),('f32','1.00000011920928955078125','1.0000001'),('f64','1.0000000000000002220446049250313080847','1.0000000000000002')]
for target,token,want in scalars:
    source+='    print(json.stringify(json.parse_as<'+target+'>('+h.literal(token)+')?)?)\n'
    expected.append(want)
escapes='"'+''.join('\\u'+format(n,'04x') for n in range(32))+'\\\"\\\\\\/"'
decoded=''.join(chr(n) for n in range(32))+'"\\/'
source+='    print(json.parse('+h.literal(escapes)+')?.as_string()? == '+h.literal(decoded)+')\n'
source+='    print(json.parse('+h.literal('"\\ud800\\udc00\\udbff\\udfff"')+')?.as_string()? == '+h.literal('\U00010000\U0010ffff')+')\n'
expected+=['true','true']
source+='''    return Ok(0)
func main():
    if demo() is Err(var failure):
        print("error {failure.code} {failure.path}")
'''
with tempfile.TemporaryDirectory(prefix='void-json-adversarial-') as tmp:
    tmp=Path(tmp);src=tmp/'adversarial.void';src.write_text(source)
    for opt in [False,True]:
        flags=[] if opt else ['--no-opt']
        h.run([h.compiler,'run',src,*flags],src,('\n'.join(expected)+'\n').encode(),timeout=300)
        native=tmp/('adversarial-O2' if opt else 'adversarial-O0')
        h.run([h.compiler,'build',src,'-o',native,*flags],src,timeout=300)
        h.run([native],src,('\n'.join(expected)+'\n').encode(),timeout=300)
print('JSON adversarial: 22 vectors in all four modes passed')
