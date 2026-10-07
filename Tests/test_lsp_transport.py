"""Assert byte-exact LSP framing, including CRLF and UTF-8 in request bodies.

Internal assertions add coverage without changing cumulative printed totals.
"""
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]


def frame(message):
    body = json.dumps(message, ensure_ascii=False, indent=2).replace('\n', '\r\n').encode('utf-8')
    return b'Content-Length: ' + str(len(body)).encode('ascii') + b'\r\n\r\n' + body


request = b''.join(frame(message) for message in [
    dict(jsonrpc='2.0', id=1, method='initialize', params=dict(clientInfo=dict(name='VOID é Ω 😀'))),
    dict(jsonrpc='2.0', id=2, method='shutdown', params=None),
    dict(jsonrpc='2.0', method='exit', params=None),
])
result = subprocess.run([str(root / 'bin/voidc'), 'lsp'], input=request,
    capture_output=True, timeout=15, cwd=root)
assert result.returncode == 0 and not result.stderr, (result.returncode, result.stdout, result.stderr)
remaining = result.stdout
messages = []
while remaining:
    header, separator, content = remaining.partition(b'\r\n\r\n')
    assert separator and header.startswith(b'Content-Length: '), repr(remaining)
    digits = header[len(b'Content-Length: '):]
    assert digits.isdigit(), repr(header)
    length = int(digits)
    assert len(content) >= length, (length, content)
    messages.append(json.loads(content[:length].decode('utf-8')))
    remaining = content[length:]
assert len(messages) == 2, messages
assert messages[0]['id'] == 1 and messages[0]['result']['serverInfo']['version'] == '0.0.340', messages
capabilities = messages[0]['result']['capabilities']
assert capabilities == dict(textDocumentSync=1, hoverProvider=True,
    signatureHelpProvider=dict(triggerCharacters=['(', ',']),
    completionProvider=dict(triggerCharacters=['.']), definitionProvider=True,
    referencesProvider=True, documentSymbolProvider=True, workspaceSymbolProvider=True), capabilities
assert set(messages[0]['result']) == {'capabilities', 'serverInfo'}, messages
assert messages[1] == dict(jsonrpc='2.0', id=2, result=None), messages
