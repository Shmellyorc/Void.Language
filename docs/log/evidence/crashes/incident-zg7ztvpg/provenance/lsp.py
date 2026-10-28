"""Compiler-owned import/member docs, signatures, navigation and unsaved recovery."""
from pathlib import Path
from types import SimpleNamespace
import json
import os
import sys
import tempfile
import support as h
h.crash.install()
if os.name == 'posix':
    h.stack()
sys.path.insert(0, str(h.ROOT / 'tools'))
from lsp_client import Client
server = Path(sys.argv[1]).resolve()
library = Path(os.environ.get('VOID_TEST_STDLIB', os.environ.get('VOID_STDLIB_ROOT', h.ROOT / 'stdlib')))
checks = 0
public = set('open open_async configure configure_async enabled write trace debug info warn error write_lazy write_async flush flush_async shutdown shutdown_async'.split())
def check(ok, detail):
    global checks
    assert ok, detail
    checks += 1

def point(text, needle, delta=0):
    prefix = text[:text.index(needle) + delta]
    return {'line': prefix.count('\n'), 'character': len(prefix.rsplit('\n', 1)[-1].encode('utf-16-le')) // 2}

with tempfile.TemporaryDirectory(prefix='VOID-log-lsp-') as directory:
    work = Path(directory); entry = work / 'main.void'
    base = 'use log as c\nuse convert\nfunc main():\n    var label = "🔥"\n    var instance = convert.unwrap(c.open(c.config(name = label)))\n    c.write(c.level.info, "hello")\n    c.configure(c.config())\n'
    entry.write_text(base)
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
            for method, phrase in [('open', 'independent managed logger'), ('write', 'default logger'), ('configure', 'configure or replace')]:
                hover = query('hover', 'c.' + method, 2)
                check(phrase in str(hover) and '@param' not in str(hover), hover)
                check('log.void' in str(query('definition', 'c.' + method, 2)), method)
                check(bool(query('references', 'c.' + method, 2, context={'includeDeclaration': True})), method)
            denied = client.request('textDocument/rename', {'textDocument': {'uri': entry.as_uri()}, 'position': point(text, 'c.write', 2), 'newName': 'renamed'})[0]
            check('read-only' in str(denied), denied)
            replace('use lo')
            check('log' in labels(query('completion', 'lo', 2)), 'import completion')
            replace('use log as c\nfunc main():\n    c.')
            exported = labels(query('completion', 'c.', 2))
            check(public | {'config', 'level', 'failure', 'failure_kind', 'logger', 'console_sink', 'existing_files'} <= exported.keys(), exported)
            check(not any(name.startswith('_') for name in exported), exported)
            replace('use log as c\nfunc main():\n    c.level.')
            check({'trace','debug','info','warn','error','off'} <= labels(query('completion', 'level.', 6)).keys(), 'severity cases')
            prefix = 'use log as c\nfunc main():\n    '
            for call, active, phrase in [('c.open(', 0, 'Copied settings'), ('c.write(', 0, 'Record severity'), ('c.write(c.level.info, ', 1, 'Already evaluated text'), ('c.write_lazy(c.level.info, ', 1, 'Temporary synchronous builder')]:
                replace(prefix + call)
                result = query('signatureHelp', call, len(call))
                check(result and result['activeParameter'] == active and phrase in str(result), result)
            for fragment in ('u', prefix + 'c.', prefix + 'c.write(c.level.info, ', prefix + 'c.open('):
                replace(fragment)
            notes = replace(base); check(all(not item['diagnostics'] for item in notes), notes)
            check(bool(query('definition', 'c.write', 2)), 'navigation after unsaved recovery')
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
    target.write_text(json.dumps({'server': str(server), 'server_sha256': h.crash.digest(server), 'checks': checks}, indent=2) + '\n')
print(f'Logging LSP: {checks} checks passed')
