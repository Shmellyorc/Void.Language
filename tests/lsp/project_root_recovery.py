"""Real-pipe project authority across physical root loss and restoration."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
from tempfile import TemporaryDirectory

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from lsp_client import Client

server, compiler = (Path(arg).resolve() for arg in sys.argv[1:])
manifest_text = """schema = 1
[project]
id = "org.test.recovery"
name = "Root recovery"
version = "1.0.0"
kind = "application"
[application]
id = "org.test.recovery.app"
[sources]
roots = ["src"]
[dependencies.helper]
id = "org.test.recovery.helper"
path = "deps/helper"
version = "=1.0.0"
"""
helper_manifest = """schema = 1
[project]
id = "org.test.recovery.helper.project"
name = "Helper"
version = "1.0.0"
kind = "library"
[package]
id = "org.test.recovery.helper"
name = "helper"
entry = "api.void"
"""
source = """use pkg/helper
value struct point:
    count: i32
## @param amount Value to return.
func identity(amount: i32) -> i32:
    return amount
func main() -> i32:
    const p = point(count = helper.value())
    return identity(p.count)
"""


def write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(text.encode('utf-8'))
    return path


def cli(root, command, entry=True):
    args = [compiler, command, '--project', root]
    if entry:
        args += ['--entry', 'src/main.void']
    return subprocess.run(args, capture_output=True, timeout=30)


def position(text, needle, delta=0):
    prefix = text[:text.index(needle) + delta]
    return {'line': prefix.count('\n'),
            'character': len(prefix.rsplit('\n', 1)[-1].encode('utf-16-le')) // 2}


def init(client):
    capabilities = {'textDocument': {
        'publishDiagnostics': {'versionSupport': True},
        'semanticTokens': {'requests': {'full': True}, 'formats': ['relative']},
    }}
    response, _ = client.request('initialize', {'capabilities': capabilities})
    assert response['result']['capabilities']['positionEncoding'] == 'utf-16', response
    assert response['result']['capabilities']['semanticTokensProvider']['full'], response
    client.notify('initialized', {})
    client.sync()


def request(client, method, params=None):
    response, notes = client.request(method, params)
    assert 'result' in response, response
    return response['result'], notes


def doc_query(client, method, path, text=None, needle=None, delta=0):
    params = {'textDocument': {'uri': path.as_uri()}}
    if needle is not None:
        params['position'] = position(text, needle, delta)
    return request(client, 'textDocument/' + method, params)


def completion(client, path, text):
    return doc_query(client, 'completion', path, text, 'p.count', 2)


def state(client):
    return request(client, 'void/projectState', {})[0]


def error_state(client, code):
    current = state(client)
    assert current['status'] == 'error' and current['requiresReselection'], current
    assert current['entry'] is None and current['executionCount'] == 0, current
    assert current['discoveryCount'] == 0 and current['snapshotKey'] == '', current
    assert [item['code'] for item in current['diagnostics']] == [code], current
    return current


def ownership(notes, manifest, document, code, version=2):
    project = [n for n in notes if n['uri'] == manifest.as_uri()]
    opened = [n for n in notes if n['uri'] == document.as_uri()]
    assert len(project) == len(opened) == 1, notes
    assert [d['code'] for d in project[0]['diagnostics']] == [code], notes
    assert project[0]['version'] == 1 and opened[0]['version'] == version, notes
    assert opened[0]['diagnostics'] == [], notes
    assert project[0]['voidProjectRevision'] == opened[0]['voidProjectRevision'], notes


def withheld(client, path, text):
    assert completion(client, path, text)[0] == []
    assert doc_query(client, 'hover', path, text, 'p.count', 2)[0] is None
    assert doc_query(client, 'signatureHelp', path, text, 'identity(p.', 9)[0] is None
    assert doc_query(client, 'semanticTokens/full', path)[0] == {'data': []}
    assert doc_query(client, 'documentSymbol', path)[0] == []
    assert request(client, 'workspace/symbol', {'query': 'point'})[0] == []
    assert client.process.poll() is None


def rename_root(root, moved):
    try:
        root.rename(moved)
        return True
    except OSError as error:
        if os.name != 'nt' or error.winerror not in (5, 32):
            raise
        print(f'Physical directory rename refused by Windows: {error!r}')
        assert root.is_dir() and not moved.exists()
        return False


with TemporaryDirectory(prefix='VOID LSP root recovery ') as directory:
    base = Path(directory).resolve()
    root = base / 'project'
    manifest = write(root / 'void.project.toml', manifest_text)
    entry = write(root / 'src/main.void', source)
    alt_text = source.replace('count', 'other_count')
    alt = write(root / 'src/alternate.void', alt_text)
    package_source = write(root / 'deps/helper/api.void', 'public func value() -> i32:\n    return 7\n')
    write(root / 'deps/helper/void.project.toml', helper_manifest)
    resolved = cli(root, 'resolve', entry=False)
    assert resolved.returncode == 0, resolved
    lock = root / 'void.lock'
    valid_lock = lock.read_bytes()
    independent = shutil.copytree(root, base / 'independent')
    dirty = source.replace('    return identity', '    const label = "😀"\n    return identity')
    moved = base / 'original'
    with Client(server, root, project=True, entry_override='src/main.void') as client, \
         Client(server, independent, project=True, entry_override='src/main.void') as other:
        init(client)
        init(other)
        client.notify('textDocument/didOpen', {'textDocument': {
            'uri': entry.as_uri(), 'languageId': 'void', 'version': 1, 'text': source}})
        client.notify('textDocument/didOpen', {'textDocument': {
            'uri': manifest.as_uri(), 'languageId': 'toml', 'version': 1, 'text': manifest_text}})
        client.notify('textDocument/didChange', {'textDocument': {'uri': entry.as_uri(), 'version': 2},
                                                 'contentChanges': [{'text': dirty}]})
        notes = client.sync()
        assert all(not n['diagnostics'] for n in notes), notes
        initial = state(client)
        assert initial['status'] == 'ready', initial
        before, _ = completion(client, entry, dirty)
        assert [item['label'] for item in before] == ['count'], before
        assert before[0]['textEdit']['range']['start'] == position(dirty, 'p.count', 2)
        assert doc_query(client, 'hover', entry, dirty, 'p.count', 2)[0]
        assert doc_query(client, 'signatureHelp', entry, dirty, 'identity(p.', 9)[0]
        assert doc_query(client, 'semanticTokens/full', entry)[0]['data']

        if rename_root(root, moved):
            lost, notes = completion(client, entry, dirty)
            assert lost == [], lost
            ownership(notes, manifest, entry, 'VOID5009')
            unavailable = error_state(client, 'VOID5009')
            assert int(unavailable['selectionRevision']) > int(initial['selectionRevision'])
            withheld(client, entry, dirty)
            assert cli(root, 'check').returncode != 0
            assert [item['label'] for item in completion(other, independent / 'src/main.void', source)[0]] == ['count']

            # The pathname, project IDs, manifest, lock and sources all match.
            # Only the physical identity distinguishes this replacement.
            shutil.copytree(moved, root)
            _, notes = request(client, 'void/refreshProject', {})
            ownership(notes, manifest, entry, 'VOID5011')
            error_state(client, 'VOID5011')
            withheld(client, entry, dirty)
            request(client, 'void/reselectProject', {'entry': 'src/alternate.void'})
            error_state(client, 'VOID5011')
            assert doc_query(client, 'completion', alt, alt_text, 'p.other_count', 2)[0] == []
            request(client, 'void/reselectProject', {'entry': 'src/main.void'})
            shutil.rmtree(root)
            moved.rename(root)
            assert completion(client, entry, dirty)[0] == [], 'refresh is the recovery barrier'
            _, notes = request(client, 'void/refreshProject', {})
            assert all(not n['diagnostics'] for n in notes), notes
            assert state(client)['status'] == 'ready'
            assert completion(client, entry, dirty)[0] == before
            # The first request can also arrive after the replacement is
            # already in place, without a prior failed root observation.
            assert rename_root(root, moved)
            shutil.copytree(moved, root)
            withheld(client, entry, dirty)
            error_state(client, 'VOID5011')
            shutil.rmtree(root)
            moved.rename(root)
            request(client, 'void/refreshProject', {})
            assert completion(client, entry, dirty)[0] == before
            print('Physical root rename, byte-identical replacement and restoration passed')
        else:
            # Windows can refuse a directory rename even with delete sharing.
            # Exercise manifest invalidation below using the real filesystem.
            print('Physical root identity scenario unsupported by the native rename result')

        if os.name != 'nt':
            mode = root.stat().st_mode & 0o777
            try:
                root.chmod(0)
                try:
                    list(root.iterdir())
                except PermissionError:
                    lost, notes = completion(client, entry, dirty)
                    assert lost == [], lost
                    ownership(notes, manifest, entry, 'VOID5009')
                    error_state(client, 'VOID5009')
                else:
                    print('Host permissions allowed listing mode-000 root; access refusal unavailable')
            finally:
                root.chmod(mode)
            request(client, 'void/refreshProject', {})
            assert completion(client, entry, dirty)[0] == before

        # Physical absence of the manifest is tested after closing its overlay.
        client.notify('textDocument/didClose', {'textDocument': {'uri': manifest.as_uri()}})
        client.sync()
        manifest.unlink()
        assert completion(client, entry, dirty)[0] == []
        _, notes = request(client, 'void/refreshProject', {})
        assert any(n['uri'] == manifest.as_uri() and
                   [d['code'] for d in n['diagnostics']] == ['VOID5009'] for n in notes), notes
        withheld(client, entry, dirty)
        write(manifest, manifest_text)
        _, notes = request(client, 'void/refreshProject', {})
        assert all(not n['diagnostics'] for n in notes), notes
        assert state(client)['status'] == 'ready'
        assert completion(client, entry, dirty)[0] == before

        # Withhold candidates immediately, before any new editor revision.
        lock.write_bytes(b'invalid lock\n')
        assert completion(client, entry, dirty)[0] == []
        _, notes = request(client, 'void/refreshProject', {})
        assert any(n['uri'] == manifest.as_uri() and
                   [d['code'] for d in n['diagnostics']] == ['VOID5011'] for n in notes), notes
        assert state(client)['status'] == 'error'
        assert cli(root, 'check').returncode != 0
        lock.write_bytes(valid_lock)
        original_package = package_source.read_bytes()
        package_source.write_bytes(original_package.replace(b'return 7', b'return 8'))
        assert completion(client, entry, dirty)[0] == []
        assert cli(root, 'check').returncode != 0
        package_source.write_bytes(original_package)
        request(client, 'void/refreshProject', {})
        assert completion(client, entry, dirty)[0] == before

        selected, _ = request(client, 'void/reselectProject', {'entry': 'src/alternate.void'})
        assert selected['entry'] == alt.as_posix() and selected['status'] == 'ready', selected
        alternate = doc_query(client, 'completion', alt, alt_text, 'p.other_count', 2)[0]
        assert [item['label'] for item in alternate] == ['other_count'], alternate
        assert entry.read_bytes() == source.encode('utf-8'), 'dirty overlay touched disk'
        client.shutdown()
        other.shutdown()
print('Project root recovery, diagnostics, entry selection and dependency authority passed')
