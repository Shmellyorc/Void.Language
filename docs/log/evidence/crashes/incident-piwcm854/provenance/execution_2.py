"""Consolidated real JIT/O2/linked AOT source execution with crash preservation."""
from pathlib import Path
import json
import os
import sys
import tempfile
import time
from crash import run

compiler = Path(sys.argv[1]).resolve()
selected = sys.argv[2] if len(sys.argv) > 2 else 'all'
small = '--small-stack' in sys.argv
checks = []


def stack():
    import resource
    resource.setrlimit(resource.RLIMIT_STACK, (1024 * 1024, 1024 * 1024))


def invoke(arguments, cwd, source):
    started = time.monotonic()
    result = run(arguments, cwd=cwd, capture_output=True, timeout=600, source_inputs=[source],
                 preexec_fn=stack if small else None)
    elapsed = time.monotonic() - started
    assert result.returncode == 0, (arguments, result.returncode, result.stdout, result.stderr)
    assert b'warning' not in result.stderr.lower(), result.stderr
    checks.append({'command': list(map(str, arguments)), 'seconds': elapsed})
    print('PASS', ' '.join(map(str, arguments)), f'{elapsed:.3f}s', flush=True)


fixtures = ['text', 'hex', 'base64', 'ascii', 'oracles', 'interop', 'large', 'errors'] if selected == 'all' else [selected]
with tempfile.TemporaryDirectory(prefix='VOID text encoding execution ') as directory:
    root = Path(directory)
    for name in fixtures:
        fixture = Path(__file__).with_name(name + '.void').resolve()
        invoke([compiler, 'check', fixture], root, fixture)
        for mode in ('jit', 'o2', 'aot', 'aot-o2'):
            cwd = root / (name + '-' + mode)
            cwd.mkdir()
            if mode in ('jit', 'o2'):
                command = [compiler, 'run', fixture]
                if mode == 'jit':
                    command += ['--no-opt']
            else:
                executable = cwd / ('application.exe' if os.name == 'nt' else 'application')
                build = [compiler, 'build', fixture, '-o', executable]
                if mode == 'aot':
                    build += ['--no-opt']
                invoke(build, cwd, fixture)
                command = [executable]
            invoke(command, cwd, fixture)
print(json.dumps({'checks': len(checks), 'small_stack': small, 'measurements': checks}, indent=2))
