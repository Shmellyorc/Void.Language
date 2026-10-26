"""Compiler-owned io/path docs and completion through the persistent VOIDLS process."""
from pathlib import Path
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from lsp_client import Client

server = Path(sys.argv[1]).resolve()
if '--small-stack' in sys.argv:
    import resource
    resource.setrlimit(resource.RLIMIT_STACK, (1024 * 1024, 1024 * 1024))
checks = 0


def check(value, message):
    global checks
    assert value, message
    checks += 1


def point(text, needle, delta=0):
    prefix = text[:text.index(needle) + delta]
    return {'line': prefix.count('\n'), 'character': len(prefix.rsplit('\n', 1)[-1])}


with tempfile.TemporaryDirectory(prefix='VOID io LSP ') as directory:
    entry = Path(directory) / 'main.void'
    base = '''use io
use path
use encoding/binary

func main():
    var pending = io.open("unused")
    if pending is Ok(var handle):
        var exact = handle.read_exact(4)
        var pascal = handle.read_pascal_string(1024)
        var files = io.get_files(".", "*.void", true)
        var changed = path.change_extension("save.dat", "bin")
'''
    entry.write_text(base)
    with Client(server, entry) as client:
        client.request('initialize', {'capabilities': {'textDocument': {
            'signatureHelp': {'signatureInformation': {'documentationFormat': ['plaintext']}}}}})
        client.notify('initialized', {})
        initial = client.sync()
        version = 0
        text = base

        def replace(updated):
            global version, text
            version += 1
            text = updated
            document = {'uri': entry.as_uri(), 'version': version}
            if version == 1:
                client.notify('textDocument/didOpen', {'textDocument': {
                    **document, 'languageId': 'void', 'text': text}})
            else:
                client.notify('textDocument/didChange', {'textDocument': document,
                                                       'contentChanges': [{'text': text}]})
            return client.sync()

        def query(method, needle, delta=0, **extra):
            result = client.request('textDocument/' + method, {
                'textDocument': {'uri': entry.as_uri()}, 'position': point(text, needle, delta), **extra})[0]
            check('error' not in result, result)
            return result['result']

        notes = initial + replace(base)
        check(all(not note['diagnostics'] for note in notes), notes)
        for needle, delta, expected in (
            ('handle.read_exact', 9, 'short transfers'),
            ('handle.read_pascal_string', 9, 'u32 UTF-8 byte-length'),
            ('io.get_files', 5, 'recursive directory'),
            ('path.change_extension', 8, 'Replace or remove'),
        ):
            hover = query('hover', needle, delta)['contents']['value']
            check(expected in hover and '@param' not in hover and '@return' not in hover, hover)
            check('Returns:' in hover, hover)
            check(query('definition', needle, delta), needle)
        header = base[:base.index('        var exact')]
        replace(header + '        var selected = handle.')
        completion = query('completion', 'handle.', len('handle.'))
        items = completion['items'] if isinstance(completion, dict) else completion
        names = {item['label']: item for item in items}
        public = {'order', 'read_bytes', 'read_exact', 'read_line', 'write_bytes', 'seek', 'position',
                  'length', 'set_length', 'flush', 'close', 'read', 'write', 'read_string', 'write_string',
                  'read_pascal_string', 'write_pascal_string'}
        public |= {name + '_async' for name in public if name != 'order'}
        check(public <= names.keys(), names)
        check(not any(name.startswith('_') for name in names), names)
        check('Returns:' in names['read_exact']['documentation'], names['read_exact'])
        call = 'io.get_files(".", '
        replace('use io\nfunc main():\n    var lookup = ' + call)
        signature = query('signatureHelp', call, len(call), context={'triggerKind': 1})
        check(signature and signature['activeParameter'] == 1 and
              'Filename pattern' in str(signature), signature)
        # Incomplete edits must keep the same server alive and recover with the
        # compiler's normal diagnostics and documented completion candidates.
        for initializer in ('', '3', '[', 'io.'):
            replace('use io\nconst choices = [1, 3, 5, 9]\nconst max_tries = ' + initializer +
                    '\nvar index: i32\nfunc main():\n    var probe = io.\n')
            for repeat in range(3):
                query('completion', 'io.\n', len('io.'))
        replace(base)
        hover = query('hover', 'handle.read_exact', 9)['contents']['value']
        check('Read exactly' in hover, hover)
print(f'{checks} io/path LSP documentation, completion and persistent recovery checks passed')
