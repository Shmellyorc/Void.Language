"""The compiler-owned LSP sees the same projected generic types and repairs."""
from pathlib import Path
import resource
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from lsp_client import Client

source = '''func identity<t>(input: t) -> t:
    return input
func read() -> result<i32, string>:
    return Ok(7)
func work() -> result<i32, string>:
    var forwarded = identity(read()?)
    return Ok(forwarded)
func main():
    var output = work()
'''

def point(text, needle, delta=0):
    prefix = text[:text.index(needle) + delta]
    return {'line': prefix.count('\n'), 'character': len(prefix.rsplit('\n', 1)[-1])}

with tempfile.TemporaryDirectory(prefix='void-hint-lsp-') as temporary:
    entry = Path(temporary) / 'main.void'
    entry.write_text(source)
    previous = resource.getrlimit(resource.RLIMIT_STACK)
    resource.setrlimit(resource.RLIMIT_STACK, (1048576, previous[1]))
    client = Client(Path(sys.argv[1]).resolve(), entry)
    resource.setrlimit(resource.RLIMIT_STACK, previous)
    assert '1048576' in next(line for line in (Path('/proc') / str(client.process.pid) / 'limits').read_text().splitlines() if 'Max stack size' in line)
    with client:
        client.initialize()
        uri = entry.as_uri()
        client.notify('textDocument/didOpen', {'textDocument': {'uri': uri, 'languageId': 'void', 'version': 1, 'text': source}})
        notes = client.sync()
        assert all(not item['diagnostics'] for item in notes), notes
        result, _ = client.request('textDocument/hover', {'textDocument': {'uri': uri}, 'position': point(source, 'forwarded', 2)})
        assert 'i32' in str(result) and 'result<' not in str(result), result
        definition, _ = client.request('textDocument/definition', {'textDocument': {'uri': uri}, 'position': point(source, 'identity(read', 2)})
        assert entry.as_uri() in str(definition), definition
        broken = source.replace('identity(read()?)', 'identity(7?)')
        client.notify('textDocument/didChange', {'textDocument': {'uri': uri, 'version': 2}, 'contentChanges': [{'text': broken}]})
        notes = client.sync()
        assert any(item['diagnostics'] for item in notes), notes
        client.notify('textDocument/didChange', {'textDocument': {'uri': uri, 'version': 3}, 'contentChanges': [{'text': source}]})
        notes = client.sync()
        assert all(not item['diagnostics'] for item in notes), notes
        client.shutdown()
print('Shared hints LSP: projected hover, navigation, diagnostics and repair passed on 1 MiB stack')
