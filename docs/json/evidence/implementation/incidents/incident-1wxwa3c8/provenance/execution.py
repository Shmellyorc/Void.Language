from pathlib import Path
import tempfile, sys
import harness as h
expected={
 'validated':b'{"score":7}\nScore must be nonnegative\n',
 'accessors':b'missing 4 0 /missing\n/@stdlib/json.void::value_kind.null_value\nwrong 6\n30\n10\nrange 5 /-4\nrange 5 /-9223372036854775808\nkey 0 4\nkey 1 5\nkey 2 2\n3\ntrue\n',
 'dynamic_smoke':b'bytes 6\n0: 10\n1: 20\n2: 30\n/@stdlib/json.void::value_kind.null_value\n',
 'typed_smoke':b'0: 10\n1: 20\n2: 30\n{"hp":100}\nhp 100\n{"name":"Ada","hp":7,"levels":[1,2]}\n[1,2,3]\n"blue"\n18446744073709551615\n18446744073709551615\n',
 'shapes':b'{"item":[{"item":9}]}\n{"label":"head","next":{"label":"tail","next":null}}\n{"id":9223372036854775807,"position":{"x":1,"y":2}}\n{"data":{"x":4,"y":5},"tags":["a","b"]}\n[{"label":"a","next":null},{"label":"b","next":null}]\nnull\n{}\n{"a":[-0,1e400],"x":"\\u0000"}\n{"a":[-0,1e400],"x":"\\u0000"}\n',
}
controls='"'
short={8:'\\b',12:'\\f',10:'\\n',13:'\\r',9:'\\t'}
for c in range(32):controls+=short.get(c,'\\u00'+format(c,'02x'))
controls+='\\"\\\\/\x7f"'
expected['ownership']=('same child\nnew root\n0\n0\nnew child\nouter 2 original 1\n[{"x":1e400}]\ninvalid UTF-8\n'+controls+'\n8\n8\n8\n').encode()
if len(sys.argv)>3:expected={sys.argv[3]:expected[sys.argv[3]]}
with tempfile.TemporaryDirectory(prefix='void-json-execution-') as tmp:
    for name,want in expected.items():
        src=h.ROOT/('examples/json' if name=='validated' else 'tests/json')/(name+'.void')
        for opt in [False,True]:
            flags=[] if opt else ['--no-opt']
            h.run([h.compiler,'run',src,*flags],src,want)
            native=Path(tmp)/(name+('-O2' if opt else '-O0'))
            h.run([h.compiler,'build',src,'-o',native,*flags],src)
            h.run([native],src,want)
print(f'execution {h.profile}: {len(h.records)}/{len(h.records)} processes passed')
