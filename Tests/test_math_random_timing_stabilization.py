#!/usr/bin/env python3
"""#379: permanent Math/Random/Stopwatch cross-feature stabilization checks."""
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
APP = ROOT / 'Tests/MathRandomTimingStabilization'
NEGATIVE = ROOT / 'Tests/MathRandomTimingStabilizationDiagnostics'
RUNTIME = sorted((ROOT / 'Runtime/src').glob('*.c'))
CC = shlex.split(os.environ.get('CC', 'gcc'))
COUNT = 0
EXPECTED_OUTPUT = 'True\n' * 45


def check(condition: bool, message: str) -> None:
    global COUNT
    if not condition:
        raise AssertionError(message)
    COUNT += 1
    print('True', flush=True)


def invoke(*args: object, timeout: int = 240) -> subprocess.CompletedProcess[str]:
    return subprocess.run([str(a) for a in args], cwd=ROOT,
                          capture_output=True, text=True, timeout=timeout)


def native_compile(compiler: list[str], source: Path, exe: Path, *, sanitize: bool = False) -> None:
    flags = ['-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2',
             '-I' + str(ROOT / 'Runtime/include')]
    if os.name != 'nt':
        flags.append('-pthread')
    if sanitize:
        flags += ['-fsanitize=undefined', '-fno-sanitize-recover=undefined']
    link = ['-lbcrypt', '-lm'] if os.name == 'nt' else ['-lm']
    result = invoke(*compiler, *flags, source, *RUNTIME, *link, '-o', exe)
    check(result.returncode == 0 and not result.stderr,
          f'native compile {compiler}: {result.stderr}\n{result.stdout}')


def main() -> None:
    version = invoke(BIN, 'version')
    check(version.returncode == 0 and version.stdout.strip() == 'voidc 0.0.381',
          'compiler version')
    # Ensure no new generator/clock source or native math subsystem was installed.
    random_source = (ROOT / 'StandardLibrary/Void/Random.void').read_text()
    watch_source = (ROOT / 'StandardLibrary/Void/Diagnostics/Stopwatch.void').read_text()
    check('6364136223846793005ul' in random_source and
          '1442695040888963407ul' in random_source and
          'vc_native_entropy_seed' in random_source,
          'original PCG stream and OS entropy imports')
    check('vc_native_monotonic_time_ns' in watch_source and
          '1000000000l' in watch_source,
          'original monotonic clock and timestamp contract')

    build = invoke(BIN, 'build', APP)
    check(build.returncode == 0 and not build.stderr,
          f'integration program build: {build.stdout}\n{build.stderr}')
    source = APP / '.void/MathRandomTimingStabilization.c'
    baseline = source.read_bytes()
    executable = APP / 'bin' / ('MathRandomTimingStabilization' + ('.exe' if os.name == 'nt' else ''))
    result = invoke(executable)
    check(result.returncode == 0 and result.stdout == EXPECTED_OUTPUT and not result.stderr,
          f'45 application assertions: {result.stdout}\n{result.stderr}')
    rebuild = invoke(BIN, 'build', APP)
    check(rebuild.returncode == 0 and baseline == source.read_bytes(),
          'deterministic generated C')

    for name, code, message in [
        ('MathObjectArgument', 'VOID3003', "type 'Math' has no matching static method 'Abs'"),
        ('WrongRandomArity', 'VOID3003', "type 'Random' has no matching instance method 'Next'"),
        ('ReadonlyElapsedTicks', 'VOID3000', "property 'ElapsedTicks' is read-only"),
    ]:
        fixture = NEGATIVE / name / 'Program.void'
        result = invoke(BIN, 'check', fixture, '--diagnostics=json')
        diagnostics = [json.loads(row) for row in result.stderr.splitlines()
                       if row.startswith('{') and 'schemaVersion' in row]
        valid = (result.returncode != 0 and len(diagnostics) == 1 and
                 diagnostics[0]['code'] == code and diagnostics[0]['message'] == message)
        if valid:
            span = diagnostics[0]['span']
            valid = 0 <= span['start']['offset'] < span['end']['offset'] <= len(fixture.read_text())
        check(valid, f'negative diagnostic {name}: {result.stderr}')

    with tempfile.TemporaryDirectory(prefix='void379-') as path:
        dest = Path(path)
        gcc_binary = dest / 'strict-gcc'
        native_compile(CC, source, gcc_binary)
        result = invoke(gcc_binary)
        check(result.returncode == 0 and not result.stderr and result.stdout == EXPECTED_OUTPUT,
              f'strict GCC runtime: {result.stdout}\n{result.stderr}')
        clang = shutil.which('clang')
        if clang:
            clang_binary = dest / 'strict-clang'
            native_compile([clang], source, clang_binary)
            result = invoke(clang_binary)
            check(result.returncode == 0 and not result.stderr and result.stdout == EXPECTED_OUTPUT,
                  f'strict Clang runtime: {result.stdout}\n{result.stderr}')
        else:
            print('Clang unavailable (not counted)', flush=True)
        probe = invoke(*CC, '-fsanitize=undefined', '-x', 'c', '-c', '-o', dest / 'probe.o',
                       '/dev/null') if os.name != 'nt' else None
        if probe is not None and probe.returncode == 0:
            ubsan_binary = dest / 'ubsan'
            native_compile(CC, source, ubsan_binary, sanitize=True)
            result = invoke(ubsan_binary)
            check(result.returncode == 0 and not result.stderr and result.stdout == EXPECTED_OUTPUT,
                  f'UBSan runtime: {result.stdout}\n{result.stderr}')
        else:
            print('UBSan unavailable (not counted)', flush=True)
    print(f'# {COUNT} checks', flush=True)


if __name__ == '__main__':
    main()
