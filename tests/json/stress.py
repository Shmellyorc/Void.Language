from pathlib import Path
import tempfile, sys
import harness as h
count=4096
array='['+','.join(str(n%4) for n in range(count))+']'
wide='{'+','.join('"field'+str(n)+'":'+str(n) for n in range(128))+'}'
source='use json as json\nstruct wide:\n'+''.join('    field'+str(n)+': i32\n' for n in range(128))
source+='''func demo() -> result<i32, json.error>:
    var fixed = json.parse_as<[i32, 4096]>('''+h.literal(array)+''')?
    var total: i32 = 0
    for value in fixed:
        total += value
    print(total)
    var saved = json.stringify(fixed)?
    print(saved.byte_length)
    var dto = json.parse_as<wide>('''+h.literal(wide)+''')?
    print(dto.field127)
    print(json.stringify(dto)? == '''+h.literal(wide)+''')
    return Ok(0)
func main():
    if demo() is Err(var failure):
        print("error {failure.code} {failure.path}")
'''
which=sys.argv[3] if len(sys.argv)>3 else 'combined'
want=b'6144\n8193\n127\ntrue\n'
if which!='combined':
    pre,body=source.split('func demo()',1)
    fixed,wide=body.split('    var dto =',1)
    wide,tail=wide.split('    return Ok(0)',1)
    if which=='fixed':
        source='use json as json\nfunc demo()'+fixed+'    return Ok(0)'+tail
        want=b'6144\n8193\n'
    else:
        assert which=='wide'
        source=pre+'func demo() -> result<i32, json.error>:\n    var dto ='+wide+'    return Ok(0)'+tail
        want=b'127\ntrue\n'
with tempfile.TemporaryDirectory(prefix='void-json-stress-') as tmp:
    tmp=Path(tmp);src=tmp/'stress.void';src.write_text(source)
    for opt in [False,True]:
        flags=[] if opt else ['--no-opt']
        h.run([h.compiler,'run',src,*flags],src,want,timeout=600)
        native=tmp/('stress-O2' if opt else 'stress-O0')
        h.run([h.compiler,'build',src,'-o',native,*flags],src,timeout=600)
        h.run([native],src,want,timeout=600)
print(f'JSON stack stress {which}: all four modes passed')
