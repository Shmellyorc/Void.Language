"""Compiler-owned await/propagation types, source spans and editor recovery."""
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from lsp_client import Client

source = '''# 👑 await / propagation
struct box:
    number: i32
## @param value Incoming value.
async func child(value: i32 = 7) -> result<box, string>:
    return Ok(box(number = value))
async func plain(value: i32) -> box:
    return box(number = value)
func envelope<t>(input: t) -> result<t, string>:
    return Ok(input)
async func compute(value: i32) -> result<i32, string>:
    var pending = child(value)
    var answer = await pending?
    var old = (await pending)?
    var chain = await child(value)?.number
    var wrapped = envelope(plain(value))
    var grouped = await (wrapped?)
    return Ok(answer.number + old.number + chain + grouped.number)
async func entry(value: i32) -> i32:
    var result = await compute(value)
    return value
'''

def point(text, needle, delta=0):
    prefix = text[:text.index(needle) + delta]
    return {'line': prefix.count('\n'), 'character': len(prefix.rsplit('\n', 1)[-1].encode('utf-16-le')) // 2}

with tempfile.TemporaryDirectory(prefix='void-await-syntax-lsp-') as temporary:
    entry = Path(temporary) / 'main.void'
    entry.write_bytes(source.encode("utf-8"))
    if sys.platform == 'linux':
        import resource
        resource.setrlimit(resource.RLIMIT_STACK, (1048576, 1048576))
    client = Client(Path(sys.argv[1]).resolve(), entry)
    if sys.platform == 'linux':
        # This script owns its process; retain the hard limit for all descendants.
        limits = (Path('/proc') / str(client.process.pid) / 'limits').read_text()
        stack = next(line for line in limits.splitlines() if 'Max stack size' in line)
        assert stack.split()[3:5] == ['1048576', '1048576'], stack
    with client:
        initialized, _ = client.request('initialize', {'capabilities': {
            'general': {'positionEncodings': ['utf-16']},
            'textDocument': {'publishDiagnostics': {'versionSupport': True}},
            'workspace': {'workspaceEdit': {'documentChanges': True, 'failureHandling': 'textOnlyTransactional'}}}})
        assert initialized['result']['capabilities']['renameProvider'], initialized
        client.notify('initialized', {})
        client.sync()
        uri = entry.as_uri()
        version = 0
        def change(text):
            global version
            version += 1
            client.notify('textDocument/didChange', {'textDocument': {'uri': uri, 'version': version}, 'contentChanges': [{'text': text}]})
            return client.sync()
        def query(method, text, needle, delta=0, **extra):
            return client.request('textDocument/' + method, {'textDocument': {'uri': uri}, 'position': point(text, needle, delta), **extra})[0]
        client.notify('textDocument/didOpen', {'textDocument': {'uri': uri, 'languageId': 'void', 'version': 0, 'text': source}})
        notes = client.sync()
        assert notes and all(not item['diagnostics'] for item in notes), notes
        for name, expected in [('answer', 'box'), ('old', 'box'), ('chain', 'i32'), ('grouped', 'box')]:
            result = query('hover', source, name, 2)
            assert expected in str(result) and 'result<' not in str(result), result
        definition = query('definition', source, 'child(value)?.', 2)
        assert entry.as_uri() in str(definition) and "'line': 4" in str(definition), definition
        rename = query('rename', source, 'answer', 2, newName='resolved')
        assert len(rename['result']['documentChanges'][0]['edits']) == 2, rename
        signature = query('signatureHelp', source, 'child(value)?.', len('child('))
        assert 'value: i32' in str(signature), signature
        formatted, _ = client.request('textDocument/formatting', {'textDocument': {'uri': uri}, 'options': {'tabSize': 4, 'insertSpaces': True}})
        edits = formatted['result']
        rendered = edits[0]['newText'] if edits else source
        assert 'await pending?' in rendered and '(await pending)?' in rendered and 'await (wrapped?)' in rendered, rendered
        completion = source.replace('var chain = await child(value)?.number', 'var chain = await child(value)?.nu')
        change(completion)
        result = query('completion', completion, '?.nu', len('?.nu'))
        assert 'number' in {item['label'] for item in result['result']}, result
        # Explicit parentheses still diagnose propagation of a task at the '?'.
        broken = source.replace('var answer = await pending?', 'var answer = await (pending?)')
        notes = change(broken)
        question = point(broken, 'pending?)', len('pending'))
        diagnostics = [d for note in notes for d in note['diagnostics']]
        assert any(d['range']['start'] == question and d['range']['end'] == {**question, 'character': question['character'] + 1} and '? requires a result' in d['message'] for d in diagnostics), diagnostics
        malformed = source.replace('var chain = await child(value)?.number', 'var chain = await child(value)? +').replace('return value\n', 'return missing_after_syntax\n')
        notes = change(malformed)
        diagnostics = [d for note in notes for d in note['diagnostics']]
        assert any(str(d.get('code')) == 'VOID1101' for d in diagnostics), diagnostics
        assert any('missing_after_syntax' in d['message'] for d in diagnostics), diagnostics
        notes = change(source)
        assert notes and all(not item['diagnostics'] for item in notes), notes
        client.shutdown()
print('await syntax LSP: hover, definition, rename, signatures, formatting, completion, exact spans, recovery and repair passed; stack=1 MiB on Linux')
