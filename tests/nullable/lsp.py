"""G79 nullable presentation through real voidls, including unsaved recovery."""
from pathlib import Path
import json
import sys
import tempfile
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from lsp_client import Client

SOURCE = '''struct player:
    health: i32
## Receive optional gameplay values.
## @param health Optional health; zero is present.
## @param label Optional label; empty is present.
## @param items Optional names; empty is present.
## @param owner Optional player.
## @return The supplied health or null.
func update(health: i32?, label: string?, items: [string]?, owner: player?) -> i32?:
    return health
## Preserve nullable generic values.
## @param value Optional payload.
## @return Original payload or null.
func echo<t>(value: t?) -> t?:
    return value
func main() -> i32:
    var health: i32? = null
    var nickname: string? = "Boss"
    var items: [string]? = null
    var actor: player? = player(health = 7)
    var result = update(health, nickname, items, actor)
    var generic = echo(health)
    if actor is Ok(var present):
        return present.health - 7
    return 1
'''

def point(text, needle, delta=0):
    prefix = text[:text.index(needle) + delta]
    return dict(line=prefix.count('\n'), character=len(prefix.rsplit('\n', 1)[-1]))

records = []
evidence = Path(sys.argv[2]).resolve()
def record(kind, value):
    records.append(dict(kind=kind, value=value))
    evidence.parent.mkdir(parents=True, exist_ok=True)
    evidence.write_text(json.dumps(records, indent=2) + '\n')

with tempfile.TemporaryDirectory(prefix='VOID G79 editor ') as directory:
    file = Path(directory) / 'main.void'
    file.write_text(SOURCE)
    uri = file.as_uri()
    with Client(Path(sys.argv[1]).resolve(), file) as client:
        initialized, _ = client.request('initialize', {'capabilities': {'textDocument': {
            'publishDiagnostics': {'versionSupport': True, 'relatedInformation': True},
            'semanticTokens': {'requests': {'full': True}, 'formats': ['relative'],
                               'tokenTypes': ['type', 'struct', 'variable', 'function', 'parameter', 'property', 'typeParameter'],
                               'tokenModifiers': []}}}})
        assert 'semanticTokensProvider' in initialized['result']['capabilities'], initialized
        client.notify('initialized', {})
        client.sync()
        client.notify('textDocument/didOpen', {'textDocument': {'uri': uri, 'version': 1, 'languageId': 'void', 'text': SOURCE}})
        notes = client.sync(); record('initial diagnostics', notes)
        assert all(not note['diagnostics'] for note in notes), notes
        def request(method, text=SOURCE, needle=None, delta=0):
            params = {'textDocument': {'uri': uri}}
            if needle is not None:
                params['position'] = point(text, needle, delta)
            response, notes = client.request('textDocument/' + method, params)
            record(method, response)
            assert 'error' not in response, response
            return response['result']
        for needle, expected in [('var health:', 'i32?'), ('var nickname:', 'string?'), ('var items:', '[string]?'), ('var actor:', 'player?')]:
            hover = request('hover', needle=needle, delta=5)
            assert expected in str(hover) and '$nullable' not in str(hover) and '/@pkg/' not in str(hover), hover
        signature = request('signatureHelp', needle='update(health, nickname', delta=len('update(health, ')+2)
        assert signature['activeParameter'] == 1, signature
        for expected in ['health: i32?', 'label: string?', 'items: [string]?', 'owner: player?', '-> i32?', 'Optional label']:
            assert expected in str(signature), signature
        hover = request('hover', needle='echo<t>', delta=1)
        assert 'value: t?' in str(hover) and '-> t?' in str(hover), hover
        hover = request('hover', needle='echo(health)', delta=1)
        assert 'value: i32?' in str(hover) and '-> i32?' in str(hover), hover
        tokens = request('semanticTokens/full')
        assert tokens and tokens['data'], tokens
        symbols = request('documentSymbol')
        assert all('$nullable' not in row['name'] for row in symbols), symbols
        probe = SOURCE.replace('return present.health - 7', 'return present.')
        client.notify('textDocument/didChange', {'textDocument': {'uri': uri, 'version': 2}, 'contentChanges': [{'text': probe}]})
        client.sync()
        completion = request('completion', probe, 'return present.', len('return present.'))
        items = completion if isinstance(completion, list) else completion['items']
        assert any(row['label']=='health' for row in items), completion
        invalid = SOURCE.replace('var health: i32? = null', 'var health: i32 = null').replace('var actor: player? = player(health = 7)', 'var actor: player? = 7')
        client.notify('textDocument/didChange', {'textDocument': {'uri': uri, 'version': 3}, 'contentChanges': [{'text': invalid}]})
        notes = client.sync(); record('invalid diagnostics', notes)
        assert any(note['diagnostics'] for note in notes), notes
        messages = [diagnostic['message'] for note in notes for diagnostic in note['diagnostics']]
        assert any('player?' in message for message in messages), messages
        assert all('.void::' not in message and '$nullable' not in message for message in messages), messages
        client.notify('textDocument/didChange', {'textDocument': {'uri': uri, 'version': 4}, 'contentChanges': [{'text': SOURCE}]})
        notes = client.sync(); record('recovered diagnostics', notes)
        assert all(not note['diagnostics'] for note in notes), notes
        recovered = request('signatureHelp', needle='update(health, nickname', delta=len('update(health, ')+2)
        assert recovered == signature, recovered
        readonly = """struct node:
    value: i32
value struct carrier:
    child: node
func need(value: carrier?):
    pass
func main() -> i32:
    const held: carrier? = carrier(child = node(value = 7))
    need(held)
    return 0
"""
        generic = readonly.replace('value struct carrier:', 'value struct carrier<t>:').replace('child: node', 'child: t').replace('carrier?', 'carrier<node>?').replace('carrier(child', 'carrier<node>(child')
        for version, source, logical in [(5, readonly, 'carrier?.child'), (6, generic, 'carrier<node>?.child')]:
            client.notify('textDocument/didChange', {'textDocument': {'uri': uri, 'version': version}, 'contentChanges': [{'text': source}]})
            notes = client.sync(); record('readonly diagnostics', notes)
            errors = [d for note in notes for d in note['diagnostics'] if d['code'] == 'VOID2007']
            assert errors and any(logical in d['message'] for d in errors), notes
            for diagnostic in errors:
                assert diagnostic['range']['start'] == {'line': 8, 'character': 4}, diagnostic
                messages = [diagnostic['message']] + [r['message'] for r in diagnostic['relatedInformation']]
                assert all(not any(hidden in message for hidden in ['$nullable', '$value', '$array:', '.void::']) for message in messages), messages
                locations = {(r['location']['range']['start']['line'], r['location']['range']['start']['character']) for r in diagnostic['relatedInformation']}
                assert {(3, 4), (4, 10)} <= locations, locations
        client.notify('textDocument/didChange', {'textDocument': {'uri': uri, 'version': 7}, 'contentChanges': [{'text': SOURCE}]})
        notes = client.sync(); record('readonly recovery', notes)
        assert any(note.get('version') == 7 and not note['diagnostics'] for note in notes), notes
        client.shutdown()
print('G79 LSP: nullable hover/signatures, generic metadata, semantic tokens, unwrapped completion and unsaved recovery passed')
