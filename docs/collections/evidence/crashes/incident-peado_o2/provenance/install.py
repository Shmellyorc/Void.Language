"""Installed and relocated compiler/tooling source discovery and native execution."""
from pathlib import Path
import os
import sys
import tempfile
import support as h
compiler = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='VOID-collections-installed-') as directory:
    work = Path(directory); prefix = work / 'original'
    for component in ('compiler', 'tooling'):
        h.run(['cmake', '--install', compiler.parent, '--prefix', prefix, '--component', component], cwd=work)
    relocated = work / 'relocated 雪 # %'; prefix.rename(relocated)
    installed = relocated / 'bin' / compiler.name
    library = relocated / 'share/void/stdlib'
    source = work / 'main.void'; source.write_bytes((h.ROOT / 'examples/collections_stdlib/main.void').read_bytes())
    for name in ('collections.void', 'error.void'):
        assert (library / name).read_bytes() == (h.ROOT / 'stdlib' / name).read_bytes(), name
    env = dict(os.environ); env.pop('VOID_STDLIB_ROOT', None)
    expected = '1 1 2 3\nenemy hero\na é 雪\n10 20\ntrue\n14 6\n'.encode()
    for switches in (['--no-opt'], []):
        h.run([installed, 'check', source], cwd=work, env=env, small=True)
        h.run([installed, 'run', source, '--no-cache', *switches], cwd=work, env=env, expected=expected, small=True)
        built = work / 'built.app'
        h.run([installed, 'build', source, '-o', built, *switches], cwd=work, env=env, small=True)
        h.run([built], cwd=work, env=env, expected=expected, small=True, sources=[source])
        obj = work / 'emitted.o'; exe = work / 'linked.app'
        h.run([installed, 'emit', source, '-o', obj, *switches], cwd=work, env=env, small=True)
        h.link(installed, obj, exe, cwd=work, env=env, sources=[source])
        h.run([exe], cwd=work, env=env, expected=expected, small=True, sources=[source])
    env['VOID_TEST_STDLIB'] = str(library)
    lsp = relocated / 'bin' / ('voidls.exe' if os.name == 'nt' else 'voidls')
    h.run([sys.executable, Path(__file__).with_name('lsp.py'), lsp], cwd=work, env=env, sources=[source], context={'mode': 'installed-lsp'})
print('Collections installed/relocated check, JIT/O2, build, independent AOT and LSP passed')
