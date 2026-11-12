"""Compiler-owned presentation while a current project source is incomplete."""
from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from lsp_client import Client

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


if __name__ == '__main__':
    run(Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve())
