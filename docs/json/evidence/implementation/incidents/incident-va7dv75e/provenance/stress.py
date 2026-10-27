from pathlib import Path
import tempfile
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
with tempfile.TemporaryDirectory(prefix='void-json-stress-') as tmp:
    tmp=Path(tmp);src=tmp/'stress.void';src.write_text(source)
    for opt in [False,True]:
        flags=[] if opt else ['--no-opt']
        h.run([h.compiler,'run',src,*flags],src,b'6144\n8193\n127\ntrue\n',timeout=300)
        native=tmp/('stress-O2' if opt else 'stress-O0')
        h.run([h.compiler,'build',src,'-o',native,*flags],src,timeout=300)
        h.run([native],src,b'6144\n8193\n127\ntrue\n',timeout=300)
print('JSON stack stress: 4096 fixed values and 128-field DTO in all four modes passed')
