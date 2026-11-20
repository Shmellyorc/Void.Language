"""Modernized standard-library enum, field and network signature documentation."""
from pathlib import Path
import json
import os
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from lsp_client import Client
from test_output import output_directory

server, report = (Path(p).resolve() for p in sys.argv[1:])
source = '''use net/tcp as tcp
use net/error as net_error
use process as process
func main():
    var first = net_error.error_kind.connection_reset
    var second = net_error.error_kind.connection_reset
    var config = tcp.connect_options()
    print(config.timeout_ms)
'''
records = []


def limits():
    import resource
    resource.setrlimit(resource.RLIMIT_STACK, (1048576, 1048576))
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


with tempfile.TemporaryDirectory(prefix='g88-docs-', dir=output_directory()) as directory:
    file = Path(directory) / 'main.void'; file.write_text(source)
    with Client(server, file, preexec_fn=limits if os.name == 'posix' else None) as client:
        client.request('initialize', {'capabilities': {}})
        client.notify('initialized', {}); client.sync(); version = 0

        def update(text):
            global source, version
            source = text; version += 1
            client.notify('textDocument/didOpen' if version == 1 else 'textDocument/didChange',
                {'textDocument': {'uri': file.as_uri(), 'languageId': 'void', 'version': version, 'text': text}}
                if version == 1 else {'textDocument': {'uri': file.as_uri(), 'version': version}, 'contentChanges': [{'text': text}]})
            notes = client.sync(); records.append(dict(diagnostics=notes))
            return notes

        def query(method, needle, delta=0, **extra):
            before = source[:source.index(needle) + delta]
            response, _ = client.request('textDocument/' + method, {'textDocument': {'uri': file.as_uri()},
                'position': dict(line=before.count('\n'), character=len(before.rsplit('\n', 1)[-1])), **extra})
            records.append(dict(method=method, response=response))
            report.parent.mkdir(parents=True, exist_ok=True)
            report.write_text(json.dumps(dict(stack_bytes=1048576, source=source, records=records), indent=2) + '\n')
            assert 'error' not in response, response
            return response.get('result')

        assert not [d for n in update(source) for d in n['diagnostics'] if d.get('severity') == 1]
        assert 'distinct from EOF' in str(query('hover', 'net_error.error_kind.connection_reset', len('net_error.error_kind.') + 2))
        assert 'net/error.void' in str(query('definition', 'net_error.error_kind.connection_reset', len('net_error.error_kind.') + 2))
        references = query('references', 'net_error.error_kind.connection_reset', len('net_error.error_kind.') + 2,
                           context={'includeDeclaration': True})
        assert references and len(references) >= 3, references
        base = 'use net/tcp as tcp\nfunc main():\n    tcp.connect('
        update(base)
        result = query('signatureHelp', 'tcp.connect(', len('tcp.connect('))
        assert 'Numeric literal or ASCII hostname' in str(result) and 'result<stream, error>' in str(result), result
        update(base + '"127.0.0.1", 80)\n')
        assert 'owned stream' in str(query('hover', 'tcp.connect(', len('tcp.') + 2))
        update('use process as process\nfunc main():\n    var reason = process.capture_end.output_limit\n')
        assert 'retained prefixes remain bounded' in str(query('hover', 'process.capture_end.output_limit', len('process.capture_end.') + 2))
print('G88 standard-library documentation: enum hover/definition/references, signature ownership and capture outcome passed')
