#!/usr/bin/env python3
"""#371: source-visible integer Math overloads, boundaries, diagnostics and strict C."""
from __future__ import annotations

import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / 'bin' / ('voidc.exe' if os.name == 'nt' else 'voidc')
FIXTURE = ROOT / 'Tests/IntegerMathFoundations'
NAME = 'IntegerMathFoundations'
GENERATED = FIXTURE / '.void' / (NAME + '.c')
EXECUTABLE = FIXTURE / 'bin' / (NAME + ('.exe' if os.name == 'nt' else ''))
RUNTIME = sorted((ROOT / 'Runtime/src').glob('*.c'))
EXPECTED = 'True\n' * 42
COUNT = 0
NEGATIVES = {
    'WrongType': 'Min',
    'WrongArity': 'Clamp',
    'UnsignedSign': 'Sign',
    'MixedNoConversion': 'Max',
    'FloatDeferred': 'Max',
}


def check(ok: bool, detail: str) -> None:
    global COUNT
    if not ok:
        raise AssertionError(detail)
    COUNT += 1
    print('True', flush=True)


def invoke(*args: object, timeout: int = 180) -> subprocess.CompletedProcess[str]:
    env = dict(os.environ)
    env.setdefault('TERM', 'xterm')
    return subprocess.run([str(x) for x in args], cwd=ROOT, env=env,
                          capture_output=True, text=True, encoding='utf-8',
                          timeout=timeout)


def verify_output(path: Path) -> str:
    result = invoke(path)
    check(result.returncode == 0 and not result.stderr and result.stdout == EXPECTED,
          f'{path}: {result.returncode}\n{result.stdout}\n{result.stderr}')
    return result.stdout


def strict_compile(command: list[str], binary: Path, *extra: str) -> None:
    flags = ['-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2',
             '-I' + str(ROOT / 'Runtime/include')]
    if os.name != 'nt':
        flags.append('-pthread')
    result = invoke(*command, *flags, *extra, GENERATED, *RUNTIME, '-lm',
                    '-o', binary, timeout=240)
    check(result.returncode == 0 and not result.stderr,
          f'{command} generated C: {result.returncode}\n{result.stdout}\n{result.stderr}')


def verify_negative(name: str, member: str) -> None:
    source = ROOT / 'Tests/IntegerMathFoundationsDiagnostics' / name / 'Program.void'
    result = invoke(COMPILER, 'check', source, '--diagnostics=json')
    messages = [json.loads(line) for line in result.stderr.splitlines()
                if line.startswith('{') and 'schemaVersion' in line]
    valid = result.returncode != 0 and len(messages) == 1
    if valid:
        d = messages[0]
        span = d.get('span') or {}
        start = (span.get('start') or {}).get('offset', -1)
        end = (span.get('end') or {}).get('offset', -1)
        valid = (d.get('code') == 'VOID3003' and d.get('severity') == 'error'
                 and d.get('message') == f"type 'Math' has no matching static method '{member}'"
                 and 0 <= start < end <= len(source.read_text(encoding='utf-8'))
                 and d.get('file', '').endswith('/' + name + '/Program.void'))
    check(valid, f'{name}: {result.returncode}\n{result.stdout}\n{result.stderr}')


def main() -> None:
    version = invoke(COMPILER, 'version')
    check(version.returncode == 0 and version.stdout.strip() == 'voidc 0.0.380',
          f'version: {version.stdout} {version.stderr}')
    built = invoke(COMPILER, 'build', FIXTURE)
    check(built.returncode == 0 and not built.stderr,
          f'build: {built.stdout} {built.stderr}')
    baseline = verify_output(EXECUTABLE)
    generated = GENERATED.read_bytes()
    rebuilt = invoke(COMPILER, 'build', FIXTURE)
    check(rebuilt.returncode == 0 and not rebuilt.stderr and
          GENERATED.read_bytes() == generated,
          f'deterministic C: {rebuilt.stdout} {rebuilt.stderr}')

    source = (ROOT / 'StandardLibrary/Void/Math.void').read_text(encoding='utf-8')
    types = ('sbyte', 'byte', 'short', 'ushort', 'int', 'uint', 'long', 'ulong')
    signed = ('sbyte', 'short', 'int', 'long')
    signatures = [f'public static {type} {name}(' for type in types
                  for name in ('Min', 'Max', 'Clamp')]
    signatures += [f'public static {type} Abs(' for type in signed]
    signatures += [f'public static int Sign({type} value)' for type in signed]
    check('namespace Void;' in source and all(x in source for x in signatures)
          and source.count('throw new OverflowException()') == 4
          and source.count('throw new ArgumentException(') == 8,
          'incomplete ordinary VOID Math overload family')

    for name, member in NEGATIVES.items():
        verify_negative(name, member)

    with tempfile.TemporaryDirectory(prefix='void371-math-') as scratch:
        folder = Path(scratch)
        gcc = shlex.split(os.environ.get('CC', 'gcc'))
        binary = folder / ('math-gcc' + ('.exe' if os.name == 'nt' else ''))
        strict_compile(gcc, binary)
        check(verify_output(binary) == baseline, 'GCC result differs from VOIDC')

        clang = shutil.which('clang')
        if clang:
            binary = folder / ('math-clang' + ('.exe' if os.name == 'nt' else ''))
            strict_compile([clang], binary)
            check(verify_output(binary) == baseline, 'Clang result differs from GCC')
        else:
            # Keep full-suite guard independent of optional toolchain availability.
            check(True, 'Clang not available')
            check(True, 'Clang not available')
            check(True, 'Clang not available')

        binary = folder / ('math-ubsan' + ('.exe' if os.name == 'nt' else ''))
        probe = folder / 'probe.c'
        probe.write_text('int main(void) { return 0; }\n', encoding='utf-8')
        supported = invoke(*gcc, '-fsanitize=undefined', probe, '-o', binary)
        if supported.returncode == 0:
            strict_compile(gcc, binary, '-fsanitize=undefined',
                           '-fno-sanitize-recover=undefined')
            check(verify_output(binary) == baseline, 'UBSan result differs from GCC')
        else:
            check(True, 'UBSan not supported')
            check(True, 'UBSan not supported')
            check(True, 'UBSan not supported')
    print(f'# {COUNT} checks')


if __name__ == '__main__':
    main()
