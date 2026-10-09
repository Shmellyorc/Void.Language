#!/usr/bin/env python3
"""#378 Stopwatch: state, deterministic injected monotonic clock, C11, sanitizers."""
from __future__ import annotations
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BIN = ROOT / 'bin' / ('voidc.exe' if os.name == 'nt' else 'voidc')
APP = ROOT / 'Tests/HighResolutionStopwatchFoundation'
MOCK = ROOT / 'Tests/HighResolutionStopwatchFoundationMock'
RUNTIME = sorted((ROOT / 'Runtime/src').glob('*.c'))
CC = shlex.split(os.environ.get('CC', 'gcc'))
N = 0


def check(ok: bool, explanation: str) -> None:
    global N
    if not ok:
        raise AssertionError(explanation)
    N += 1
    print('True', flush=True)


def run(*args: object, timeout: int = 240) -> subprocess.CompletedProcess[str]:
    return subprocess.run([str(a) for a in args], cwd=ROOT,
                          capture_output=True, text=True, timeout=timeout)


def compile_c(cc: list[str], source: Path, target: Path, *, opt: str = '-O2',
              extra: tuple[str, ...] = (), clock_mock: bool = False) -> None:
    flags = ['-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', opt,
             '-I' + str(ROOT / 'Runtime/include')]
    if os.name != 'nt':
        flags += ['-pthread']
    link = ['-lbcrypt', '-lm'] if os.name == 'nt' else ['-lm']
    sources = [source, *RUNTIME]
    if clock_mock:
        sources += [MOCK / 'native_clock.c']
    result = run(*cc, *flags, *extra, *sources, *link, '-o', target)
    check(result.returncode == 0 and not result.stderr,
          f'strict compilation: {result.stderr}\n{result.stdout}')


def main() -> None:
    version = run(BIN, 'version')
    check(version.returncode == 0 and version.stdout.strip() == 'voidc 0.0.380', 'version')
    src = (ROOT / 'StandardLibrary/Void/Diagnostics/Stopwatch.void').read_text()
    check(src.count('[Native(') == 1 and 'vc_native_monotonic_time_ns' in src
          and 'public static long Frequency' in src and 'public static long GetTimestamp()' in src,
          'existing monotonic clock and fixed frequency')
    check('throw new OverflowException' in src and 'throw new InvalidOperationException' in src
          and 'long.MaxValue - accumulated' in src and '1000000l' in src,
          'failure, overflow, integer millisecond conversion')
    check('vc_native_monotonic_time_ns' in (ROOT / 'Runtime/src/vc_thread.c').read_text(),
          'existing native clock preserved')
    result = run(BIN, 'build', APP)
    check(result.returncode == 0 and not result.stderr, f'application build: {result.stderr}')
    generated = APP / '.void/HighResolutionStopwatchFoundation.c'
    baseline = generated.read_bytes()
    exe = APP / 'bin' / ('HighResolutionStopwatchFoundation' + ('.exe' if os.name == 'nt' else ''))
    result = run(exe)
    check(result.returncode == 0 and not result.stderr and result.stdout == 'True\n' * 29,
          f'live clock lifecycle: {result.stdout}\n{result.stderr}')
    result = run(BIN, 'build', APP)
    check(result.returncode == 0 and generated.read_bytes() == baseline,
          'generated C deterministic')
    for name, code, message in [
        ('WrongConstructor', 'VOID3000', "no matching constructor for 'Stopwatch' was found"),
        ('InvalidStartArgument', 'VOID3003', "type 'Stopwatch' has no matching instance method 'Start'"),
        ('PrivateNativeTimestamp', 'VOID3000', "unsafe method 'NativeTimestamp' requires an unsafe method"),
        ('ReadonlyFrequency', 'VOID3000', "property 'Frequency' is read-only"),
        ('WrongTimestampArgument', 'VOID3003', "type 'Stopwatch' has no matching static method 'GetTimestamp'"),
    ]:
        fixture = ROOT / 'Tests/HighResolutionStopwatchFoundationDiagnostics' / name / 'Program.void'
        result = run(BIN, 'check', fixture, '--diagnostics=json')
        diagnostics = [json.loads(s) for s in result.stderr.splitlines()
                       if s.startswith('{') and 'schemaVersion' in s]
        valid = (result.returncode != 0 and len(diagnostics) == 1
                 and diagnostics[0]['code'] == code and diagnostics[0]['message'] == message)
        if valid:
            span = diagnostics[0]['span']
            valid = 0 <= span['start']['offset'] < span['end']['offset'] <= len(fixture.read_text())
        check(valid, f'negative fixture {name}: {result.stderr}')

    result = run(BIN, 'build', MOCK / 'Program.void')
    check(result.returncode == 0 and not result.stderr, f'mock fixture build: {result.stderr}')
    generated_mock = (MOCK / '.void/Program.c').read_text()
    check('vc_native_monotonic_time_ns' in generated_mock, 'existing clock symbol emitted')
    with tempfile.TemporaryDirectory(prefix='void378-') as dirname:
        dest = Path(dirname)
        # Substitute ONLY the symbol reference in disposable generated-C test output.
        # The runtime and shipped compiler/library source remain unchanged.
        mock_source = dest / 'mock_program.c'
        mock_source.write_text('''#ifndef _GNU_SOURCE\n#define _GNU_SOURCE 1\n#endif\n#include <stdint.h>\n#include <stdbool.h>\nextern bool vc_stopwatch_test_clock_ns(uint64_t *);\n'''
                               + generated_mock.replace('vc_native_monotonic_time_ns(',
                                                        'vc_stopwatch_test_clock_ns('))
        mock_count = 15
        mock_bin = dest / 'mock-gcc'
        compile_c(CC, mock_source, mock_bin, clock_mock=True, opt='-O0')
        result = run(mock_bin)
        check(result.returncode == 0 and not result.stderr and result.stdout == 'True\n' * mock_count,
              f'deterministic clock edge checks: {result.stdout}\n{result.stderr}')
        cc_exe = dest / 'real-gcc'
        compile_c(CC, generated, cc_exe)
        result = run(cc_exe)
        check(result.returncode == 0 and not result.stderr and result.stdout == 'True\n' * 29,
              f'GCC strict runtime: {result.stdout}\n{result.stderr}')
        clang = shutil.which('clang')
        if clang:
            cc_exe = dest / 'real-clang'
            compile_c([clang], generated, cc_exe)
            result = run(cc_exe)
            check(result.returncode == 0 and not result.stderr and result.stdout == 'True\n' * 29,
                  f'Clang strict runtime: {result.stdout}\n{result.stderr}')
        else:
            print('Clang unavailable (not counted)', flush=True)
        probe = run(*CC, '-fsanitize=undefined', '-c', str(MOCK / 'native_clock.c'),
                    '-o', dest / 'probe.o')
        if probe.returncode == 0:
            sanitized = dest / 'mock-ubsan'
            compile_c(CC, mock_source, sanitized, opt='-O0', clock_mock=True,
                      extra=('-fsanitize=undefined', '-fno-sanitize-recover=undefined'))
            result = run(sanitized)
            check(result.returncode == 0 and not result.stderr and result.stdout == 'True\n' * mock_count,
                  f'UBSan clock bounds: {result.stdout}\n{result.stderr}')
        else:
            print('UBSan unavailable (not counted)', flush=True)
    print(f'# {N} checks')


if __name__ == '__main__':
    main()
