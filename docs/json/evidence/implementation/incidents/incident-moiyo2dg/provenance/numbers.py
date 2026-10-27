from pathlib import Path
import tempfile
import harness as h
vectors=[]
for signed in [True,False]:
    for width in [8,16,32,64]:
        t=('i' if signed else 'u')+str(width)
        lo=-(1<<(width-1)) if signed else 0
        hi=(1<<(width-1))-1 if signed else (1<<width)-1
        for value in [lo,hi,0]:vectors.append((t,str(value),str(value)))
        vectors.extend([(t,str(lo-1),'E7'),(t,str(hi+1),'E7'),(t,'-0','0'),(t,'-0.000e999999999999999999999','0'),(t,'1.00e2','100'),(t,'100e-2','1'),(t,'1e-1','E7'),(t,'1.5','E7'),(t,'1e999999999999999999999','E7'),(t,'1e-999999999999999999999','E7')])
vectors += [('i64','9007199254740993','9007199254740993'),('f32','1e400','E7'),('f64','1e400','E7'),('f32','1e-999','E7'),('f64','1e-999','E7'),('f32','-0e999999999999999999999','-0'),('f64','-0e999999999999999999999','-0'),('f32','1.000000059604644775390625','1'),('f64','1.00000000000000011102230246251565404236316680908203125','1'),('f32','1.401298464324817070923729583289916131280e-45','1e-45'),('f64','4.9406564584124654e-324','5e-324')]
source='''use json as json
func number<t>(text: string) -> string:
    var parsed = json.parse(text)
    if parsed is Ok(var node):
        var converted = node.as_number<t>()
        if converted is Ok(var result):
            return string(result)
        if converted is Err(var failure):
            return "E{failure.code}"
    return "parse error"
func main():
'''
expected=[]
for t,token,want in vectors:
    source+='    print(number<'+t+'>('+h.literal(token)+'))\n';expected.append(want)
with tempfile.TemporaryDirectory(prefix='void-json-numbers-') as tmp:
    src=Path(tmp)/'numbers.void';src.write_text(source);expected=('\n'.join(expected)+'\n').encode()
    for opt in [False,True]:
        flags=[] if opt else ['--no-opt']
        h.run([h.compiler,'run',src,*flags],src,expected)
        native=Path(tmp)/('numbers-O2' if opt else 'numbers-O0')
        h.run([h.compiler,'build',src,'-o',native,*flags],src)
        h.run([native],src,expected)
print(f'numbers {h.profile}: {len(vectors)} vectors in 4 execution modes passed')
