"""G76 editor contracts: readable metadata, exact edits, freshness and queued work."""
from pathlib import Path
import os
import subprocess
import sys
from tempfile import TemporaryDirectory
import tomllib

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from lsp_client import Client, frame


class InteractiveClient(Client):
    def receive(self):
        while True:
            message = super().receive()
            if message.get('method') != 'workspace/semanticTokens/refresh':
                return message
            self.write(frame({'jsonrpc': '2.0', 'id': message['id'], 'result': None}))


def run(server, compiler):
    with TemporaryDirectory(prefix='VOID G76 é😀 ') as temporary:
        root = Path(temporary) / 'app'
        library = root / 'libs/library'
        def cli(*args):
            result=subprocess.run([str(compiler), *map(str, args)], capture_output=True, timeout=30)
            assert result.returncode==0,(args,result.returncode,(result.stdout+result.stderr).decode(errors='replace'))
        cli('new', root)
        library.parent.mkdir()
        cli('new', library, '--kind', 'library', '--package-name', 'library', '--nested')
        metadata = tomllib.loads((library / 'void.project.toml').read_text())
        (library / metadata['package']['entry']).write_text(
            '## Combine two values.\n## @param left First value.\n## @param right Second value.\n'
            '## @return Combined value.\npublic func blend(left: i32, right: i32) -> i32:\n    return left + right\n'
            'public func other() -> i32:\n    return 3\n', encoding='utf-8')
        readme = library / 'README.md'
        readme.write_text('package documentation v1\n')
        manifest = root / 'void.project.toml'
        manifest.write_text(manifest.read_text() + '\n[dependencies.lib]\n'
            f'id = "{metadata["package"]["id"]}"\npath = "libs/library"\nversion = "=0.1.0"\n')
        cli('resolve', '--project', root)
        model = root / 'src/model.void'
        model_text = 'public value struct point:\n    public x: i32\n'
        model.write_text(model_text)
        path = root / 'src/app.void'
        good = ('use pkg/lib as lib\nuse "./model.void" as model\nfunc main() -> i32:\n'
                '    var item = model.point()\n    var value = lib.blend(1, 2)\n'
                '    var result = item.x\n    return value + result\n')
        path.write_text(good)
        with InteractiveClient(server, root, project=True) as client:
            response, _ = client.request('initialize', {
                'capabilities': {'textDocument': {
                    'signatureHelp': {'signatureInformation': {'parameterInformation': {'labelOffsetSupport': True}}},
                    'semanticTokens': {'requests': {'full': True}, 'formats': ['relative']},
                    'publishDiagnostics': {'versionSupport': True}},
                    'workspace': {'semanticTokens': {'refreshSupport': True}}},
                'initializationOptions': {'voidInteractivePresentation': True, 'voidIncrementalSync': True}})
            caps = response['result']['capabilities']
            assert caps['textDocumentSync']['change'] == 2
            legend = caps['semanticTokensProvider']['legend']['tokenTypes']
            client.notify('initialized', {}); client.sync()
            version = 1
            text = good
            client.notify('textDocument/didOpen', {'textDocument': {'uri': path.as_uri(), 'version': version,
                          'languageId': 'void', 'text': text}})
            assert not [d for note in client.sync() for d in note['diagnostics']]

            def point(needle, content=None):
                content = text if content is None else content
                prefix = content[:content.index(needle) + len(needle)]
                return {'line': prefix.count('\n'), 'character': len(prefix.rsplit('\n', 1)[-1].encode('utf-16-le')) // 2}
            def query(method, needle=None):
                params = {'textDocument': {'uri': path.as_uri()}}
                if needle is not None: params['position'] = point(needle)
                result, _ = client.request('textDocument/' + method, params)
                assert 'result' in result, result
                return result['result']
            def items(needle):
                result = query('completion', needle)
                return result['items'] if isinstance(result, dict) else result
            def replace(value, sync=True):
                nonlocal version, text
                version += 1; text = value
                client.notify('textDocument/didChange', {'textDocument': {'uri': path.as_uri(), 'version': version},
                              'contentChanges': [{'text': text}]})
                return client.sync() if sync else []
            def roles():
                data = query('semanticTokens/full')['data']
                lines = text.splitlines(); line = column = 0; result = set()
                for n in range(0, len(data), 5):
                    delta, start, width, kind, _ = data[n:n+5]
                    line += delta; column = start if delta else column + start
                    word = lines[line].encode('utf-16-le')[column*2:(column+width)*2].decode('utf-16-le')
                    result.add((line, word, legend[kind]))
                return result

            for _ in range(3):
                blend = next(i for i in items('lib.') if i['label'] == 'blend')
                assert blend['detail'] == 'blend(left: i32, right: i32) -> i32', blend
                assert 'Combine two values.' in blend['documentation']
                assert '\n' not in blend['detail'] and '.void::' not in blend['detail']
            assert query('hover', 'var ite')['contents']['value'].startswith('item: point')
            assert query('definition', 'item.')['uri'] == model.as_uri()

            replace(good.replace('lib.blend(1, 2)', 'lib.blend(right = 2, left = '))
            signature = query('signatureHelp', 'left = ')['signatures'][0]
            result = query('signatureHelp', 'left = ')
            assert result['activeParameter'] == 0, result
            assert signature['label'] == 'blend(left: i32, right: i32) -> i32'
            for parameter, expected in zip(signature['parameters'], ['left: i32', 'right: i32']):
                begin, end = parameter['label']
                assert signature['label'].encode('utf-16-le')[begin*2:end*2].decode('utf-16-le') == expected
            assert 'First value.' in signature['parameters'][0]['documentation']

            replace(good); original_roles = roles()
            replace(good.replace('blend(1, 2)', ''), sync=False)
            assert (4, 'lib', 'namespace') in roles(), 'unchanged namespace dropped on edited line'
            assert 'blend' in {i['label'] for i in items('lib.')}
            replace(good)
            for suffix in ['.', '.b', '.bl', '.', '.o']:
                replace(good + '    lib' + suffix, sync=False)
                assert ('other' if suffix == '.o' else 'blend') in {i['label'] for i in items('    lib' + suffix)}
            replace(good)
            assert roles() == original_roles

            # Ranged changes are sequential, UTF-16 exact and atomically admitted.
            replace('# é😀\r\n' + good)
            version += 1
            client.notify('textDocument/didChange', {'textDocument': {'uri': path.as_uri(), 'version': version},
                'contentChanges': [{'range': {'start': {'line': 0, 'character': 4}, 'end': {'line': 0, 'character': 4}}, 'text': '!'}]})
            client.sync()  # Surrogate interior rejects without consuming the version.
            client.notify('textDocument/didChange', {'textDocument': {'uri': path.as_uri(), 'version': version},
                'contentChanges': [
                    {'range': {'start': {'line': 0, 'character': 5}, 'end': {'line': 0, 'character': 5}}, 'rangeLength': 0, 'text': '!'},
                    {'range': {'start': {'line': 0, 'character': 6}, 'end': {'line': 0, 'character': 6}}, 'text': '?'}]})
            text = '# é😀!?\r\n' + good
            client.sync()
            assert 'blend' in {i['label'] for i in items('lib.')}
            client.notify('textDocument/didChange', {'textDocument': {'uri': path.as_uri(), 'version': version - 1},
                          'contentChanges': [{'text': 'broken'}]})
            assert 'blend' in {i['label'] for i in items('lib.')}
            version += 1
            client.notify('textDocument/didChange', {'textDocument': {'uri': path.as_uri(), 'version': version},
                'contentChanges': [{'text': 'broken'}, {'range': {'start': {'line': 9, 'character': 0},
                    'end': {'line': 9, 'character': 0}}, 'text': 'also broken'}]})
            client.sync()
            assert 'blend' in {i['label'] for i in items('lib.')}, 'partial malformed edit was applied'
            client.notify('textDocument/didChange', {'textDocument': {'uri': path.as_uri(), 'version': version},
                          'contentChanges': [{'text': text}]})
            client.sync()

            # A closed-file change is observed without relying on a watcher or mtime.
            assert {i['label'] for i in items('item.')} == {'x'}
            old_stat = model.stat()
            model.write_text(model_text.replace('public x:', 'public y:'))
            os.utime(model, ns=(old_stat.st_atime_ns, old_stat.st_mtime_ns))
            response, _ = client.request('textDocument/completion', {'textDocument': {'uri': path.as_uri()}, 'position': point('item.')})
            assert response.get('error', {}).get('code') == -32801, response
            client.sync()
            assert {i['label'] for i in items('item.')} == {'y'}
            model.write_text(model_text)
            client.notify('workspace/didChangeWatchedFiles', {'changes': [{'uri': model.as_uri(), 'type': 2}]})
            client.sync()
            assert {i['label'] for i in items('item.')} == {'x'}

            # Whole package digest reuse includes non-imported documentation.
            assert 'blend' in {i['label'] for i in items('lib.')}
            readme_before=readme.read_bytes(); readme_stat=readme.stat()
            readme.write_bytes(readme_before.replace(b'v1',b'v2'))
            os.utime(readme,ns=(readme_stat.st_atime_ns,readme_stat.st_mtime_ns))
            response, _ = client.request('textDocument/completion', {'textDocument': {'uri': path.as_uri()}, 'position': point('lib.')})
            assert response.get('error', {}).get('code') == -32801, response
            client.sync()
            response, _ = client.request('textDocument/completion', {'textDocument': {'uri': path.as_uri()}, 'position': point('lib.')})
            assert response.get('error', {}).get('code') == -32801, 'unlocked package cannot produce a verified async result'
            readme.write_bytes(readme_before)
            os.utime(readme,ns=(readme_stat.st_atime_ns,readme_stat.st_mtime_ns))
            client.request('void/refreshProject', {}); client.sync()
            assert 'blend' in {i['label'] for i in items('lib.')}

            # Identical package bytes do not authorize a hard-linked manifest.
            alias = Path(temporary) / 'package-manifest-alias'
            os.link(library / 'void.project.toml', alias)
            try:
                response, _ = client.request('textDocument/completion', {
                    'textDocument': {'uri': path.as_uri()}, 'position': point('lib.')})
                assert response.get('error', {}).get('code') == -32801, response
                rejected = client.sync()
                assert any('hard-link' in diagnostic['message'].lower()
                           for note in rejected for diagnostic in note['diagnostics']), rejected
                response, _ = client.request('textDocument/completion', {
                    'textDocument': {'uri': path.as_uri()}, 'position': point('lib.')})
                # Invalid project selection can reject before an async job is
                # created. It must publish no package suggestions in either path.
                assert response.get('error', {}).get('code') == -32801 or response.get('result') == [], response
            finally:
                alias.unlink()
            client.request('void/refreshProject', {}); client.sync()
            assert 'blend' in {i['label'] for i in items('lib.')}

            # A new valid import/body must use its own inputs, not be compared
            # indiscriminately to the previous successful project's graph.
            (root / 'src/alternate.void').write_text('public value struct point:\n    public z: i32\n')
            previous = text
            replace(good.replace('./model.void', './alternate.void').replace('item.x', 'item.z'), sync=False)
            assert {i['label'] for i in items('item.')} == {'z'}
            replace(previous)
            assert {i['label'] for i in items('item.')} == {'x'}
            previous = text
            replace('fu', sync=False)
            assert 'func' in {i['label'] for i in items('fu')}, 'verified declaration keyword recovery was lost'
            replace(previous)

            # Cancellation does not poison later ID reuse; queued obsolete work
            # gets a response and cannot publish a result for another revision.
            params = {'textDocument': {'uri': path.as_uri()}, 'position': point('lib.')}
            client.write(frame({'jsonrpc': '2.0', 'id': 'cancelled', 'method': 'textDocument/completion', 'params': params}) +
                         frame({'jsonrpc': '2.0', 'method': '$/cancelRequest', 'params': {'id': 'cancelled'}}))
            assert client.response('cancelled')[0]['error']['code'] == -32800
            response, _ = client.request('textDocument/completion', params, request_id='cancelled')
            assert response.get('result'), response
            client.notify('$/cancelRequest', {'id': 'not-yet-used'})
            assert client.request('textDocument/completion', params, request_id='not-yet-used')[0].get('result')
            version += 1; text += '# next version\n'
            client.write(frame({'jsonrpc': '2.0', 'id': 'obsolete', 'method': 'textDocument/signatureHelp', 'params': params}) +
                frame({'jsonrpc': '2.0', 'method': 'textDocument/didChange', 'params': {
                    'textDocument': {'uri': path.as_uri(), 'version': version}, 'contentChanges': [{'text': text}]}}))
            assert client.response('obsolete')[0]['error']['code'] == -32801
            client.sync()
            client.write(frame({'jsonrpc': '2.0', 'id': 'old-selection', 'method': 'textDocument/signatureHelp', 'params': params}) +
                frame({'jsonrpc': '2.0', 'id': 'reselection', 'method': 'void/reselectProject', 'params': {'entry': 'src/app.void'}}))
            assert client.response('old-selection')[0]['error']['code'] == -32801
            assert client.response('reselection')[0]['result']['status'] == 'ready'
            assert 'blend' in {i['label'] for i in items('lib.')}
            client.sync(); client.shutdown()
        with InteractiveClient(server, root, project=True) as client:
            client.request('initialize', {'capabilities': {'textDocument': {'semanticTokens': {
                'requests': {'full': True}, 'formats': ['relative']}},
                'workspace': {'semanticTokens': {'refreshSupport': True}}},
                'initializationOptions': {'voidInteractivePresentation': True}})
            client.notify('initialized', {}); client.sync()
            client.notify('textDocument/didOpen', {'textDocument': {'uri': path.as_uri(), 'languageId': 'void',
                'version': 1, 'text': good}}); client.sync()
            client.write(frame({'jsonrpc': '2.0', 'id': 'input-closed', 'method': 'textDocument/signatureHelp',
                'params': {'textDocument': {'uri': path.as_uri()}, 'position': {'line': 4, 'character': len('    var value = lib.blend(')}}}))
            client.process.stdin.close()
            assert client.response('input-closed')[0]['error']['code'] == -32800
            client.finish(expected=1)
    print('G76: compact metadata, offsets, edited-line tokens, EOF typing, UTF-16 edits, stale versions, disk freshness and cancellation passed')


if __name__ == '__main__':
    run(*(Path(p).resolve() for p in sys.argv[1:3]))
