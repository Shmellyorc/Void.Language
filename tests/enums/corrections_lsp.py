"""Generic enum declarations stay symbolic; selected uses stay concrete."""
from pathlib import Path
import json
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from lsp_client import Client

DECLARATIONS = '''## A generic packet.
enum packet<t>:
    ## No data.
    empty
    ## A packet with its original payload type.
    ## @param data Stored payload.
    value(data: t)
## A numeric generic code.
enum code<t>(u8):
    ## A successful code.
    ok = 3
## Generic access bits.
flags enum access<t>(u8):
    ## Read permission.
    read
'''
FIRST = '    var input: i32 = 7\n    var first = packet<i32>.value(input)\n'
SECOND = '    var second = packet<string>.value("ok")\n'
TAIL = '    var number = code<i32>.ok\n    var mask = access<string>.read\n'


def point(text, needle, delta=0):
    prefix = text[:text.index(needle) + delta]
    return dict(line=prefix.count('\n'), character=len(prefix.rsplit('\n', 1)[-1]))


records = []
with tempfile.TemporaryDirectory(prefix='VOID G78 template metadata ') as directory:
    file = Path(directory) / 'main.void'
    initial = DECLARATIONS + 'func main():\n    pass\n'
    file.write_text(initial)
    with Client(Path(sys.argv[1]).resolve(), file) as client:
        response, _ = client.request('initialize', {'capabilities': {'textDocument': {
            'publishDiagnostics': {'versionSupport': True},
            'semanticTokens': {'requests': {'full': True}, 'formats': ['relative'],
                               'tokenTypes': ['enum', 'enumMember', 'struct'], 'tokenModifiers': []}}}})
        legend = response['result']['capabilities']['semanticTokensProvider']['legend']['tokenTypes']
        client.notify('initialized', {})
        client.sync()
        uri = file.as_uri()
        client.notify('textDocument/didOpen', {'textDocument': {'uri': uri, 'version': 1, 'languageId': 'void', 'text': initial}})
        assert all(not note['diagnostics'] for note in client.sync())

        def request(method, text=None, needle=None, delta=0):
            params = {'textDocument': {'uri': uri}}
            if needle is not None:
                params['position'] = point(text, needle, delta)
            result, _ = client.request('textDocument/' + method, params)
            records.append(dict(method=method, response=result))
            return result['result']

        def hover(text, needle, delta=0):
            return request('hover', text, needle, delta)['contents']['value']

        def verify(text, instantiated):
            symbols = request('documentSymbol')
            for name in ['packet', 'code', 'access']:
                matches = [symbol for symbol in symbols if symbol['name'] == name]
                assert len(matches) == 1 and matches[0]['kind'] == 10, matches
            for name in ['empty', 'value', 'ok', 'read']:
                matches = [symbol for symbol in symbols if symbol['name'] == name]
                assert len(matches) == 1 and matches[0]['kind'] == 22, matches
            assert 'enum packet<t>' in hover(text, 'packet<t>', 2)
            assert 'enum code<t>(u8)' in hover(text, 'code<t>', 2)
            assert 'flags enum access<t>(u8)' in hover(text, 'access<t>', 2)
            declaration = hover(text, 'value(data:', 2)
            assert 'value(data: t) -> packet<t>' in declaration and 'i32' not in declaration, declaration
            assert 'code<t>.ok' in hover(text, 'ok =', 1)
            data = request('semanticTokens/full')['data']
            roles = {}
            line = character = 0
            for index in range(0, len(data), 5):
                dl, dc, length, kind, modifiers = data[index:index+5]
                line += dl
                character = dc if dl else character + dc
                roles[(line, character)] = legend[kind]
            for needle in ['packet<t>', 'code<t>', 'access<t>']:
                location = point(text, needle)
                assert roles.get((location['line'], location['character'])) == 'enum', roles
            if instantiated:
                for spelling, expected in [('packet<i32>.value(input)', 'value(data: i32) -> packet<i32>'),
                                           ('packet<string>.value("ok")', 'value(data: string) -> packet<string>')]:
                    assert expected in hover(text, spelling, len(spelling.split('.')[0])+2)
                    location = point(text, spelling)
                    assert roles.get((location['line'], location['character'])) == 'enum', roles
                assert 'code<i32>.ok' in hover(text, 'code<i32>.ok', len('code<i32>.')+1)
                signature = request('signatureHelp', text, 'value(input)', len('value(')+1)
                assert signature['activeParameter'] == 0 and 'data: i32' in str(signature), signature
                assert 'Stored payload' in str(signature), signature

        verify(initial, False)
        final = None
        for version, statements in enumerate([FIRST+SECOND+TAIL, SECOND+FIRST+TAIL], 2):
            final = DECLARATIONS + 'func main():\n' + statements
            client.notify('textDocument/didChange', {'textDocument': {'uri': uri, 'version': version}, 'contentChanges': [{'text': final}]})
            notes = client.sync()
            assert all(not note['diagnostics'] for note in notes), notes
            verify(final, True)
        broken = final.replace('packet<string>.value("ok")', 'packet<string>.value(7)')
        for version, text, invalid in [(4, broken, True), (5, final, False)]:
            client.notify('textDocument/didChange', {'textDocument': {'uri': uri, 'version': version}, 'contentChanges': [{'text': text}]})
            notes = client.sync()
            assert any(note['diagnostics'] for note in notes) == invalid, notes
        verify(final, True)
        client.shutdown()
if len(sys.argv) > 2:
    Path(sys.argv[2]).write_text(json.dumps(records, indent=2)+'\n')
print('G78 symbolic enum/case declarations, enum token/symbol roles, concrete uses, ordering and edit recovery passed')
