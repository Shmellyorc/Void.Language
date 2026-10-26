"""Compiler-owned standard error, conversion and ordinary console editor metadata."""
from pathlib import Path
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from lsp_client import Client

server = Path(sys.argv[1]).resolve()
checks = 0


def check(valid, detail):
    global checks
    assert valid, detail
    checks += 1


def point(text, needle, delta=0):
    prefix = text[:text.rindex(needle) + delta]
    return {'line': prefix.count('\n'), 'character': len(prefix.rsplit('\n', 1)[-1])}


base = '''use error as core
use convert
use console
use time
use encoding/binary
use io/memory

func main():
    var reason = core.error(message = "reason", path = "asset", code = 7)
    var text = convert.to_string(42)
    var number = convert.parse<i32>("42")
    var value = convert.unwrap(number)
    var fallback = convert.unwrap_or(number, 0)
    var line = readline("Prompt: ")
    var character = read(2)
    var day = time.make_date(2024, 2, 29)
'''
with tempfile.TemporaryDirectory(prefix='VOID combined LSP ') as directory:
    entry = Path(directory) / 'main.void'
    entry.write_text(base)
    with Client(server, entry) as client:
        client.request('initialize', {'capabilities': {'textDocument': {
            'signatureHelp': {'signatureInformation': {'documentationFormat': ['plaintext']}}}}})
        client.notify('initialized', {})
        initial = client.sync()
        text = base
        version = 0

        def replace(updated):
            global text, version
            text = updated
            version += 1
            if version == 1:
                client.notify('textDocument/didOpen', {'textDocument': {
                    'uri': entry.as_uri(), 'languageId': 'void', 'version': version, 'text': text}})
            else:
                client.notify('textDocument/didChange', {'textDocument': {
                    'uri': entry.as_uri(), 'version': version}, 'contentChanges': [{'text': text}]})
            return client.sync()

        def query(method, needle, delta=0):
            response = client.request('textDocument/' + method, {
                'textDocument': {'uri': entry.as_uri()}, 'position': point(text, needle, delta)})[0]
            check('error' not in response, response)
            return response['result']

        check(all(not note['diagnostics'] for note in initial + replace(base)), 'base diagnostics')
        for name, needle, description in (
            ('parse', 'convert.parse', 'complete invariant'),
            ('to_string', 'convert.to_string', 'existing invariant display'),
            ('unwrap', 'convert.unwrap(', 'checked Runtime failure'),
            ('unwrap_or', 'convert.unwrap_or', 'eagerly evaluated fallback'),
            ('readline', 'readline(', 'complete logical line'),
            ('read', 'read(2)', 'Unicode scalars'),
        ):
            delta = needle.index(name) + 1
            result = query('hover', needle, delta)
            shown = result['contents']['value']
            check(description in shown and 'Returns:' in shown, shown)
            check('@param' not in shown and '@return' not in shown, shown)
            definition = query('definition', needle, delta)
            check(definition and ('convert.void' if name.startswith(('parse', 'to_string', 'unwrap'))
                                 else 'console.void') in str(definition), definition)
        for module in ('convert', 'binary', 'memory', 'core', 'time', 'console'):
            replace(base + f'    var selected = {module}.')
            result = query('completion', module + '.', len(module) + 1)
            items = result['items'] if isinstance(result, dict) else result
            labels = {item['label']: item for item in items}
            check(not any(name.startswith('_') for name in labels), labels)
            check(('error' if module != 'console' else 'io_error') in labels, labels)
            if module in ('binary', 'memory', 'convert'):
                check('error_kind' in labels, labels)
        for expression in ('core.error()', 'binary.error(kind = binary.error_kind.wrong_width)',
                           'memory.error(kind = memory.error_kind.invalid_range)',
                           'convert.error(kind = convert.error_kind.invalid_text)', 'time.error()',
                           'console.io_error(kind = console.io_error_kind.invalid_argument)'):
            replace(base + f'    var failure = {expression}\n    var selected = failure.')
            result = query('completion', 'failure.', len('failure.'))
            items = result['items'] if isinstance(result, dict) else result
            labels = {item['label']: item for item in items}
            check({'message', 'path', 'code'} <= labels.keys(), (expression, labels))
            for field in ('message', 'path', 'code'):
                check(bool(labels[field].get('documentation')), labels[field])
        for call, active, description in (
            ('readline(', 0, 'complete and flush'),
            ('read(', 0, 'Maximum scalar count'),
            ('convert.parse<i32>(', 0, 'Exact text'),
            ('convert.unwrap_or(number, ', 1, 'evaluated normally'),
        ):
            replace(base + '    var pending = ' + call)
            result = query('signatureHelp', call, len(call))
            check(result and result['activeParameter'] == active, result)
            shown = result['signatures'][0]
            check(description in shown['parameters'][active]['documentation']['value'], shown)
        replace(base + '    var location = reason.path\n')
        definition = query('definition', 'reason.path', len('reason.') + 1)
        check(definition and 'error.void' in str(definition), definition)
        check(all(not note['diagnostics'] for note in replace(base)), 'final diagnostics')
        client.shutdown()

print(f'Combined LSP: {checks} documentation, completion, signature and navigation assertions passed')
