"""Native process capture, synchronous source JIT/AOT and real async jobs."""
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

compiler, probe, stager, runner, async_binary = (Path(value).resolve() for value in sys.argv[1:])
fixtures = Path(__file__).resolve().parent


def run(arguments, work, status=0):
    result = subprocess.run(list(map(str, arguments)), cwd=work, capture_output=True, timeout=90,
                            env=os.environ | {'VOID_PARENT': 'must not leak'})
    assert result.returncode == status, (arguments, result.returncode, result.stdout, result.stderr)
    assert not any(error in result.stderr for error in (b'AddressSanitizer', b'LeakSanitizer', b'runtime error:')), result.stderr
    return result


with tempfile.TemporaryDirectory(prefix='VOID capture 雪 ') as temporary:
    work = Path(temporary).resolve()
    destination = work / 'approved child 雪'
    if os.name == 'nt':
        digest = hashlib.sha256(probe.read_bytes()).hexdigest()
        run([stager, '--stage-executable', probe, destination, digest], work)
        child = destination / 'child.exe'
        assert hashlib.sha256(child.read_bytes()).hexdigest() == digest
    else:
        destination.mkdir()
        child = destination / 'child fixture'
        shutil.copy2(probe, child)
    run([probe, '--native', child, work], work)
    source = fixtures / 'process_capture_sync.void'
    for flags in (['--no-opt'], []):
        run([compiler, 'run', source, *flags, '--no-cache', '--', child, work], work)
        application = work / ('capture app.exe' if os.name == 'nt' else 'capture app')
        run([compiler, 'build', source, *flags, '-o', application], work)
        run([application, child, work], work)

    marker = work / 'async marker'
    for name, value in (('image', str(child)), ('cwd', str(work)), ('marker', str(marker)), ('mode', 'group')):
        (work / f'capture-{name}.txt').write_bytes(value.encode())
    for command, expected in (([runner, '--async', fixtures / 'process_capture_async.void'], 4),
                              ([async_binary], 2)):
        result = run(command, work)
        ids = re.findall(rb'^drained (\d+)\r?$', result.stdout, re.M)
        assert len(ids) == expected, result.stdout
        for pid in ids:
            run([probe, '--reaped', pid.decode()], work)
    (work / 'capture-mode.txt').write_bytes(b'await')
    result = run([runner, '--async', fixtures / 'process_capture_async.void'], work, status=1)
    assert b'Async task cancelled' in result.stderr, result.stderr
    run([probe, '--reaped', marker.read_text()], work)
    print('process capture: native ownership, normal/O2 JIT, linked AOT, async progress and cancellation passed')
