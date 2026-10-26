"""Verify synthetic preservation in disposable storage, including editor inputs."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import sys
import tempfile
from crash import ROOT, run, install


def snapshot(directory):
    return {path.relative_to(directory) for path in directory.rglob('*') if path.is_file()}


def exercise(root):
    original = root / 'arbitrary_name.void'
    original.write_bytes(b'# synthetic preservation input\r\nfunc main():\r\n    print("x")\r\n')
    companion = root / 'nested' / original.name
    companion.parent.mkdir()
    companion.write_bytes(b'# synthetic companion with a colliding filename\r\n')
    expected = hashlib.sha256(original.read_bytes()).hexdigest()
    kept = root / 'evidence'
    os.environ['VOID_CRASH_EVIDENCE'] = str(kept)
    script = 'import os,signal; os.kill(os.getpid(), signal.SIGSEGV)'
    if os.name == 'nt':
        script = 'import os; os.abort()'
    else:
        script = 'import resource; resource.setrlimit(resource.RLIMIT_CORE,(0,0)); ' + script
    executable = Path(sys.executable).resolve()
    try:
        selected_executable = os.path.relpath(executable, root)
    except ValueError:
        # Windows can place the interpreter and temporary storage on different drives.
        selected_executable = str(executable)
    first = run([selected_executable, '-c', script, original], cwd=root, capture_output=True,
                source_inputs=(companion,), provenance_inputs=(Path(__file__).resolve(),))
    assert first.returncode != 0
    try:
        run([sys.executable, '-c', script, original], cwd=root, capture_output=True, check=True)
        raise AssertionError('checked crash did not raise')
    except subprocess.CalledProcessError as failure:
        assert failure.returncode == first.returncode
    edited = 'func main():\n    print("雪\\0")\n'
    message = json.dumps({'jsonrpc': '2.0', 'method': 'textDocument/didOpen',
                          'params': {'textDocument': {'uri': original.as_uri(),
                                                     'version': 2, 'text': edited}}}).encode()
    framed = b'Content-Length: ' + str(len(message)).encode() + b'\r\n\r\n' + message
    direct = subprocess.Popen([sys.executable, '-c', 'import sys; sys.stdin.buffer.read(); ' + script,
                               str(original)], cwd=root, stdin=subprocess.PIPE,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    direct.communicate(framed)
    rejected = run([sys.executable, '-c', 'raise SystemExit(1)', original],
                   cwd=root, capture_output=True)
    assert rejected.returncode == 1
    receipts = list(kept.glob('*/receipt.json'))
    assert len(receipts) == 3
    collisions = 0
    for receipt in receipts:
        record = json.loads(receipt.read_text())
        entry = next(item for item in record['sources'] if item['original'] == str(original))
        assert entry['sha256'] == expected
        assert entry['original_sha256'] == entry['preserved_sha256'] == expected
        copy = Path(entry['copy'])
        assert copy.name == 'arbitrary_name_crash.void'
        assert copy.read_bytes() == original.read_bytes()
        assert copy.stat().st_mode & 0o222 == 0
        assert record['executable_sha256'] == hashlib.sha256(executable.read_bytes()).hexdigest()
        assert record['cwd'] == str(root)
        matching = [item for item in record['sources'] if item['original'] == str(companion)]
        if matching:
            collisions += 1
            saved = Path(matching[0]['copy'])
            assert saved.name == 'arbitrary_name_crash_2.void'
            assert saved.read_bytes() == companion.read_bytes()
        generator = next(item for item in record['provenance_inputs']
                         if item['original'] == str(Path(__file__).resolve()))
        assert Path(generator['copy']).read_bytes() == Path(__file__).read_bytes()
        assert generator['original_sha256'] == generator['preserved_sha256']
        if record.get('editor_snapshots'):
            snapshot = record['editor_snapshots'][0]
            assert Path(snapshot['copy']).read_bytes() == edited.encode()
            assert snapshot['sha256'] == hashlib.sha256(edited.encode()).hexdigest()
    assert collisions == 1
    assert sum(bool(json.loads(p.read_text()).get('editor_snapshots')) for p in receipts) == 1


install()
legacy = ROOT / 'docs/text-encoding/evidence/synthetic-preservation'
before = snapshot(legacy)
previous = os.environ.get('VOID_CRASH_EVIDENCE')
try:
    with tempfile.TemporaryDirectory(prefix='VOID crash preservation selftest ') as directory:
        disposable = Path(directory)
        exercise(disposable)
finally:
    if previous is None:
        os.environ.pop('VOID_CRASH_EVIDENCE', None)
    else:
        os.environ['VOID_CRASH_EVIDENCE'] = previous
assert not disposable.exists()
assert snapshot(legacy) == before
print(json.dumps({'intentional_incidents_verified': 3, 'checked_crash_preserved': True,
                  'colliding_filenames_verified': True, 'editor_snapshot_verified': True,
                  'ordinary_rejection_ignored': True, 'temporary_outputs_removed': True,
                  'repository_artifacts_added': 0}))
