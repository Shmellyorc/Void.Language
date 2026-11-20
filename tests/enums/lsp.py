"""G78 uses compiler-owned enum symbols for editor assistance and diagnostics."""
from pathlib import Path
import json
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from lsp_client import Client

source = '''## Access bits for a document.
flags enum access(u8):
    ## Read the document.
    read
    ## Write the document.
    write
enum item<t>:
    ## A typed payload.
    ## @param data The stored value.
    value(data: t)
func main():
    var mask = access.read
    var selected = mask.has(access.write)
    var stored = item<i32>.value(7)
    var title = mask.name
'''

def point(text, needle, delta=0):
    prefix = text[:text.index(needle) + delta]
    return dict(line=prefix.count('\n'), character=len(prefix.rsplit('\n', 1)[-1]))

records = []
with tempfile.TemporaryDirectory(prefix='VOID G78 editor ') as directory:
    entry = Path(directory) / 'main.void'
    entry.write_text(source)
    with Client(Path(sys.argv[1]).resolve(), entry) as client:
        response, _ = client.request('initialize', {'capabilities': {'textDocument': {
            'publishDiagnostics': {'versionSupport': True},
            'semanticTokens': {'requests': {'full': True}, 'formats': ['relative'],
                               'tokenTypes': ['enum', 'enumMember', 'variable'], 'tokenModifiers': []}}}})
        assert 'semanticTokensProvider' in response['result']['capabilities'], response
        client.notify('initialized', {})
        client.sync()
        uri = entry.as_uri()
        def change(text, version):
            client.notify('textDocument/didChange', {'textDocument': {'uri': uri, 'version': version}, 'contentChanges': [{'text': text}]})
            return client.sync()
        def query(method, text, needle, delta=0):
            response, _ = client.request('textDocument/' + method, {'textDocument': {'uri': uri}, 'position': point(text, needle, delta)})
            records.append(dict(method=method, response=response))
            return response
        client.notify('textDocument/didOpen', {'textDocument': {'uri': uri, 'languageId': 'void', 'version': 1, 'text': source}})
        assert all(not note['diagnostics'] for note in client.sync())
        hover = query('hover', source, 'has(access', 1)
        assert 'mask' in str(hover) and 'zero' in str(hover).lower(), hover
        signature = query('signatureHelp', source, 'has(access', 4)
        assert 'mask: access' in str(signature) and signature['result']['activeParameter'] == 0, signature
        hover = query('hover', source, 'value(7)', 2)
        assert 'data: i32' in str(hover) and 'A typed payload' in str(hover), hover
        hover = query('hover', source, 'mask.name', 6)
        assert 'string' in str(hover) and 'Declared' in str(hover), hover
        incomplete = source.replace('var title = mask.name', 'var title = mask.')
        change(incomplete, 2)
        completion = query('completion', incomplete, 'mask.\n', 5)
        items = completion['result']['items'] if isinstance(completion['result'], dict) else completion['result']
        labels = {item['label'] for item in items}
        assert {'name', 'value', 'is_defined', 'has', 'has_any', 'with', 'without', 'toggled'} <= labels, items
        assert all('documentation' in item for item in items), items
        broken = source.replace('mask.has(access.write)', 'mask | 3')
        assert any(note['diagnostics'] for note in change(broken, 3))
        assert all(not note['diagnostics'] for note in change(source, 4))
        tokens, _ = client.request('textDocument/semanticTokens/full', {'textDocument': {'uri': uri}})
        assert tokens['result']['data'], tokens
        records.append(dict(method='semanticTokens/full', response=tokens))
        client.shutdown()
if len(sys.argv) > 2:
    Path(sys.argv[2]).write_text(json.dumps(records, indent=2) + '\n')
print('G78 enum hover, flags completion/docs, generic payloads, active parameter, diagnostics and semantic tokens passed')
