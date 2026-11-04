#!/usr/bin/env python3
"""Compile and execute the website's exact downloadable teaching programs."""
import argparse
import json
import os
from contextlib import contextmanager
from http.server import BaseHTTPRequestHandler, HTTPServer
from threading import Thread
from pathlib import Path
import subprocess
import tempfile
import textwrap

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('compiler', type=Path, help='Existing current voidc binary')
parser.add_argument('--async-build', type=Path, required=True,
                    help='Matching existing build containing void_async_tests and runtime archives')
parser.add_argument('--cxx', help='Optional matching C++ linker driver override')
args = parser.parse_args()
compiler = args.compiler.resolve(strict=True)
build = args.async_build.resolve(strict=True)
emitter = (build / 'void_async_tests').resolve(strict=True)
link = (build / 'void-native.txt').read_text().splitlines()
if link[0] not in ('VOID native 23.1.4 v1', 'VOID native 23.1.4 v2'):
    parser.error('Expected the LLVM 23.1.4 native link manifest')
cxx = args.cxx or link[1]
link_flags = ['--driver-mode=g++', *link[3:]]
if link[2]:
    link_flags.append('--ld-path=' + link[2])
root = Path(__file__).resolve().parents[1]
samples = json.loads((root / 'scripts/guide-examples.json').read_text())
# Keep a real recoverable-error branch in the verification set.
samples.append({'source': 'public/examples/guides/errors.void', 'input': 0, 'expected': -1})

@contextmanager
def example_environment(fixture):
    if not fixture:
        yield None
        return
    if fixture == 'three-byte-file':
        with tempfile.TemporaryDirectory(prefix='void-guide-file-') as directory:
            path = Path(directory) / 'field.bin'
            path.write_bytes(bytes((12, 18, 30)))
            yield dict(os.environ, VOID_GUIDE_FILE=str(path))
        return
    if fixture != 'loopback-http':
        raise ValueError(f'Unknown example fixture: {fixture}')
    class StatusHandler(BaseHTTPRequestHandler):
        def do_GET(self):
            self.send_response(200 if self.path == '/status' else 404)
            self.send_header('Content-Length', '0')
            self.end_headers()
        def log_message(self, *args):
            pass

    with HTTPServer(('127.0.0.1', 0), StatusHandler) as server:
        thread = Thread(target=server.serve_forever)
        thread.start()
        try:
            yield dict(os.environ, VOID_GUIDE_URL=f'http://127.0.0.1:{server.server_port}/status')
        finally:
            server.shutdown()
            thread.join()


def run(command, env=None):
    result = subprocess.run([str(arg) for arg in command], capture_output=True,
                            text=True, timeout=60, env=env)
    if result.returncode:
        raise SystemExit(f'FAIL: {command}\n{result.stdout}\n{result.stderr}')
    return result.stdout.strip()

executed = async_compilations = 0
with tempfile.TemporaryDirectory(prefix='void-website-examples-') as temporary:
    output = Path(temporary)
    # Compile outside public/: compiler caches must never become site assets.
    staged = output / 'sources'
    for original in (root / 'public').rglob('*.void'):
        if original.is_file():
            copy = staged / original.relative_to(root / 'public')
            copy.parent.mkdir(parents=True, exist_ok=True)
            copy.write_bytes(original.read_bytes())
    for index, sample in enumerate(samples):
        source = staged / Path(sample['source']).relative_to('public')
        if sample.get('kind') == 'fragment':
            # Compile the exact displayed fragment in its explicit teaching context.
            fragment = textwrap.indent(source.read_text(), ' ' * sample.get('context_indent', 0))
            source = output / f'fragment-{index}.void'
            source.write_text(sample.get('context_before', '') + fragment
                              + sample.get('context_after', ''))
        if sample.get('maintenance') == 'repository-copy':
            if source.read_bytes() != (root.parent / sample['repository_source']).read_bytes():
                raise SystemExit(f'FAIL: website copy differs from {sample["repository_source"]}')
        if sample.get('mode') == 'app':
            # A nonzero application status is a returned value, not CLI output.
            # Compare the raw stdout independently; do not strip or fabricate it.
            targets = (source, source.parent) if source.name == 'main.void' else (source,)
            for target, flags in ((target, flags) for target in targets
                                  for flags in ([], ['--no-opt'])):
                command = [str(compiler), 'run', str(target), *flags]
                result = subprocess.run(command, capture_output=True, text=True, timeout=60)
                if (result.returncode != sample['expected_status']
                        or result.stdout != sample['expected_stdout'] or result.stderr):
                    raise SystemExit(f'FAIL: {command}: expected status {sample["expected_status"]}, '
                                     f'stdout {sample["expected_stdout"]!r}; got status '
                                     f'{result.returncode}, stdout {result.stdout!r}, stderr {result.stderr!r}')
                executed += 1
            continue
        cases = [{'input': sample['input'], 'expected': sample['expected']}]
        cases += sample.get('cases', [])
        if sample.get('mode') == 'async':
            run([compiler, 'check', source])
            # Emit from this exact website source, never reuse fixture executables.
            # Both LLVM modes exercise the supported explicit async adapter.
            for optimization in ('normal', 'optimized'):
                obj = output / f'{index}-{optimization}.o'
                executable = output / f'{index}-{optimization}'
                run([emitter, '--emit', source, obj, optimization])
                run([cxx, obj, *link_flags, '-o', executable])
                async_compilations += 1
                for case in cases:
                    actual = run([executable, case['input']])
                    if actual != str(case['expected']):
                        raise SystemExit(f'FAIL: {source} {optimization} input {case["input"]}: '
                                         f'expected {case["expected"]}, got {actual}')
                    executed += 1
        else:
            modes = ([], ['--no-opt'])
            with example_environment(sample.get('fixture')) as env:
                for flags in modes:
                    for case in cases:
                        actual = run([compiler, 'run', source, case['input'], *flags], env=env)
                        if actual != str(case['expected']):
                            raise SystemExit(f'FAIL: {source} input {case["input"]}: '
                                             f'expected {case["expected"]}, got {actual}')
                        executed += 1
print(f'PASS: {executed} result checks; {async_compilations} fresh async AOT builds; all website programs executed')
