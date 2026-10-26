"""Persistent compiler-owned documentation, completion, signatures and edit recovery."""
from pathlib import Path
from types import SimpleNamespace
import sys
import tempfile

from crash import crashed, preserve
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from lsp_client import Client

server = Path(sys.argv[1]).resolve()
if '--small-stack' in sys.argv:
    import resource
    resource.setrlimit(resource.RLIMIT_STACK, (1024 * 1024, 1024 * 1024))
checks = 0


def check(valid, detail):
    global checks
    assert valid, detail
    checks += 1


def point(value, needle, delta):
    prefix = value[:value.rindex(needle) + delta]
    return {'line': prefix.count('\n'), 'character': len(prefix.rsplit('\n', 1)[-1])}


base = '''use text
use encoding/hex
use encoding/base64
use encoding/ascii

const choices = [1, 3, 5, 9]
var index: i32

func main():
    var trimmed = text.trim(" x ")
    var encoded_hex = hex.encode([0, 128, 255])
    var decoded_base64 = base64.decode("AA==")
    var encoded_ascii = ascii.encode("hello")
'''
with tempfile.TemporaryDirectory(prefix='VOID text LSP ') as directory:
    entry = Path(directory) / 'main.void'
    entry.write_text(base)
    client = Client(server, entry)
    current = base
    version = 0
    try:
        client.initialize()

        def replace(updated):
            global current, version
            current = updated
            # Preserve the exact edited source on disk before a possible server crash.
            entry.write_text(updated)
            version += 1
            document = {'uri': entry.as_uri(), 'version': version}
            if version == 1:
                client.notify('textDocument/didOpen', {'textDocument': {
                    **document, 'languageId': 'void', 'text': updated}})
            else:
                client.notify('textDocument/didChange', {'textDocument': document,
                    'contentChanges': [{'text': updated}]})
            return client.sync()

        def query(method, needle, delta):
            response = client.request('textDocument/' + method, {
                'textDocument': {'uri': entry.as_uri()}, 'position': point(current, needle, delta)})[0]
            check('error' not in response, response)
            return response['result']

        check(all(not item['diagnostics'] for item in replace(base)), 'initial diagnostics')
        for module, name, expected in [('text', 'trim', 'White_Space'), ('hex', 'encode', 'hexadecimal'),
                                       ('base64', 'decode', 'RFC 4648'), ('ascii', 'encode', 'U+007F')]:
            needle = module + '.' + name
            shown = query('hover', needle, len(module) + 2)['contents']['value']
            check(expected in shown and 'Returns:' in shown, shown)
            check('@param' not in shown and '@return' not in shown, shown)
            check(query('definition', needle, len(module) + 2), needle)
        public = {
            'text': {'trim', 'trim_start', 'trim_end', 'is_empty', 'is_blank', 'contains', 'starts_with',
                     'ends_with', 'index_of', 'last_index_of', 'substring', 'split', 'split_lines', 'join',
                     'replace', 'replace_first', 'repeat', 'pad_left', 'pad_right', 'to_lower', 'to_upper'},
            'hex': {'encode', 'try_encode', 'decode', 'error', 'error_kind'},
            'base64': {'encode', 'try_encode', 'decode', 'alphabet', 'padding', 'error', 'error_kind'},
            'ascii': {'encode', 'decode', 'is_alpha', 'is_digit', 'is_alphanumeric', 'is_upper', 'is_lower',
                      'is_whitespace', 'is_printable', 'is_control', 'to_lower', 'to_upper'},
        }
        for module, names in public.items():
            replace(base + f'    var pending = {module}.')
            result = query('completion', module + '.', len(module) + 1)
            items = result['items'] if isinstance(result, dict) else result
            indexed = {item['label']: item for item in items}
            check(names <= indexed.keys(), indexed)
            check(not any(name.startswith('_') for name in indexed), indexed)
        call = 'text.substring("雪", '
        replace(base + '    var selected = ' + call)
        signature = query('signatureHelp', call, len(call))
        check(signature and signature['activeParameter'] == 1 and 'Scalar boundary' in str(signature), signature)
        for initializer in ['', '3', '[', 'text.', 'base64.decode(']:
            replace(base.replace('const choices', 'const pending = ' + initializer + '\nconst choices') +
                    '    var candidate = text.')
            for repeat in range(3):
                query('completion', 'text.', len('text.'))
        check(all(not item['diagnostics'] for item in replace(base)), 'recovery diagnostics')
        check('White_Space' in query('hover', 'text.trim', 6)['contents']['value'], 'recovery hover')
        client.shutdown()
    finally:
        status = client.process.poll()
        if status is None:
            if client.process.stdin and not client.process.stdin.closed:
                client.process.stdin.close()
            status = client.process.wait(timeout=30)
        client.reader.join(timeout=5)
        client.logger.join(timeout=5)
        if crashed(status):
            preserve([server, '--entry', entry], directory,
                     SimpleNamespace(returncode=status, stdout=b'', stderr=bytes(client.logs)))
        check(status == 0, (status, bytes(client.logs)))
print(f'{checks} persistent LSP documentation/completion/recovery checks passed')
