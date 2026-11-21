"""G93: shared constant errors, source navigation, incomplete buffers and cancellation."""
from pathlib import Path
import json, os, sys, tempfile
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from lsp_client import Client, frame
from test_output import output_directory
server, report = (Path(p).resolve() for p in sys.argv[1:])
records = []
def save(kind, value):
    records.append(dict(kind=kind, value=value)); report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(records, indent=2) + '\n')
def stack():
    import resource
    resource.setrlimit(resource.RLIMIT_STACK, (1048576, 1048576))
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
class InteractiveClient(Client):
    def receive(self):
        while True:
            m = super().receive()
            if m.get('method') != 'workspace/semanticTokens/refresh': return m
            self.write(frame(dict(jsonrpc='2.0', id=m['id'], result=None)))
with tempfile.TemporaryDirectory(prefix='g93-lsp-', dir=output_directory()) as directory:
    root = Path(directory); main = root / 'main.void'
    good = '''## Square a value.
## @param value The input to square.
compile func square(value: i32) -> i32:
    return value * value
compile func identity<t>(value: t) -> t:
    return value
value struct dimensions:
    width: i32
    height: i32
compile func (d dimensions) area() -> i32:
    return d.width * d.height
const answer = square(8)
const generic = identity<i64>(42)
const pixels = dimensions(width = 20, height = 10).area()
compile if false:
    compile func excluded() -> i32:
        return missing()
func main():
    print(answer, generic, pixels)
'''
    main.write_text(good)
    with InteractiveClient(server, main, preexec_fn=stack if os.name == 'posix' else None) as client:
        init, _ = client.request('initialize', {'capabilities': {'textDocument': {'semanticTokens': {'requests': {'full': True}, 'formats': ['relative']}, 'publishDiagnostics': {'versionSupport': True, 'relatedInformation': True}}, 'workspace': {'semanticTokens': {'refreshSupport': True}}}, 'initializationOptions': {'voidInteractivePresentation': True, 'voidIncrementalSync': True}})
        legend = init['result']['capabilities']['semanticTokensProvider']['legend']; save('initialize', init)
        client.notify('initialized', {}); client.sync(); version = 0; text = ''
        def replace(body):
            global version, text
            version += 1; text = body
            doc = dict(uri=main.as_uri(), version=version)
            if version == 1: client.notify('textDocument/didOpen', {'textDocument': dict(doc, languageId='void', text=text)})
            else: client.notify('textDocument/didChange', {'textDocument': doc, 'contentChanges': [{'text': text}]})
            notes = client.sync(); save('edit', dict(version=version, source=text, notes=notes)); return notes
        def point(needle, back=0):
            offset = text.rindex(needle) + len(needle) - back; prefix = text[:offset]
            return dict(line=prefix.count('\n'), character=len(prefix.rsplit('\n', 1)[-1].encode('utf-16-le')) // 2)
        def query(method, needle=None, back=0, **extra):
            params = {'textDocument': {'uri': main.as_uri()}, **extra}
            if needle is not None: params['position'] = point(needle, back)
            result, notes = client.request('textDocument/' + method, params)
            save(method, result); assert 'result' in result, result; return result['result']
        def no_errors(notes):
            assert all(not n['diagnostics'] for n in notes), notes
        no_errors(replace(good))
        hover = query('hover', 'square(8)', 4)
        assert 'compile func' in str(hover) and 'value: i32' in str(hover), hover
        hover = query('hover', 'print(answer', 1)
        assert '64' in str(hover), hover
        hover = query('hover', 'identity<i64>', 6)
        assert 'i64' in str(hover), hover
        definition = query('definition', 'square(8)', 4)
        assert definition and (definition[0] if isinstance(definition, list) else definition)['range']['start']['line'] == good[:good.index('compile func square')].count('\n'), definition
        references = query('references', 'square(value', 7, context={'includeDeclaration': True})
        assert len(references) == 2, references
        tokens = query('semanticTokens/full')['data']; row = column = 0; decoded = []
        for i in range(0, len(tokens), 5):
            dl, dc, length, kind, modifiers = tokens[i:i+5]; row += dl; column = dc if dl else column + dc
            decoded.append((row, column, length, legend['tokenTypes'][kind], modifiers))
        assert any(t[0] == 2 and t[3] == 'function' for t in decoded), decoded
        assert any(t[0] == 15 and t[3] == 'comment' for t in decoded), decoded
        replace(good.replace('    print(answer, generic, pixels)', '    var d = dimensions()\n    d.'))
        result = query('completion', 'd.'); items = result['items'] if isinstance(result, dict) else result
        area = next(i for i in items if i['label'] == 'area'); assert 'i32' in area['detail'], area
        replace(good.replace('square(8)', 'square()'))
        signature = query('signatureHelp', 'square(')
        assert signature and 'value: i32' in str(signature), signature
        bad = 'compile func divide(value: i32) -> i32:\n    return 8 / value\nconst answer = divide(0)\n'
        notes = replace(bad); errors = [d for n in notes for d in n['diagnostics']]
        assert any(d['range']['start']['line'] == 1 and 'zero' in d['message'].lower() for d in errors), errors
        assert any(d.get('relatedInformation') for d in errors), errors
        no_errors(replace(good))
        nested = ('compile func calculate(n: i32) -> i32:\n' +
                  ''.join('    ' * (i + 1) + 'if true:\n' for i in range(12)) +
                  '    ' * 13 + 'if n == 0:\n' + '    ' * 14 + 'return 0\n' +
                  '    ' * 13 + 'return calculate(n - 1)\n' +
                  '    return 0\nconst answer = calculate(60)\n')
        notes = replace(nested); errors = [d for n in notes for d in n['diagnostics']]
        assert any('combined expression/statement nesting limit' in d['message'] and d['range']['start']['line'] < 18 and d.get('relatedInformation') for d in errors), errors
        no_errors(replace(good))
        # Cancel a new assistance request and then supersede a bounded endless
        # constant. No result from an older revision may replace the repaired one.
        slow = 'compile func spin() -> i32:\n    for:\n        pass\n    return 0\nconst busy = spin()\nfunc main():\n    pass\n'
        version += 1; text = slow
        client.notify('textDocument/didChange', {'textDocument': {'uri': main.as_uri(), 'version': version}, 'contentChanges': [{'text': slow}]})
        params = {'textDocument': {'uri': main.as_uri()}, 'position': {'line': 6, 'character': 4}}
        client.write(frame({'jsonrpc': '2.0', 'id': 'cancel-g93', 'method': 'textDocument/completion', 'params': params}) + frame({'jsonrpc': '2.0', 'method': '$/cancelRequest', 'params': {'id': 'cancel-g93'}}))
        reply, _ = client.response('cancel-g93'); save('cancel', reply)
        assert reply['error']['code'] == -32800, reply
        no_errors(replace(good))
        client.shutdown()
        save('stderr', client.logs.decode())
print('G93 LSP: constant diagnostics, assistance, recovery and cancellation at 1 MiB')
