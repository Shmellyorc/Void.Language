"""Unused named labels from resolved local/imported callable metadata."""
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from lsp_client import Client

server = Path(sys.argv[1]).resolve()
API = '''## Combine three numbers.
## @param first First input.
## @param second Second input.
## @param final Last input.
func combine(first: i32, second: i32 = 2, final: i32 = 3) -> i32:
    return first + second + final
struct accumulator:
    initial: i32
## Combine numbers with an instance.
## @param first First input.
## @param second Second input.
## @param final Last input.
func (self accumulator) combine(first: i32, second: i32 = 2, final: i32 = 3) -> i32:
    return self.initial + first + second + final
## Combine with an explicitly selected type.
## @param first First input.
## @param second Second input.
## @param final Last input.
func generic_combine<t>(first: t, second: i32 = 2, final: i32 = 3) -> i32:
    return second + final
## Combine with a generic array.
## @param first First input.
## @param second Second input.
## @param final Last input.
func generic_array<t>(first: [t], second: i32 = 2, final: i32 = 3) -> i32:
    return second + final
'''
BASE = 'use "./helpers.void" as helpers\n' + API + '''
func main() -> i32:
    var first_value = 11
    var instance = accumulator(initial = 0)
    return combine(first_value)
'''

with tempfile.TemporaryDirectory(prefix='VOID named argument completion ') as scratch:
    root = Path(scratch)
    entry, helper = root / 'main.void', root / 'helpers.void'
    entry.write_text(BASE)
    helper.write_text(API[:API.index('struct accumulator:')] +
                      API[API.index('## Combine with an explicitly selected type.'):])
    with Client(server, entry) as client:
        client.request('initialize', {'capabilities': {'textDocument': {
            'publishDiagnostics': {'versionSupport': True},
            'semanticTokens': {'requests': {'full': True}, 'formats': ['relative'],
                               'tokenTypes': ['namespace', 'function', 'variable'], 'tokenModifiers': []},
        }}})
        client.notify('initialized', {})
        notes = client.sync()
        assert all(not note['diagnostics'] for note in notes), notes
        uri = entry.as_uri()
        version = 0

        def update(text):
            global version
            version += 1
            document = {'uri': uri, 'version': version}
            if version == 1:
                client.notify('textDocument/didOpen', {'textDocument': dict(document, languageId='void', text=text)})
            else:
                client.notify('textDocument/didChange', {'textDocument': document, 'contentChanges': [{'text': text}]})
            return client.sync()

        def complete(expression):
            # The marker gives the cursor position inside complete labels too.
            offset = expression.index('|')
            expression = expression.replace('|', '')
            text = BASE[:BASE.index('    return combine')] + '    return ' + expression + '\n'
            update(text)
            prefix = text[:BASE.index('    return combine') + len('    return ') + offset]
            response, _ = client.request('textDocument/completion', {
                'textDocument': {'uri': uri},
                'position': {'line': prefix.count('\n'), 'character': len(prefix.rsplit('\n', 1)[-1])}})
            assert 'result' in response, response
            items = response['result']
            return items['items'] if isinstance(items, dict) else items, text

        for call in ('combine', 'helpers.combine', 'instance.combine',
                     'helpers.generic_combine<i32>', 'helpers.generic_array<i32>'):
            for suffix, expected in [
                ('(|', {'first =', 'second =', 'final ='}),
                ('(fi|', {'first =', 'final ='}),
                ('(11, fi|', {'final ='}),
                ('(second = 4, fi|', {'first =', 'final ='}),
                ('(first = 11, fi|', {'final ='}),
                ('(fi| = 11, second = 4)', {'first =', 'final ='}),
                ('(fi|, final = 4)', {'first ='}),
                ('(first = 11, second = 4, |', {'final ='}),
            ]:
                items, _ = complete(call + suffix)
                labels = {item['label'] for item in items if item['label'].endswith(' =')}
                assert labels == expected, (call, suffix, labels, expected, items)
                for item in items:
                    if item['label'] not in expected:
                        continue
                    name = item['label'].removesuffix(' =')
                    assert item['documentation'] == {'first': 'First input.', 'second': 'Second input.', 'final': 'Last input.'}[name]
                    assert item['textEdit']['newText'] == (name if '| = 11' in suffix else name + ' = '), item
            items, _ = complete(call + '(fi|')
            assert any(item['label'] == 'first_value' for item in items), items
            items, _ = complete(call + '(first = fi|')
            assert not any(item['label'].endswith(' =') for item in items), items
            assert any(item['label'] == 'first_value' for item in items), items
            for broken in ('(unknown = 2, fi|', '(first = 1, first = 2, fi|'):
                items, _ = complete(call + broken)
                if call.endswith('<i32>'):
                    # Generic inference can retain ordinary expression
                    # completion after rejecting this argument binding.
                    assert all(item['label'] == 'first_value' for item in items), (broken, items)
                else:
                    assert not items, (broken, items)

        # Completion belongs to the nested callable, without outer labels.
        items, _ = complete('combine(first = helpers.combine(second = 4, fi|')
        assert {item['label'] for item in items if item['label'].endswith(' =')} == {'first =', 'final ='}, items

        for _ in range(5):
            items, _ = complete('helpers.combine(first = 11, fi|')
            assert {item['label'] for item in items if item['label'].endswith(' =')} == {'final ='}
            notes = update(BASE)
            assert notes and all(not note['diagnostics'] for note in notes), notes
            response, _ = client.request('textDocument/semanticTokens/full', {'textDocument': {'uri': uri}})
            assert response['result']['data'], response
        client.shutdown()

print('Named labels passed local/imported calls, partial labels, existing bindings, expression completions, nested calls and edit recovery')
