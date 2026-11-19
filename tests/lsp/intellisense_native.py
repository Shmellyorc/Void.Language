"""Explicit-catalog G76 qualification; never prepares or executes native code.

Usage: intellisense_native.py voidls voidc approved-catalog
The catalog is copied; only that disposable copy is mutated.
"""
from pathlib import Path
import json
import os
import shutil
import subprocess
import sys
import queue
import time
from tempfile import TemporaryDirectory
import tomllib
from unittest.mock import patch

from intellisense import InteractiveClient


class ProfileClient(InteractiveClient):
    def __init__(self, *args, **kwargs):
        self.begins = queue.Queue()
        super().__init__(*args, **kwargs)
    def _logs(self):
        while line := self.process.stderr.readline():
            self.logs.extend(line)
            if line == b'VOID_LSP_PROFILE_BEGIN assistance/analysis\n':
                self.begins.put(time.perf_counter())


def run(server, compiler, supplied):
    results = []
    with TemporaryDirectory(prefix='VOID G76 native cache 雪 ') as temporary:
        base = Path(temporary)
        catalog = base / 'catalog'
        shutil.copytree(supplied, catalog, symlinks=True)
        plan = tomllib.loads((catalog / 'void.native.toml').read_text())['native']
        environment = dict(os.environ, VOID_NATIVE_CATALOG=str(catalog), VOID_LSP_PROFILE='1')
        root = base / 'app'
        subprocess.run([str(compiler), 'new', str(root)], env=environment, check=True, capture_output=True, timeout=30)
        manifest = root / 'void.project.toml'
        manifest.write_text(manifest.read_text() + '\n[native-requirements.provider]\n'
            f'id = "{plan["id"]}"\nversion = "={plan["version"]}"\nabi = {plan["abi"]}\n')
        (root / 'src/helper.void').write_text('public func known() -> i32:\n    return 7\n')
        source = 'use "./helper.void" as helper\nfunc main() -> i32:\n    return helper.known()\n'
        path = root / 'src/app.void'; path.write_text(source)
        subprocess.run([str(compiler), 'resolve', '--project', str(root)], env=environment, check=True, capture_output=True, timeout=30)
        with patch.dict(os.environ, environment, clear=True), ProfileClient(server, root, project=True) as client:
            client.request('initialize', {'capabilities': {'textDocument': {
                'semanticTokens': {'requests': {'full': True}, 'formats': ['relative']}},
                'workspace': {'semanticTokens': {'refreshSupport': True}}},
                'initializationOptions': {'voidInteractivePresentation': True}})
            client.notify('initialized', {}); client.sync()
            client.notify('textDocument/didOpen', {'textDocument': {'uri': path.as_uri(), 'version': 1,
                          'languageId': 'void', 'text': source}}); client.sync()
            params = {'textDocument': {'uri': path.as_uri()}, 'position': {'line': 2, 'character': len('    return helper.')}}
            def query():
                return client.request('textDocument/completion', params)[0]
            def known(response):
                value = response.get('result', [])
                if isinstance(value, dict): value = value['items']
                return 'known' in {item['label'] for item in value}
            assert known(query()) and known(query())
            for name in sorted(p.name for p in catalog.iterdir() if p.is_file()):
                file = catalog / name
                original = file.read_bytes(); info = file.stat()
                assert original, name
                try:
                    altered = original[:-1] + bytes([original[-1] ^ 1])
                    file.write_bytes(altered)
                    os.utime(file, ns=(info.st_atime_ns, info.st_mtime_ns))
                    response = query()
                    assert response.get('error', {}).get('code') == -32801, (name, response)
                    diagnostics = client.sync()
                    assert any(n['diagnostics'] for n in diagnostics), (name, diagnostics)
                    assert not known(query()), (name, 'stale native-dependent completion escaped')
                    results.append({'input': name, 'same_size_and_mtime_tamper': 'REJECTED'})
                finally:
                    file.write_bytes(original)
                    os.utime(file, ns=(info.st_atime_ns, info.st_mtime_ns))
                    client.request('void/refreshProject', {})
                    client.sync()
                assert known(query()) and known(query()), name
            while not client.begins.empty(): client.begins.get_nowait()
            running = []
            for trial in range(8):
                client.request('void/refreshProject', {}); client.sync()
                request_id = 'running-' + str(trial)
                signature = dict(params, position={'line': 2, 'character': len('    return helper.known(')})
                client.write(__import__('lsp_client').frame({'jsonrpc': '2.0', 'id': request_id,
                    'method': 'textDocument/signatureHelp', 'params': signature}))
                # Observe real compiler work beginning, not a guessed sleep.
                client.begins.get(timeout=10)
                start = time.perf_counter()
                client.notify('$/cancelRequest', {'id': request_id})
                response, _ = client.response(request_id)
                cancelled = response.get('error', {}).get('code') == -32800
                running.append({'trial': trial, 'cancelled_in_flight': cancelled,
                    'reply_ms': (time.perf_counter() - start) * 1000, 'response': response})
            assert sum(case['cancelled_in_flight'] for case in running) >= 3, running
            client.shutdown()
    print(json.dumps({'status': 'PASS', 'cases': results, 'observed_running_cancellation': running,
        'native_execution': 'NOT RUN'}, indent=2))


if __name__ == '__main__':
    run(*(Path(value).resolve() for value in sys.argv[1:4]))
