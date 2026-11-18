"""Compiler-owned presentation while a current project source is incomplete."""
from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from lsp_client import Client, frame

FIXTURES = Path(__file__).parent / 'fixtures' / 'editor_recovery'
BROKEN = (FIXTURES / 'main.void').read_text(encoding='utf-8')
GOOD = BROKEN.replace('if game.\n', 'if game.is_running:\n')
MEMBERS = {'game_over', 'is_running', 'reset', 'running', 'score', 'tries', 'value'}


def request(client, method, params):
    response, _ = client.request(method, params)
    assert 'result' in response, response
    return response['result']


def query(client, method, path, text=None, needle=None, after=False):
    params = {'textDocument': {'uri': path.as_uri()}}
    if needle is not None:
        offset = text.index(needle) + (len(needle) if after else 0)
        before = text[:offset]
        params['position'] = {'line': before.count('\n'),
                              'character': len(before.rsplit('\n', 1)[-1].encode('utf-16-le')) // 2}
    return request(client, 'textDocument/' + method, params)


def update(client, path, text, version):
    doc = {'uri': path.as_uri(), 'version': version}
    if version == 1:
        client.notify('textDocument/didOpen', {'textDocument': dict(doc, languageId='void', text=text)})
    else:
        client.notify('textDocument/didChange', {'textDocument': doc, 'contentChanges': [{'text': text}]})
    return [d for note in client.sync() if note['uri'] == path.as_uri() for d in note['diagnostics']]


def labels(client, path, text, needle):
    return {item['label'] for item in query(client, 'completion', path, text, needle, after=True)}


def roles(client, path, text, legend):
    data = query(client, 'semanticTokens/full', path)['data']
    lines = text.splitlines()
    line = column = 0
    decoded = set()
    assert len(data) % 5 == 0
    for n in range(0, len(data), 5):
        delta, start, width, kind, _ = data[n:n+5]
        line += delta
        column = start if delta else column + start
        content = lines[line].encode('utf-16-le')
        word = content[column*2:(column+width)*2].decode('utf-16-le')
        decoded.add((line, word, legend[kind]))
    return decoded


def run(server, compiler):
    with TemporaryDirectory(prefix='VOID editor recovery 雪 ') as temp:
        root = Path(temp)
        main, app = root / 'main.void', root / 'app.void'
        main.write_bytes(GOOD.encode())
        app.write_bytes((FIXTURES / 'app.void').read_bytes())
        (root / 'void.project.toml').write_bytes((FIXTURES / 'void.project.toml').read_bytes())
        with Client(server, root, project=True) as client:
            capabilities = {'textDocument': {'semanticTokens': {'requests': {'full': True}, 'formats': ['relative'],
                                                               'tokenTypes': ['variable'], 'tokenModifiers': []},
                                             'publishDiagnostics': {'versionSupport': True}}}
            result = request(client, 'initialize', {'capabilities': capabilities})
            legend = result['capabilities']['semanticTokensProvider']['legend']['tokenTypes']
            client.notify('initialized', {})
            client.sync()
            assert not update(client, main, GOOD, 1)
            initial = roles(client, main, GOOD, legend)
            assert (8, 'game', 'variable') in initial
            assert labels(client, main, GOOD, 'if game.') == MEMBERS

            errors = update(client, main, BROKEN, 2)
            assert any(d['code'] == 'VOID1102' and d['range']['start'] == {'line': 14, 'character': 20} for d in errors), errors
            assert any(d['code'] == 'VOID1101' and "found '<indent>'" in d['message'] for d in errors), errors
            assert request(client, 'void/projectState', {})['status'] == 'error'
            assert labels(client, main, BROKEN, 'if game.') == MEMBERS
            recovered = roles(client, main, BROKEN, legend)
            assert recovered == initial - {(14, 'is_running', 'property')}, (initial, recovered)
            hover = query(client, 'hover', main, BROKEN, 'game =')
            assert hover['contents']['value'] == 'game: /app.void::app', hover
            definition = query(client, 'definition', main, BROKEN, 'app()')
            assert definition['uri'] == app.as_uri(), definition
            assert request(client, 'textDocument/formatting', {'textDocument': {'uri': main.as_uri()},
                                                               'options': {'tabSize': 4, 'insertSpaces': True}}) == []
            assert main.read_bytes() == GOOD.encode()

            partial = BROKEN.replace('if game.\n', 'if game.is_\n')
            assert update(client, main, partial, 3)
            assert labels(client, main, partial, 'if game.is_') == {'is_running'}
            assert roles(client, main, partial, legend) == recovered

            # The unrelated declaration is unused, so it cannot supply a type
            # or shadow a reference in the recovered graph.
            unrelated = BROKEN.replace('        else:', '        else:\n            var unfinished =')
            errors = update(client, main, unrelated, 4)
            assert any(d['range']['start']['line'] == 17 for d in errors), errors
            assert labels(client, main, unrelated, 'if game.') == MEMBERS
            assert (8, 'game', 'variable') in roles(client, main, unrelated, legend)
            assert not labels(client, main, unrelated, 'print('), 'Uncertain bindings cannot grant plain name completion'

            unbound = BROKEN.replace('if game.\n', 'if missing.\n')
            assert update(client, main, unbound, 5)
            assert not labels(client, main, unbound, 'if missing.')
            shadow = BROKEN.replace('            if game.', '            var game =\n            if game.')
            assert update(client, main, shadow, 6)
            assert not labels(client, main, shadow, 'if game.')
            assert not roles(client, main, shadow, legend)

            body = (FIXTURES / 'app.void').read_text(encoding='utf-8').replace(
                '    score: i32', '    private secret: i32\n    score: i32')
            body += '\nprivate func (a app) hide():\n    pass\n'
            assert not update(client, app, body, 1)
            nested = GOOD.replace('                pass', '                match p:\n'
                                  '                    _:\n                        print("雪😀", game.score)')
            nested = nested.replace('        else:\n', '        else:\n            print(game.score)\n')
            assert not update(client, main, nested, 7)
            for needle in ('for game.', 'if game.', 'print("雪😀", game.', 'print(game.'):
                assert labels(client, main, nested, needle) == MEMBERS, needle
            assert (17, 'score', 'property') in roles(client, main, nested, legend)
            assert update(client, main, BROKEN, 8)
            assert labels(client, main, BROKEN, 'if game.') == MEMBERS

            denied = BROKEN.replace('use "/app.void"', 'use pkg/undeclared')
            errors = update(client, main, denied, 9)
            assert errors and not labels(client, main, denied, 'if game.')
            assert not roles(client, main, denied, legend)
            assert not update(client, main, GOOD, 10)
            assert roles(client, main, GOOD, legend) == initial
            client.notify('textDocument/didChange', {'textDocument': {'uri': main.as_uri(), 'version': 9},
                                                     'contentChanges': [{'text': denied}]})
            assert roles(client, main, GOOD, legend) == initial
            assert update(client, main, BROKEN, 11)
            assert roles(client, main, BROKEN, legend)
            client.notify('textDocument/didClose', {'textDocument': {'uri': main.as_uri()}})
            client.sync()
            assert not roles(client, main, BROKEN, legend)

            other = root / 'other.void'
            other_text = 'func main():\n    var different = 17\n    if different == 17:\n        pass\n'
            other.write_bytes(other_text.encode())
            assert request(client, 'void/reselectProject', {'entry': 'other.void'})['status'] == 'ready'
            assert not update(client, other, other_text, 1)
            assert (2, 'different', 'variable') in roles(client, other, other_text, legend)
            assert not roles(client, main, BROKEN, legend)
            client.shutdown()

        (root / 'other.void').unlink()
        main.write_bytes(BROKEN.encode())
        result = subprocess.run([compiler, 'check', '--project', root], capture_output=True, timeout=30)
        assert result.returncode and b'VOID1102' in result.stderr and b'VOID1101' in result.stderr, result.stderr
        print('editor recovery: nested members, visibility, UTF-16, tokens, diagnostics, revision, switch and strict CLI passed')


def run_members(server, compiler):
    with TemporaryDirectory(prefix='VOID member edits é😀 ') as temp:
        parent = Path(temp)
        root, package = parent / 'app', parent / 'maths'

        def cli(*args):
            subprocess.run([compiler, *args], check=True, capture_output=True, timeout=30)

        cli('new', root)
        cli('new', package, '--kind', 'library', '--package-name', 'maths')
        large = ''.join(f'public func api_{n:03}() -> i32:\n    return {n}\n' for n in range(270))
        (package / 'src/maths.void').write_text(large + 'public func z_final() -> i32:\n    return 42\n', encoding='utf-8')
        cli('add', 'maths', '--path', package, '--project', root)
        path = root / 'src/app.void'
        (root / 'src/shapes.void').write_text(
            'public value struct point:\n    public x: i32\n    private hidden: i32\n'
            'public func (p point) shift() -> i32:\n    return p.x\n'
            'public func create() -> point:\n    return point()\n', encoding='utf-8')
        good = '''use "./shapes.void" as shapes
use pkg/maths as library

value struct local_point:
    x: i32

func main() -> i32:
    var local = local_point()
    var imported = shapes.point()
    print("é😀", local.x, imported.shift())
    return library.z_final()
'''
        path.write_text(good, encoding='utf-8')
        with Client(server, root, project=True) as client:
            capabilities = {'textDocument': {'semanticTokens': {'requests': {'full': True},
                'formats': ['relative'], 'tokenTypes': ['variable'], 'tokenModifiers': []}}}
            legend = request(client, 'initialize', {'capabilities': capabilities})[
                'capabilities']['semanticTokensProvider']['legend']['tokenTypes']
            client.notify('initialized', {})
            client.sync()
            assert not update(client, path, good, 1)
            stable = roles(client, path, good, legend)
            expected = {(4, 'x', 'property'), (7, 'local_point', 'struct'), (8, 'shapes', 'namespace'),
                        (8, 'point', 'struct'), (9, 'x', 'property'), (9, 'shift', 'method')}
            assert expected <= stable, stable
            completion = query(client, 'completion', path, good, 'library.z_', after=True)
            final = next(item for item in completion if item['label'] == 'z_final')
            assert final['textEdit'] == {'newText': 'z_final', 'range': {
                'start': {'line': 10, 'character': 19}, 'end': {'line': 10, 'character': 26}}}, final
            version = 1
            for base, name, full in [('local', 'x', 'x'), ('imported', 'shift', 'shift()'),
                                     ('shapes', 'create', 'create()'), ('library', 'z_final', 'z_final()')]:
                for selector in ('.', '.' + (name[:2] if len(name) > 2 else ''), '.' + full):
                    # Use the same UTF-16 line before and after the edit and
                    # check that the compiler retains unrelated bindings.
                    current = good.replace('    return', f'    {base}{selector}\n    return')
                    version += 1
                    diagnostics = update(client, path, current, version)
                    assert bool(diagnostics) == (selector != '.' + full or full == 'x'), (base, selector, diagnostics)
                    tokens = roles(client, path, current, legend)
                    assert expected <= tokens and (11, 'z_final', 'function') in tokens, tokens
                    cursor = selector[:-2] if selector.endswith('()') else selector
                    when = f'    {base}{cursor}'
                    if selector != '.' + full:
                        for _ in range(3):
                            candidates = labels(client, path, current, when)
                            assert name in candidates and 'hidden' not in candidates, candidates
            mixed = good.replace('    return', '\tlibrary.\n    return')
            version += 1
            assert any(d['code'] == 'VOID1002' for d in update(client, path, mixed, version))
            tokens = roles(client, path, mixed, legend)
            assert expected <= tokens and (10, 'library', 'namespace') in tokens
            assert 'z_final' in labels(client, path, mixed, '\tlibrary.')
            version += 1
            assert not update(client, path, good, version)
            assert roles(client, path, good, legend) == stable
            client.shutdown()
        run_interactive(server, root, path, good)
        path.write_text(mixed, encoding='utf-8')
        result = subprocess.run([compiler, 'check', '--project', root], capture_output=True, timeout=30)
        assert result.returncode and b'VOID1002' in result.stderr, result.stderr
        print('member editing: local values, imported types and methods, large installed package, tabs, UTF-16 and strict compiler checks passed')


class InteractiveClient(Client):
    def receive(self):
        while True:
            message = super().receive()
            if message.get('method') != 'workspace/semanticTokens/refresh':
                return message
            self.write(frame({'jsonrpc': '2.0', 'id': message['id'], 'result': None}))


def run_interactive(server, root, path, original):
    valid = original.replace('    return library.z_final()', '    local.x -= 1\n    return library.z_final()')
    cases = [
        ('VOID2001', '    local.x -= 1', '    local.x -= 1.0'),
        ('VOID2002', '    local.x -= 1', '    missing.x -= 1'),
        ('VOID1101', '    local.x -= 1', '    local.x -='),
        ('VOID1002', '    local.x -= 1', '\tlocal.x -= 1'),
    ]
    important = {(7, 'local_point', 'struct'), (8, 'shapes', 'namespace'), (8, 'point', 'struct'),
                 (9, 'shift', 'method'), (11, 'z_final', 'function')}

    def initialize(client):
        caps = {'textDocument': {'semanticTokens': {'requests': {'full': True}, 'formats': ['relative'],
                  'tokenTypes': ['variable'], 'tokenModifiers': []}},
                'workspace': {'semanticTokens': {'refreshSupport': True}}}
        legend = request(client, 'initialize', {'capabilities': caps,
                    'initializationOptions': {'voidInteractivePresentation': True, 'voidGuardedFixes': True}})[
                        'capabilities']['semanticTokensProvider']['legend']['tokenTypes']
        client.notify('initialized', {})
        client.sync()
        return legend

    path.write_text(valid, encoding='utf-8')
    # Cold-open each invalid document. Coloring must come from checked current
    # source even when there is no earlier token response to project forward.
    for code, before, after in cases:
        broken = valid.replace(before, after)
        path.write_text(broken, encoding='utf-8')
        with InteractiveClient(server, root, project=True) as client:
            legend = initialize(client)
            assert any(d['code'] == code for d in update(client, path, broken, 1)), code
            color = roles(client, path, broken, legend)
            assert important <= color, (code, color)
            assert not update(client, path, valid, 2)
            assert important <= roles(client, path, valid, legend)
            client.shutdown()

    path.write_text(valid, encoding='utf-8')
    with InteractiveClient(server, root, project=True) as client:
        legend = initialize(client)
        assert not update(client, path, valid, 1)
        assert important <= roles(client, path, valid, legend)
        version = 1
        for prefix in ('.', '.z', '.z_', '.', '.api_', '.z_f'):
            text = valid.replace('    local.x -= 1', f'    library{prefix}')
            version += 1
            client.notify('textDocument/didChange', {'textDocument': {'uri': path.as_uri(), 'version': version},
                          'contentChanges': [{'text': text}]})
            request(client, 'textDocument/codeAction', {'textDocument': {'uri': path.as_uri()},
                    'context': {'diagnostics': [], 'triggerKind': 2},
                    'range': {'start': {'line': 10, 'character': 4}, 'end': {'line': 10, 'character': 4}}})
            color = roles(client, path, text, legend)
            assert important <= color, color
            found = labels(client, path, text, f'    library{prefix}')
            assert ('api_269' if prefix=='.api_' else 'z_final') in found, found
        assert not update(client, path, valid, version + 1)
        assert important <= roles(client, path, valid, legend)
        # Package disk changes must invalidate the previous completion catalog.
        installed = root / 'libs/maths/src/maths.void'
        original = installed.read_bytes()
        try:
            installed.write_bytes(original + b'\n# invalidates the recorded tree\n')
            client.notify('workspace/didChangeWatchedFiles', {'changes': [{'uri': installed.as_uri(), 'type': 2}]})
            text = valid.replace('    local.x -= 1', '    library.')
            client.notify('textDocument/didChange', {'textDocument': {'uri': path.as_uri(), 'version': version + 2},
                          'contentChanges': [{'text': text}]})
            assert not labels(client, path, text, '    library.')
        finally:
            installed.write_bytes(original)
        client.notify('workspace/didChangeWatchedFiles', {'changes': [{'uri': installed.as_uri(), 'type': 2}]})
        assert not update(client, path, valid, version + 3)
        assert important <= roles(client, path, valid, legend)
        client.shutdown()
    print('interactive semantic tokens: cold type, name, syntax and indentation errors, repairs and rapid completion passed')


if __name__ == '__main__':
    arguments = tuple(Path(argument).resolve() for argument in sys.argv[1:3])
    run(*arguments)
    run_members(*arguments)
