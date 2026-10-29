"""Verify synthetic preservation in disposable storage, including editor inputs."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import sys
import tempfile
from types import SimpleNamespace
from crash import ROOT, crashed, preserve, run, install


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
        script = 'import ctypes; ctypes.windll.kernel32.ExitProcess(0xC0000005)'
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
    fatal_stdout = b'synthetic fatal stdout\0\r\n'
    fatal_stderr = b'synthetic fatal stderr\0\r\n'
    for status in (87, 86):
        fatal = ('import os; '
                 f'os.write(1, {fatal_stdout!r}); '
                 f'os.write(2, {fatal_stderr!r}); '
                 f'os._exit({status})')
        arguments = [sys.executable, '-c', fatal, original]
        result = run(arguments, cwd=root, capture_output=True,
                     provenance_inputs=(Path(__file__).resolve(),))
        assert result.returncode == status
        direct_fatal = subprocess.Popen(arguments, cwd=root, stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE)
        assert direct_fatal.communicate() == (fatal_stdout, fatal_stderr)
        assert direct_fatal.returncode == status
    # Exercise signed Windows receipt creation on every host. This is a status
    # simulation, not a claim that the Windows native branch ran on POSIX.
    windows_status = -1073741819
    assert crashed(windows_status) and crashed(0xC0000005)
    preserve([sys.executable, '--simulated-signed-windows-status', original], root,
             SimpleNamespace(returncode=windows_status, stdout=fatal_stdout, stderr=fatal_stderr),
             provenance_inputs=(Path(__file__).resolve(),))
    for status in (0, 1, 2, 3, 42, 85, 88):
        assert not crashed(status), status
    rejected = run([sys.executable, '-c', 'raise SystemExit(1)', original],
                   cwd=root, capture_output=True)
    assert rejected.returncode == 1
    receipts = list(kept.glob('*/receipt.json'))
    assert len(receipts) == 8
    collisions = 0
    fatal_counts = {87: 0, 86: 0, windows_status: 0}
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
        if record['returncode'] in fatal_counts:
            fatal_counts[record['returncode']] += 1
            assert record['signal'] is None
            assert (receipt.parent / 'stdout.bin').read_bytes() == fatal_stdout
            assert (receipt.parent / 'stderr.bin').read_bytes() == fatal_stderr
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
    # Recursive project companions are now retained on every argv-source run.
    assert collisions == 8
    assert fatal_counts == {87: 2, 86: 2, windows_status: 1}
    assert sum(bool(json.loads(p.read_text()).get('editor_snapshots')) for p in receipts) == 1

    # Ordinary diagnostics stay ordinary; only an unmet execution expectation
    # records an incident. Preserve before the temporary source scope is removed.
    baseline = set(receipts)
    context = {'profile': 'synthetic', 'mode': 'runner', 'hard_stack_bytes': 1048576}
    rejected = run([sys.executable, '-c', 'raise SystemExit(1)', original],
                   cwd=root, capture_output=True, expected_returncode=1,
                   evidence_context={'mode': 'expected_diagnostic'})
    assert not rejected.expectation_failed
    diagnostic = run([sys.executable, '-c',
                      'import sys; sys.stderr.write("case.void:1:2: runtime error: expected\\n"); sys.exit(1)', original],
                     cwd=root, capture_output=True, expected_returncode=1)
    assert not diagnostic.expectation_failed
    assert set(kept.glob('*/receipt.json')) == baseline
    cases = [
        ('unexpected_exit', 'raise SystemExit(1)', 0, None),
        ('test_assertion', 'print("wrong")', 0, b'correct\n'),
        ('sanitizer_finding', 'import sys; sys.stderr.write("ERROR: LeakSanitizer: synthetic\\n")', 0, None),
    ]
    for kind, script_text, status, output in cases:
        before_case = set(kept.glob('*/receipt.json'))
        result = run([sys.executable, '-c', script_text, original], cwd=root,
                     capture_output=True, expected_returncode=status, expected_stdout=output,
                     source_inputs=(companion.parent,), evidence_context=context)
        added = set(kept.glob('*/receipt.json')) - before_case
        assert len(added) == 1
        data = json.loads(added.pop().read_text())
        assert data['failure_kind'] == kind and data['context'] == context
        assert result.expectation_failed
        assert any(s['original'] == str(companion) for s in data['sources'])
    before_case = set(kept.glob('*/receipt.json'))
    try:
        run([sys.executable, '-c', 'import time; time.sleep(60)', original], cwd=root,
            capture_output=True, timeout=0.1, evidence_context=context)
        raise AssertionError('timeout did not raise')
    except subprocess.TimeoutExpired:
        added = set(kept.glob('*/receipt.json')) - before_case
        assert len(added) == 1
        data = json.loads(added.pop().read_text())
        assert data['failure_kind'] == 'timeout' and data['timed_out']
        assert data['context'] == context
    run([sys.executable, '-c', 'print("correct")', original], cwd=root, capture_output=True,
        expected_returncode=0, expected_stdout=b'correct\n')
    assert len(list(kept.glob('*/receipt.json'))) == len(baseline) + 4
    before_case = set(kept.glob('*/receipt.json'))
    raw = original.read_bytes()
    run([sys.executable, '-c', 'import pathlib,sys; pathlib.Path(sys.argv[1]).write_text("changed"); sys.exit(1)', original],
        cwd=root, capture_output=True, expected_returncode=0)
    added = set(kept.glob('*/receipt.json')) - before_case
    assert len(added) == 1
    data = json.loads(added.pop().read_text())
    entry = next(item for item in data['sources'] if item['original'] == str(original))
    assert Path(entry['copy']).read_bytes() == raw and entry['captured_at'] == 'subprocess_launch'
    assert entry['current_sha256'] != entry['original_sha256']
    original.write_bytes(raw)

    # Both process entry points must honor subprocess's env=None contract before
    # launch snapshots, while preserving the distinction from an empty mapping.
    key = 'VOID_CRASH_RECORD_ENV_TEST'
    previous_value = os.environ.get(key)
    os.environ[key] = 'inherited'
    environment_script = 'import os; print(os.environ.get("' + key + '", "empty"))'
    try:
        for environment, expected_output in [(None, b'inherited\n'), ({}, b'empty\n')]:
            result = run([sys.executable, '-c', environment_script, original], cwd=root,
                         env=environment, capture_output=True, expected_returncode=0,
                         expected_stdout=expected_output)
            assert not result.expectation_failed
            child = subprocess.Popen([sys.executable, '-c', environment_script, original],
                                     cwd=root, env=environment, stdout=subprocess.PIPE,
                                     stderr=subprocess.PIPE)
            output, errors = child.communicate(timeout=10)
            assert child.returncode == 0 and output == expected_output and not errors
    finally:
        if previous_value is None:
            os.environ.pop(key, None)
        else:
            os.environ[key] = previous_value
    assert len(list(kept.glob('*/receipt.json'))) == len(baseline) + 5


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
print(json.dumps({'intentional_incidents_verified': 13, 'checked_crash_preserved': True,
                  'sanitizer_exits_verified': [87, 86], 'run_and_popen_verified': True,
                  'signed_windows_receipt_simulation_verified': True,
                  'windows_native_branch_executed': os.name == 'nt',
                  'colliding_filenames_verified': True, 'editor_snapshot_verified': True,
                  'ordinary_rejection_ignored': True, 'assertion_and_timeout_preserved': True,
                  'passing_retry_keeps_original': True, 'temporary_outputs_removed': True,
                  'launch_bytes_survive_source_changes': True,
                  'none_and_empty_environments_verified': True,
                  'repository_artifacts_added': 0}))
