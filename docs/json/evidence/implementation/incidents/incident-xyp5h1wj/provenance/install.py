from pathlib import Path
import tempfile, os
import harness as h
with tempfile.TemporaryDirectory(prefix='void-json-installed-') as tmp:
    work=Path(tmp);src=work/'main.void';src.write_bytes((h.ROOT/'tests/json/typed_smoke.void').read_bytes())
    prefix=work/'original'
    h.run(['cmake','--install',h.compiler.parent,'--prefix',prefix,'--component','compiler'],src)
    relocated=work/'relocated';prefix.rename(relocated)
    installed=relocated/'bin'/h.compiler.name
    for name in ['json.void','error.void','convert.void']:
        assert (relocated/'share/void/stdlib'/name).read_bytes()==(h.ROOT/'stdlib'/name).read_bytes()
    env=dict(os.environ);env.pop('VOID_STDLIB_ROOT',None)
    expected=b'0: 10\n1: 20\n2: 30\n{"hp":100}\nhp 100\n{"name":"Ada","hp":7,"levels":[1,2]}\n[1,2,3]\n"blue"\n18446744073709551615\n18446744073709551615\n'
    for opt in [False,True]:
        flags=[] if opt else ['--no-opt']
        h.run([installed,'run',src,*flags],src,expected,env=env)
        native=work/('installed-O2' if opt else 'installed-O0')
        h.run([installed,'build',src,'-o',native,*flags],src,env=env)
        h.run([native],src,expected,env=env)
print(f'JSON relocated install: {len(h.records)} processes passed')
