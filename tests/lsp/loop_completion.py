"""Incomplete loop conditions must retain member assistance on a cold server.

Use read-only repository fixtures and open-document overlays: no copied project,
package installation or source-file rewrite can hide a startup-state regression.
"""
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from lsp_client import Client, frame


class InteractiveClient(Client):
    def receive(self):
        while True:
            message = super().receive()
            if message.get('method') != 'workspace/semanticTokens/refresh':
                return message
            self.write(frame({'jsonrpc': '2.0', 'id': message['id'], 'result': None}))


def run(server):
    root = Path(__file__).parent / 'fixtures/editor_recovery'
    main = root / 'main.void'
    original = main.read_bytes()
    prefix = 'use "/app.void" as rl\n\nfunc main():\n    var game = rl.app()\n'
    cases = [
        ('for rl.', 'app', None),
        ('for rl.ap', 'app', None),
        ('for game.', 'is_running', None),
        ('for game.is_', 'is_running', None),
        ('for not rl.', 'app', None),
        ('for not game.is_', 'is_running', None),
        ('for game.', 'is_running', '        pass\n'),
        ('for game.:', 'is_running', '        pass\n'),
        ('if game.', 'is_running', None),
        ('for missing.', None, None),
    ]
    for warm in (False, True):
        for header, expected, body in cases:
            with InteractiveClient(server, root, project=True) as client:
                response, _ = client.request('initialize', {'capabilities': {},
                    'initializationOptions': {'voidInteractivePresentation': True}})
                assert 'result' in response, response
                client.notify('initialized', {})
                client.sync()
                version = 1
                valid = prefix + '    for game.is_running:\n        game.game_over()\n'

                def update(text):
                    nonlocal version
                    doc = {'uri': main.as_uri(), 'version': version}
                    if version == 1:
                        client.notify('textDocument/didOpen', {
                            'textDocument': dict(doc, languageId='void', text=text)})
                    else:
                        client.notify('textDocument/didChange', {
                            'textDocument': doc, 'contentChanges': [{'text': text}]})
                    version += 1
                    return [d for n in client.sync() for d in n['diagnostics']
                            if n['uri'] == main.as_uri()]

                if warm:
                    assert not update(valid)
                text = prefix + '    ' + header + '\n' + (body or '')
                assert update(text), 'ordinary diagnostics must reject incomplete source'
                needle = header.rstrip(':')
                offset = text.index(needle) + len(needle)
                before = text[:offset]
                response, _ = client.request('textDocument/completion', {
                    'textDocument': {'uri': main.as_uri()},
                    'position': {'line': before.count('\n'),
                                 'character': len(before.rsplit('\n', 1)[-1])}})
                assert 'result' in response, response
                items = response['result']
                if isinstance(items, dict):
                    items = items['items']
                labels = {item['label'] for item in items}
                if expected:
                    assert expected in labels, (warm, header, labels)
                else:
                    assert not labels, (warm, header, labels)
                if header == 'for rl.':
                    # Manifest-mode graphs must wire the type probe as well
                    # as expression completion. The second hole must not make
                    # an otherwise known qualified type disappear.
                    for statement, needle in [
                        ('var typed = rl.app() as rl.', 'as rl.'),
                        ('var typed: rl. = rl.app()', ': rl.'),
                    ]:
                        text = prefix + '    ' + statement + '\n    for rl.\n'
                        assert update(text)
                        before = text[:text.index(needle) + len(needle)]
                        response, _ = client.request('textDocument/completion', {
                            'textDocument': {'uri': main.as_uri()},
                            'position': {'line': before.count('\n'),
                                         'character': len(before.rsplit('\n', 1)[-1])}})
                        assert 'result' in response, response
                        result = response['result']
                        if isinstance(result, dict):
                            result = result['items']
                        assert 'app' in {item['label'] for item in result}, (warm, statement, result)
                assert not update(valid), 'finishing the condition must repair the project'
                client.shutdown()
    assert main.read_bytes() == original
    print('loop completion: cold/warm aliases, values, prefixes, bodies, unknown names and repairs passed')


if __name__ == '__main__':
    run(Path(sys.argv[1]).resolve())
