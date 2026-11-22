"""Qualified wrapper result types and cold/warm editor recovery.

qualified_types.py voidls voidc [raylib-project]
The optional real raylib project is read only; all editor changes use overlays.
"""
from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory

from editor_recovery import request, update, roles, query
from member_completion import InteractiveClient

SOURCE = (Path(__file__).parent / 'fixtures/qualified_types.void').read_text()
TYPES = {'vector2', 'rectangle', 'color'}


def verify(server, root, main, good, target):
    disk = main.read_bytes()
    for warm in (False, True):
        with InteractiveClient(server, root, project=True) as client:
            initialized = request(client, 'initialize', {'capabilities': {'textDocument': {
                'semanticTokens': {'requests': {'full': True}, 'formats': ['relative']}}},
                'initializationOptions': {'voidInteractivePresentation': True}})
            legend = initialized['capabilities']['semanticTokensProvider']['legend']['tokenTypes']
            client.notify('initialized', {}); client.sync()
            version = 0

            def replace(text):
                nonlocal version
                version += 1
                return update(client, main, text, version)

            def complete(text, needle):
                result = query(client, 'completion', main, text, needle, after=True)
                return {item['label'] for item in (result['items'] if isinstance(result, dict) else result)}

            if warm:
                assert not replace(good)
            prefix = good[:good.index('func main')]
            for expression, needle, wanted in [('rl.', 'rl.', TYPES),
                    ('if rl.', 'if rl.', TYPES), ('for not rl.', 'not rl.', TYPES),
                    ('rl.vec', 'rl.vec', {'vector2'}), ('point.', 'point.', {'x', 'y'})]:
                text = prefix + 'func main():\n    var point: rl.vector2 = rl.vector2(3, 4)\n    ' + expression + '\n'
                assert replace(text), (warm, expression)
                assert wanted <= complete(text, '    ' + needle if needle == 'rl.' else needle), (warm, expression)
                tokens = roles(client, main, text, legend)
                assert any(word == 'point' and role == 'variable' for _, word, role in tokens), (warm, expression, tokens)
                assert any(word == 'vector2' and role == 'struct' for _, word, role in tokens), (warm, expression, tokens)
                assert client.process.poll() is None
            annotations = [(good.replace('value: rl.vector2', 'value: rl.', 1), 'value: rl.'),
                           (good.replace('local: rl.vector2', 'local: rl.', 1), 'local: rl.')]
            for colon in (':', ''):
                for suffix in ('', 'vec'):
                    annotations.append((good.replace('-> rl.vector2:', '-> rl.' + suffix + colon, 1),
                                        '-> rl.' + suffix))
                # A parameter probe also needs a retained declaration when its
                # final colon has not yet been written.
                annotations.append((good.replace('value: rl.vector2', 'value: rl.', 1)
                                    .replace('-> rl.vector2:', '-> rl.vector2' + colon, 1), 'value: rl.'))
            for text, needle in annotations:
                assert replace(text)
                wanted = {'vector2'} if needle.endswith('vec') else TYPES
                assert wanted <= complete(text, needle), (warm, needle, text)
            # A newly authored header may have neither a colon nor a body yet.
            for colon in (':', ''):
                text = good[:good.index('    var local')].replace('-> rl.vector2:', '-> rl.' + colon, 1)
                assert replace(text)
                assert TYPES <= complete(text, '-> rl.'), (warm, colon)
            assert not replace(good)
            definition = query(client, 'definition', main, good, 'value: rl.vector', after=True)
            assert definition['uri'].endswith(target), definition
            assert request(client, 'void/projectState', {})['status'] == 'ready'
            assert client.process.poll() is None
            client.shutdown()
    assert main.read_bytes() == disk


def run(server, compiler, supplied=None):
    with TemporaryDirectory(prefix='VOID qualified types ') as temp:
        root = Path(temp)
        (root / 'void.project.toml').write_text('schema=1\n[project]\nid="org.test.qualified"\nname="qualified"\nversion="1.0.0"\nkind="application"\n[application]\nid="org.test.qualified"\n')
        (root / 'records.void').write_text('value struct vector2:\n    x: f32\n    y: f32\n'
            'value struct rectangle:\n    x: f32\n    y: f32\n    width: f32\n    height: f32\n'
            'value struct color:\n    r: u8\n    g: u8\n    b: u8\n    a: u8\n')
        (root / 'api.void').write_text('use "./records.void" as native\n'
            'func vector2(x: f32 = 0, y: f32 = 0) -> native.vector2:\n    return native.vector2(x = x, y = y)\n'
            'func rectangle(x: f32 = 0, y: f32 = 0, width: f32 = 0, height: f32 = 0) -> native.rectangle:\n    return native.rectangle(x = x, y = y, width = width, height = height)\n'
            'func color(r: u8 = 0, g: u8 = 0, b: u8 = 0, a: u8 = 255) -> native.color:\n    return native.color(r = r, g = g, b = b, a = a)\n')
        main = root / 'main.void'
        good = SOURCE.replace('use pkg/raylib as rl', 'use "./api.void" as rl')
        main.write_text(good)
        for command in ('check', 'run'):
            result = subprocess.run([compiler, command, '--project', root], capture_output=True, text=True, timeout=30)
            assert result.returncode == 0, result.stderr
        # Editor recovery must not make the same source legal to the compiler.
        probe = root / 'strict.void'
        for ending, code in [('rl.', 'VOID1102'), ('rl.:', 'VOID1102'), ('rl.vec', 'VOID1101')]:
            broken = good.replace('-> rl.vector2:', '-> ' + ending, 1)
            probe.write_text(broken)
            result = subprocess.run([compiler, 'check', '--project', root, '--entry', probe.name],
                                    capture_output=True, text=True, timeout=30)
            assert result.returncode == 1 and code in result.stderr, result.stderr
            assert probe.read_text() == broken
        probe.unlink()
        verify(server, root, main, good, '/records.void')
    if supplied:
        root = Path(supplied).resolve()
        verify(server, root, root / 'app.void', SOURCE, '/raylib_native.void')
    print('qualified types: compiler/runtime, canonical definitions, cold/warm tokens, expression/type completion and server recovery passed')


if __name__ == '__main__':
    run(Path(sys.argv[1]).resolve(), str(Path(sys.argv[2]).resolve()), *sys.argv[3:])
