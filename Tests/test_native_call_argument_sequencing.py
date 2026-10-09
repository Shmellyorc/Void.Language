#!/usr/bin/env python3
"""Shared native call sequencing: runtime assertions, strict C11 and sanitizers."""
from __future__ import annotations
import json
import os
from pathlib import Path
import shutil
import tempfile
import test_high_resolution_stopwatch_foundation as harness

ROOT = harness.ROOT
FIXTURE = ROOT / 'Tests/NativeCallArgumentSequencing'


def main():
    version = harness.run(harness.BIN, 'version')
    harness.check(version.returncode == 0 and version.stdout.strip() == 'voidc 0.0.381', 'version')
    with tempfile.TemporaryDirectory(prefix='void381-') as temporary:
        dest = Path(temporary)
        shutil.copy(FIXTURE / 'Program.void', dest)
        shutil.copy(FIXTURE / 'native.c', dest)
        (dest / 'native').mkdir()
        obj = dest / 'native/native.o'
        result = harness.run(*harness.CC, '-std=c11', '-Wall', '-Wextra', '-Wpedantic',
                             '-Werror', '-c', dest / 'native.c', '-o', obj)
        harness.check(result.returncode == 0 and not result.stderr, result.stderr)
        result = harness.run('ar', 'rcs', dest / 'native/libsequence.a', obj)
        harness.check(result.returncode == 0, result.stderr)
        project = {'format': 1, 'name': 'Sequence', 'output': 'exe', 'version': '0.0.381',
                   'compiler': {'unsafe': True, 'libraryPaths': ['native'], 'libraries': ['sequence']}}
        (dest / 'Sequence.voidproj').write_text(json.dumps(project))
        result = harness.run(harness.BIN, 'build', dest)
        harness.check(result.returncode == 0 and not result.stderr, result.stderr)
        source = dest / '.void/Sequence.c'
        baseline = source.read_bytes()
        result = harness.run(harness.BIN, 'build', dest)
        harness.check(result.returncode == 0 and not result.stderr and source.read_bytes() == baseline,
                      'deterministic generated C: ' + result.stderr)
        def execute(binary, *, asan=False):
            # LeakSanitizer is unavailable under the desktop tracing host.
            import subprocess
            env = dict(os.environ)
            if asan:
                env['ASAN_OPTIONS'] = 'detect_leaks=0'
            result = subprocess.run([str(binary)], cwd=ROOT, capture_output=True, text=True,
                                    timeout=240, env=env)
            lines = result.stdout.splitlines()
            harness.check(result.returncode == 0 and not result.stderr and
                          len(lines) == ASSERTIONS and all(line == 'True' for line in lines),
                          f'{binary.name}: {result.returncode}\n{result.stdout}\n{result.stderr}')
        execute(dest / 'bin' / ('Sequence.exe' if os.name == 'nt' else 'Sequence'))
        compilers = [('gcc', [shutil.which('gcc')]), ('clang', [shutil.which('clang')])]
        for label, compiler in compilers:
            harness.check(compiler[0] is not None, label + ' available for C11 verification')
            for opt in ('-O0', '-O2'):
                binary = dest / (label + opt)
                harness.compile_c([*compiler, str(dest / 'native.c')], source, binary, opt=opt)
                execute(binary)
        for sanitizer in ('undefined', 'address'):
            binary = dest / sanitizer
            harness.compile_c([*harness.CC, str(dest / 'native.c')], source, binary, opt='-O1',
                              extra=('-g', '-fsanitize=' + sanitizer, '-fno-sanitize-recover=all'))
            execute(binary, asan=sanitizer == 'address')
        # Bounds faults are native runtime failures, not managed exceptions.
        # Execute this probe separately: its gate is deliberately never completed.
        for fixture, message in (
            ('NativeCallArgumentSequencingBoundsFault', 'array index out of range'),
            ('NativeCallArgumentSequencingReceiverFault', 'null'),
        ):
            fault = dest / fixture
            fault.mkdir()
            shutil.copy(ROOT / 'Tests' / fixture / 'Program.void', fault)
            result = harness.run(harness.BIN, 'build', fault / 'Program.void')
            harness.check(result.returncode == 0 and not result.stderr, result.stderr)
            result = harness.run(fault / 'bin' / ('Program.exe' if os.name == 'nt' else 'Program'))
            harness.check(result.returncode != 0 and result.stdout == '' and
                          message in result.stderr and 'Program.Probe()' in result.stderr,
                          'receiver storage fault must precede suspension: ' + result.stderr)
        # Storage identity remains a binder responsibility: sequencing must not
        # make non-lvalues legal ref/out arguments or mutable copies of in values.
        negatives = [
            ('int n = 0; F(ref (n + 1), 2);', 'VOID3000', 'ref argument requires a variable or writable field'),
            ('int n = 0; F(ref n, true);', 'VOID3003', "no matching method 'F' was found"),
        ]
        for body, code, message in negatives:
            (dest / 'negative').mkdir(exist_ok=True)
            negative = dest / 'negative/Program.void'
            negative.write_text('public static class Invalid { public static void F(ref int x, int y) {} '
                                'public static void Main() {' + body + '} }')
            result = harness.run(harness.BIN, 'check', negative, '--diagnostics=json')
            diagnostics = [json.loads(line) for line in result.stderr.splitlines()
                           if line.startswith('{') and 'schemaVersion' in line]
            harness.check(result.returncode != 0 and len(diagnostics) == 1 and
                          diagnostics[0]['code'] == code and diagnostics[0]['message'] == message and
                          diagnostics[0]['span']['start']['offset'] < diagnostics[0]['span']['end']['offset'],
                          'invalid ref/overload diagnostic: ' + result.stderr)


ASSERTIONS = 132
if __name__ == '__main__':
    main()
