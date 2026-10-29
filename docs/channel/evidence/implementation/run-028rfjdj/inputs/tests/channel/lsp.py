"""Use the production stdio server for imports, docs, generic signatures and recovery."""
from pathlib import Path
from contextlib import contextmanager
import json
import os
import sys
import tempfile
import support as h
sys.path.insert(0, str(h.ROOT / 'tools'))
from lsp_client import Client

server = Path(sys.argv[1]).resolve()
checks = 0

def check(ok, detail):
    global checks
    assert ok, detail
    checks += 1

def point(text, needle, delta=0):
    prefix = text[:text.index(needle) + delta]
    return {'line': prefix.count('\n'), 'character': len(prefix.rsplit('\n', 1)[-1].encode('utf-16-le')) // 2}

@contextmanager
def project(path):
    previous = Path.cwd(); os.chdir(path)
    try:
        yield
    finally:
        os.chdir(previous)

with tempfile.TemporaryDirectory(prefix='VOID-channel-LSP-') as directory, project(directory):
    entry = Path(directory) / 'main.void'
    prefix = 'use channel as ch\nfunc receive(const amount: i32):\n    pass\nfunc main():\n    var bus = ch.router<i32>()\n'
    base = prefix + '    const token = bus.subscribe("score", receive)\n    bus.emit("score", 7)\n    bus.unsubscribe(token)\n    bus.clear()\n'
    entry.write_text(base)
    with Client(server, entry) as client:
        client.request('initialize', {'capabilities': {}}); client.notify('initialized', {}); client.sync()
        text = base; version = 0
        def replace(content):
            global text, version
            text = content; version += 1
            client.notify('textDocument/didOpen' if version == 1 else 'textDocument/didChange',
                          {'textDocument': {'uri': entry.as_uri(), 'languageId': 'void', 'version': version, 'text': content}} if version == 1 else
                          {'textDocument': {'uri': entry.as_uri(), 'version': version}, 'contentChanges': [{'text': content}]})
            return client.sync()
        def query(method, needle, delta=0, **extra):
            response = client.request('textDocument/' + method, {'textDocument': {'uri': entry.as_uri()}, 'position': point(text, needle, delta), **extra})[0]
            check('error' not in response, response)
            return response['result']
        def names(response):
            return {item['label']: item for item in (response.get('items', []) if isinstance(response, dict) else response)}
        notes = replace(base); check(all(not x['diagnostics'] for x in notes), notes)
        for method, description in [('subscribe', 'strong callback'), ('unsubscribe', 'exactly one'), ('emit', 'synchronously'), ('clear', 'reusable')]:
            hover = query('hover', 'bus.' + method, 4)
            check(description in str(hover), hover)
            check('@param' not in str(hover), hover)
            definition = query('definition', 'bus.' + method, 4)
            check('channel.void' in str(definition), definition)
            references = query('references', 'bus.' + method, 4, context={'includeDeclaration': True})
            check(bool(references), references)
        replace('use chan')
        check('channel' in names(query('completion', 'chan', 4)), 'channel import missing')
        replace('use channel as ch\nfunc main():\n    var value = ch.')
        exported = names(query('completion', 'ch.', 3))
        check({'router', 'subscription'} <= exported.keys(), exported)
        check(not any(name.startswith('_') for name in exported), exported)
        replace(prefix + '    bus.')
        methods = names(query('completion', 'bus.', 4))
        check({'subscribe', 'unsubscribe', 'emit', 'clear'} <= methods.keys(), methods)
        check(not any(name.startswith('_') for name in methods), methods)
        for call, active, description in [('bus.subscribe(', 0, 'Exact UTF-8'), ('bus.subscribe("t", ', 1, 'Synchronous callback'),
                                          ('bus.emit("t", ', 1, 'readonly authority'), ('bus.unsubscribe(', 0, 'Handle returned')]:
            replace(prefix + '    ' + call)
            result = query('signatureHelp', call, len(call))
            check(bool(result), (call, result))
            check(result['activeParameter'] == active and description in str(result), result)
        # Concrete completion/hover must retain the selected payload signature.
        replace(base)
        hover = query('hover', 'bus.subscribe', 4)
        check('func(readonly i32)' in str(hover), hover)
        denied = client.request('textDocument/rename', {'textDocument': {'uri': entry.as_uri()}, 'position': point(text, 'bus.subscribe', 4), 'newName': 'renamed'})[0]
        check('read-only' in str(denied), denied)
        for fragment in ('u', prefix + '    bus.subscribe("t", ', prefix + '    bus.emit(', prefix + '    bus.'):
            replace(fragment)
        notes = replace(base); check(all(not x['diagnostics'] for x in notes), notes)
        check(bool(query('definition', 'bus.emit', 4)), 'definition lost after unsaved repair')
        client.shutdown()
    (h.EVIDENCE / ('lsp-' + server.parent.name + '.json')).write_text(json.dumps({'server': str(server), 'checks': checks}, indent=2) + '\n')
print(f'Channel LSP: {checks} checks passed')
