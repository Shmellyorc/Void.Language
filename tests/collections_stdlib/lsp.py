"""Compiler-owned import/member docs, signatures, navigation and unsaved recovery."""
from pathlib import Path
from types import SimpleNamespace
import json
import os
import sys
import tempfile
import support as h
h.crash.install()
sys.path.insert(0, str(h.ROOT / 'tools'))
from lsp_client import Client
server = Path(sys.argv[1]).resolve()
library = Path(os.environ.get('VOID_TEST_STDLIB', os.environ.get('VOID_STDLIB_ROOT', h.ROOT / 'stdlib'))).resolve()
checks = 0
public = set('reversed sorted is_sorted numeric_less text_less lower_bound upper_bound binary_search find find_index index_of contains any all count_if filter map fold min max sum keys values copy_dictionary'.split())
def check(ok, detail):
    global checks
    assert ok, detail
    checks += 1

def point(text, needle, delta=0):
    prefix = text[:text.index(needle) + delta]
    return {'line': prefix.count('\n'), 'character': len(prefix.rsplit('\n', 1)[-1].encode('utf-16-le')) // 2}

with tempfile.TemporaryDirectory(prefix='VOID-collections-lsp-') as directory:
    work = Path(directory); entry = work / 'main.void'
    base = 'use collections as c\nfunc main():\n    var label = "🔥"\n    var input = [3, 1, 2]\n    var ordered = c.sorted(input, c.numeric_less<i32>)\n    var copied = c.copy_dictionary({"a": 1})\n'
    entry.write_text(base, encoding='utf-8')
    previous = Path.cwd(); os.chdir(work)
    client = None
    try:
        with Client(server, entry) as client:
            client.initialize()
            text = base; version = 0
            def replace(content):
                global text, version
                text = content; version += 1
                client.notify('textDocument/didOpen' if version == 1 else 'textDocument/didChange',
                    {'textDocument': {'uri': entry.as_uri(), 'languageId': 'void', 'version': version, 'text': text}} if version == 1 else
                    {'textDocument': {'uri': entry.as_uri(), 'version': version}, 'contentChanges': [{'text': text}]})
                return client.sync()
            def query(method, needle, delta=0, **extra):
                response = client.request('textDocument/' + method, {'textDocument': {'uri': entry.as_uri()}, 'position': point(text, needle, delta), **extra})[0]
                check('error' not in response, response)
                return response['result']
            def labels(response):
                return {item['label']: item for item in (response.get('items', []) if isinstance(response, dict) else response)}
            notes = replace(base); check(all(not item['diagnostics'] for item in notes), notes)
            for method, phrase in [('sorted', 'Stable sorted copy'), ('copy_dictionary', 'insertion order'), ('numeric_less', 'NaNs last')]:
                hover = query('hover', 'c.' + method, 2)
                check(phrase in str(hover) and '@param' not in str(hover), hover)
                check('collections.void' in str(query('definition', 'c.' + method, 2)), method)
                check(bool(query('references', 'c.' + method, 2, context={'includeDeclaration': True})), method)
            denied = client.request('textDocument/rename', {'textDocument': {'uri': entry.as_uri()}, 'position': point(text, 'c.sorted', 2), 'newName': 'renamed'})[0]
            check('read-only' in str(denied), denied)
            replace('use collec')
            check('collections' in labels(query('completion', 'collec', 6)), 'import completion')
            replace('use collections as c\nfunc main():\n    c.')
            exported = labels(query('completion', 'c.', 2))
            check(public | {'error', 'error_kind'} <= exported.keys(), exported)
            check(not any(name.startswith('_') for name in exported), exported)
            replace('use collections as c\nfunc main():\n    c.error_kind.')
            check({'not_found', 'empty_collection'} <= labels(query('completion', 'error_kind.', 11)).keys(), 'error cases')
            prefix = 'use collections as c\nfunc main():\n    var input = [3, 1, 2]\n    '
            for call, active, phrase in [('c.sorted(', 0, 'Growable input'), ('c.sorted(input, ', 1, 'strict weak ordering'), ('c.sorted(input, c.numeric_less<i32>, ', 2, 'Swap comparator'), ('c.map<i32, i32>(', 0, 'Live input')]:
                replace(prefix + call)
                result = query('signatureHelp', call, len(call))
                check(result and result['activeParameter'] == active and phrase in str(result), result)
            custom = 'use collections as c\nvalue struct marker:\n    number: i32\n    operator ==(const left: marker, const right: marker) -> bool:\n        return left.number == right.number\nfunc main():\n    var found = c.index_of([marker(number = 1)], marker(number = 1))\n    var present = c.contains([marker(number = 1)], marker(number = 1))\n'
            notes = replace(custom); check(all(not item['diagnostics'] for item in notes), notes)
            library_text = (library / 'collections.void').read_text(encoding='utf-8')
            response = client.request('textDocument/definition', {'textDocument': {'uri': (library / 'collections.void').as_uri()}, 'position': point(library_text, 'if item == value', 8)})[0]
            check('error' not in response and 'main.void' in str(response.get('result')), response)
            for fragment in ('u', prefix + 'c.', prefix + 'c.sorted(input, ', prefix + 'c.map<'):
                replace(fragment)
            notes = replace(base); check(all(not item['diagnostics'] for item in notes), notes)
            check(bool(query('definition', 'c.sorted', 2)), 'navigation after unsaved recovery')
            client.shutdown()
    except BaseException as error:
        if client is not None and not getattr(client.process, '_incident_preserved', False):
            process = client.process
            if process.poll() is None:
                process.kill(); process.wait()
            h.crash.preserve([server, '--entry', entry], work, SimpleNamespace(
                returncode=process.returncode, stdout=process.stdout.captured(), stderr=process.stderr.captured(),
                stdin=process.stdin.captured()), source_inputs=[work], provenance_inputs=[Path(__file__).resolve()],
                failure_kind='timeout' if type(error).__name__ in ('Empty', 'TimeoutExpired') else 'test_assertion',
                evidence_context={'mode': 'lsp', 'assertion': str(error)},
                source_snapshots=process._source_snapshots)
        raise
    finally:
        os.chdir(previous)
    target = Path(tempfile.mkdtemp(prefix='lsp-' + server.parent.name + '-', dir=h.EVIDENCE)) / 'receipt.json'
    target.write_text(json.dumps({'server': str(server), 'server_sha256': h.crash.digest(server), 'checks': checks}, indent=2) + '\n', encoding='utf-8')
print(f'Collections LSP: {checks} checks passed')
