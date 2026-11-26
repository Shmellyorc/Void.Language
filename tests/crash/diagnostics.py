"""Native, controlled failures; never inject a test crash method into production."""
from pathlib import Path
import hashlib
import json
import os
import signal
import subprocess
import sys
import tempfile
import time

PROBE, COMPILER, SERVER = [str(Path(arg).resolve()) for arg in sys.argv[1:4]]
ROOT = Path(__file__).resolve().parents[2]
EVIDENCE = Path(os.environ['VOID_G90_EVIDENCE']) if os.environ.get('VOID_G90_EVIDENCE') else None
if EVIDENCE:
    EVIDENCE.mkdir(parents=True, exist_ok=True)
sequence = 0


def limits(stack=False):
    if os.name == 'nt':
        return None
    def apply():
        import resource
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        if stack:
            resource.setrlimit(resource.RLIMIT_STACK, (1048576, 1048576))
    return apply


def record(command, result):
    global sequence
    sequence += 1
    if EVIDENCE:
        prefix = EVIDENCE / f'{sequence:03}'
        prefix.with_suffix('.stdout').write_bytes(result.stdout or b'')
        prefix.with_suffix('.stderr').write_bytes(result.stderr or b'')
        prefix.with_suffix('.json').write_text(json.dumps({'command': command, 'status': result.returncode}) + '\n')
    return result


def run(command, env, stack=False, **options):
    result = subprocess.run(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            timeout=30, preexec_fn=limits(stack), **options)
    return record(command, result)


def reports(directory):
    return sorted(directory.glob('crash-*.json'))


def read_latest(directory):
    paths = reports(directory)
    assert paths, directory
    path = max(paths, key=lambda path: path.stat().st_mtime_ns)
    data = json.loads(path.read_text())
    assert len(path.read_bytes()) <= 16384
    assert len(data['recent_operations']) <= 32
    assert data['explanation'] is None and data['source_location'] is None
    assert data['stack'] is None and data['stack_unavailable_reason']
    return data


def children(pid):
    # Native POSIX process relationship, not a simulated termination status.
    result = subprocess.check_output(['ps', '-axo', 'pid=,ppid='], text=True)
    return [int(row[0]) for line in result.splitlines() if len(row := line.split()) == 2 and int(row[1]) == pid]


def worker(pid):
    end = time.monotonic() + 5
    while time.monotonic() < end:
        found = children(pid)
        if found:
            return found[0]
        time.sleep(.01)
    raise AssertionError('native worker not found')


def frame(method, params=None, id=None):
    value = {'jsonrpc': '2.0', 'method': method}
    if params is not None:
        value['params'] = params
    if id is not None:
        value['id'] = id
    body = json.dumps(value).encode()
    return b'Content-Length: ' + str(len(body)).encode() + b'\r\n\r\n' + body


def main(root):
    if os.name == 'nt':
        raise SystemExit('Use corrections.py with the native windows_filter fixture; unmarked Windows exceptions are unsupported.')
    directory = root / 'private reports 雪'
    env = dict(os.environ, VOID_CRASH_DIAGNOSTICS='1', VOID_CRASH_DIRECTORY=str(directory),
               G90_PRIVATE_SECRET='do-not-collect-me')
    disabled = dict(env, VOID_CRASH_DIAGNOSTICS='0')
    for code in (0, 1, 2, 42, 86, 87, 139):
        assert run([PROBE, 'exit', str(code)], env).returncode == code
    assert not directory.exists(), 'ordinary exits must not create storage'
    assert run([PROBE, 'abort'], disabled).returncode != 0
    assert not directory.exists(), 'disabled crash must not create storage'
    direct = run([PROBE, 'abort', 'command-line-secret'], env)
    assert direct.returncode != 0
    if os.name != 'nt':
        assert direct.returncode == -signal.SIGABRT
    data = read_latest(directory)
    assert data['process'] == 'probe' and data['compiler_build_identity'] == 'fixture-build'
    assert data['binary_sha256'] == hashlib.file_digest(open(PROBE, 'rb'), 'sha256').hexdigest()
    assert data['recent_operations'][-1] == 'jit_execute'
    assert b'do-not-collect-me' not in reports(directory)[0].read_bytes()
    assert b'command-line-secret' not in reports(directory)[0].read_bytes()
    if os.name != 'nt':
        assert directory.stat().st_mode & 0o777 == 0o700
        assert reports(directory)[0].stat().st_mode & 0o777 == 0o600
    for _ in range(18):
        assert run([PROBE, 'abort'], env).returncode == direct.returncode
    assert len(reports(directory)) == 16
    assert sum(path.stat().st_size for path in reports(directory)) <= 16 * 16384
    # Concurrent native crashes use a bounded, nonblocking storage lock. Busy
    # collectors may refuse storage, but must preserve every termination result.
    batch = [subprocess.Popen([PROBE, 'abort'], env=env, stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE, preexec_fn=limits()) for _ in range(6)]
    for child in batch:
        out, err = child.communicate(timeout=30)
        record([PROBE, 'abort', '(concurrent)'], subprocess.CompletedProcess([], child.returncode, out, err))
        assert child.returncode == direct.returncode
    assert len(reports(directory)) == 16
    assert run([PROBE, 'stack-thread'], env, stack=True).returncode != 0
    assert run([PROBE, 'stack'], env, stack=True).returncode != 0
    assert read_latest(directory)['classification'] in ('fatal_signal', 'sanitizer_finding')
    # Optional storage failures must never replace a native failure's exit.
    missing = root / 'missing parent' / 'reports'
    assert run([PROBE, 'abort'], dict(env, VOID_CRASH_DIRECTORY=str(missing))).returncode == direct.returncode
    if os.name != 'nt':
        public = root / 'public'; public.mkdir(mode=0o755)
        refused = run([PROBE, 'abort'], dict(env, VOID_CRASH_DIRECTORY=str(public)))
        assert refused.returncode == direct.returncode and not reports(public)
        unwritable = root / 'unwritable'; unwritable.mkdir(mode=0o500)
        try:
            assert run([PROBE, 'abort'], dict(env, VOID_CRASH_DIRECTORY=str(unwritable))).returncode == direct.returncode
            assert not reports(unwritable)
        finally:
            unwritable.chmod(0o700)
        ignored = subprocess.run([PROBE, 'exit', '42'], env=env, capture_output=True, timeout=10,
                                 preexec_fn=lambda: signal.signal(signal.SIGCHLD, signal.SIG_IGN))
        assert ignored.returncode == 42, 'inherited SIGCHLD ignore must not lose worker exit'
        redirected = root / 'redirected'; redirected.symlink_to(directory, target_is_directory=True)
        before = len(reports(directory))
        assert run([PROBE, 'abort'], dict(env, VOID_CRASH_DIRECTORY=str(redirected))).returncode == direct.returncode
        assert len(reports(directory)) == before
        crowded = root / 'crowded'; crowded.mkdir(mode=0o700)
        for i in range(257): (crowded / f'unrelated-{i}').touch()
        assert run([PROBE, 'abort'], dict(env, VOID_CRASH_DIRECTORY=str(crowded))).returncode == direct.returncode
        assert not reports(crowded) and len(list(crowded.iterdir())) == 257
        waiting = subprocess.Popen([PROBE, 'wait'], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, preexec_fn=limits())
        assert waiting.stdout.readline() == b'ready\n'
        waiting.terminate(); out, err = waiting.communicate(timeout=10)
        assert waiting.returncode == -signal.SIGTERM
        assert b'VOID crash report:' not in err
    child_dir = root / 'native child reports'
    assert run([PROBE, 'child'], dict(env, VOID_CRASH_DIRECTORY=str(child_dir))).returncode == 0
    if os.name != 'nt':
        child_data = read_latest(child_dir)
        assert child_data['process'] == 'native-tool' and child_data['pid'] is None
        assert child_data['classification'] == 'child_crash'
    else:
        assert not child_dir.exists(), 'Windows tool exit status alone is not exception evidence'
    clean = root / 'normal reports'
    normal_env = dict(env, VOID_CRASH_DIRECTORY=str(clean))
    source = root / 'main.void'
    source.write_text('func main() -> i32:\n    return 17\n')
    assert run([COMPILER, 'check', str(source)], normal_env, stack=True).returncode == 0
    for flags in ([], ['--no-opt']):
        assert run([COMPILER, 'run', str(source), *flags], normal_env, stack=True).returncode == 17
        output = root / ('app-o0' if flags else 'app-o2')
        if os.name == 'nt': output = output.with_suffix('.exe')
        assert run([COMPILER, 'build', str(source), '-o', str(output), *flags], normal_env, stack=True).returncode == 0
        assert run([COMPILER, 'crashes', 'run', str(output)], normal_env, stack=True).returncode == 17
    source.write_text('func main():\n    this is deliberately invalid\n')
    assert run([COMPILER, 'check', str(source)], normal_env).returncode != 0
    ordinary = frame('initialize', {'capabilities': {}}, 1) + frame('initialized', {}) + frame('shutdown', None, 2) + frame('exit')
    assert run([SERVER, '--entry', str(source)], normal_env, input=ordinary, stack=True).returncode == 0
    assert not clean.exists(), 'diagnostics/source errors/program nonzero exits are not crashes'
    # Explicit external boundary cannot attribute a nested program to voidc.
    nested = root / 'nested reports'
    nested_env = dict(disabled, VOID_CRASH_DIRECTORY=str(nested))
    arguments = ['two words', 'quote"slash\\', '${SECRET}', '--app']
    literal = run([COMPILER, 'crashes', 'run', PROBE, 'args', *arguments], nested_env)
    assert literal.returncode == 0 and literal.stdout.decode() == ''.join(arg+'\n' for arg in arguments)
    result = run([COMPILER, 'crashes', 'run', PROBE, 'abort'], nested_env)
    assert result.returncode == direct.returncode
    data = read_latest(nested)
    assert data['process'] == 'aot-program' and data['boundary'] == 'aot-program'
    assert data['binary_sha256'] == hashlib.file_digest(open(PROBE, 'rb'), 'sha256').hexdigest()
    assert len(reports(nested)) == 1
    if os.name != 'nt':
        # Real voidls worker, native fatal signal, original LSP framing untouched.
        lsp_dir = root / 'lsp reports'
        lsp = subprocess.Popen([SERVER, '--entry', str(source)], env=dict(env, VOID_CRASH_DIRECTORY=str(lsp_dir)),
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, preexec_fn=limits())
        lsp.stdin.write(frame('initialize', {'capabilities': {}}, 1)); lsp.stdin.flush()
        header = lsp.stdout.readline(); assert header.startswith(b'Content-Length: '), header
        assert lsp.stdout.readline() == b'\r\n'
        body = lsp.stdout.read(int(header.split(b':')[1])); assert 'result' in json.loads(body)
        os.kill(worker(lsp.pid), signal.SIGABRT)
        out, err = lsp.communicate(timeout=10)
        record([SERVER, '--entry', '(controlled worker signal)'], subprocess.CompletedProcess([], lsp.returncode, out, err))
        assert lsp.returncode == -signal.SIGABRT
        assert read_latest(lsp_dir)['process'] == 'voidls'
        assert b'VOID crash report:' in err
        import select
        source.write_text('use console\nfunc main() -> i32:\n    print("ready", flush = true)\n    var answer = console.stdin.read_line()\n    return 7\n')
        for flags in ([], ['--no-opt']):
            jit_dir = root / ('jit-o0 reports' if flags else 'jit-o2 reports')
            jit = subprocess.Popen([COMPILER, 'run', str(source), *flags], env=dict(env, VOID_CRASH_DIRECTORY=str(jit_dir)),
                stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, preexec_fn=limits(True))
            assert select.select([jit.stdout], [], [], 25)[0], 'JIT readiness timeout'
            ready = jit.stdout.readline()
            assert ready == b'ready\n', (ready, jit.stderr.read() if jit.poll() is not None else b'')
            os.kill(worker(jit.pid), signal.SIGABRT)
            out, err = jit.communicate(timeout=10)
            record([COMPILER, 'run', *flags, '(controlled JIT worker signal)'], subprocess.CompletedProcess([], jit.returncode, ready+out, err))
            assert jit.returncode == -signal.SIGABRT
            assert read_latest(jit_dir)['boundary'] == 'jit-program-or-runtime'
            aot = root / ('blocking-o0' if flags else 'blocking-o2')
            aot_dir = root / ('aot-o0 reports' if flags else 'aot-o2 reports')
            assert run([COMPILER, 'build', str(source), '-o', str(aot), *flags], env, stack=True).returncode == 0
            native = subprocess.Popen([COMPILER, 'crashes', 'run', str(aot)], env=dict(env, VOID_CRASH_DIRECTORY=str(aot_dir)),
                stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, preexec_fn=limits(True))
            assert select.select([native.stdout], [], [], 25)[0], 'AOT readiness timeout'
            ready = native.stdout.readline(); assert ready == b'ready\n', ready
            os.kill(worker(native.pid), signal.SIGABRT)
            out, err = native.communicate(timeout=10)
            record([COMPILER, 'crashes', 'run', str(aot), '(controlled AOT worker signal)'], subprocess.CompletedProcess([], native.returncode, ready+out, err))
            assert native.returncode == -signal.SIGABRT
            aot_data = read_latest(aot_dir)
            assert aot_data['boundary'] == 'aot-program' and aot_data['version'] is None
            assert aot_data['binary_sha256'] == hashlib.file_digest(open(aot, 'rb'), 'sha256').hexdigest()
            # Even with the environment enabled, a direct generated executable
            # has no collector. Only the explicit native-launch boundary opts in.
            direct_dir = root / 'direct app reports'
            app = subprocess.Popen([str(aot)], env=dict(env, VOID_CRASH_DIRECTORY=str(direct_dir)),
                stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, preexec_fn=limits(True))
            assert select.select([app.stdout], [], [], 25)[0]
            assert app.stdout.readline() == b'ready\n'
            app.send_signal(signal.SIGABRT); app.communicate(timeout=10)
            assert app.returncode == -signal.SIGABRT and not direct_dir.exists()

        # Compiler attribution uses a real compiler worker with a controlled
        # fatal signal. It is not evidence of an underlying compiler defect.
        # A fast check can finish before ps observes the worker. Wait for the
        # frontend status line and leave the oversized diagnostic on the pipe
        # so the worker cannot exit before receiving the signal.
        failing = root / 'compiler-diagnostic.void'
        failing.write_text('func main() -> i32:\n    return undefined_' + 'x' * (512 * 1024) + '\n')
        comp_dir = root / 'compiler reports'
        command = [COMPILER, 'check', str(failing), '--status', 'plain']
        with subprocess.Popen(command, env=dict(env, VOID_CRASH_DIRECTORY=str(comp_dir)),
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE, preexec_fn=limits()) as comp:
            try:
                assert select.select([comp.stderr], [], [], 5)[0], 'compiler frontend readiness timeout'
                checking = comp.stderr.readline()
                assert checking.startswith(b'Checking ') and b'project sources' in checking, checking
                os.kill(worker(comp.pid), signal.SIGABRT)
                out, err = comp.communicate(timeout=15)
            finally:
                if comp.poll() is None:
                    comp.terminate()
            record(command, subprocess.CompletedProcess([], comp.returncode, out, checking + err))
            assert comp.returncode == -signal.SIGABRT
        build = run([COMPILER, '--version'], disabled).stdout.decode().split('Build identity: ')[1].strip()
        data = read_latest(comp_dir)
        assert data['process'] == 'voidc' and data['boundary'] == 'voidc'
        assert data['compiler_build_identity'] == build
        assert data['binary_sha256'] == hashlib.file_digest(open(COMPILER, 'rb'), 'sha256').hexdigest()
    if os.environ.get('VOID_G90_STRICT'):
        sanitizer = run([PROBE, 'sanitizer'], env)
        assert sanitizer.returncode != 0 and b'AddressSanitizer' in sanitizer.stderr
        assert read_latest(directory)['classification'] == 'sanitizer_finding'
        undefined = run([PROBE, 'ubsan'], env)
        assert undefined.returncode != 0 and b'runtime error:' in undefined.stderr
        assert read_latest(directory)['classification'] == 'sanitizer_finding'
    if EVIDENCE:
        for category in (directory, nested, child_dir, root / 'lsp reports', root / 'compiler reports', root / 'jit-o0 reports', root / 'jit-o2 reports', root / 'aot-o0 reports', root / 'aot-o2 reports'):
            if category.exists():
                target = EVIDENCE / category.name
                target.mkdir(exist_ok=True)
                for path in reports(category): (target / path.name).write_bytes(path.read_bytes())
    print('G90: disabled/enabled exits, native crashes, identity, privacy, retention, failure safety, 1 MiB, JIT/AOT O0/O2 and LSP passed')


if __name__ == '__main__':
    with tempfile.TemporaryDirectory(prefix='VOID G90 雪 ') as temporary:
        main(Path(temporary))
