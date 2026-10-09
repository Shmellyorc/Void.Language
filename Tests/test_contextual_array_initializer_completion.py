#!/usr/bin/env python3
"""#361: declaration-context array braces share existing typed array construction."""
from __future__ import annotations

import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
VOIDC = ROOT / 'bin' / ('voidc.exe' if os.name == 'nt' else 'voidc')
NAME = 'ContextualArrayInitializerCompletion'
FIXTURE = ROOT / 'Tests' / NAME
GENERATED = FIXTURE / '.void' / (NAME + '.c')
EXECUTABLE = FIXTURE / 'bin' / (NAME + ('.exe' if os.name == 'nt' else ''))
RUNTIME = sorted((ROOT / 'Runtime/src').glob('*.c'))
EXPECTED = 'True\n' * 23
COUNT = 0

NEGATIVES = {
    'AssignmentInitializer': ('VOID2001', "expected expression, found '{'"),
    'CallInitializer': ('VOID2001', "expected expression, found '{'"),
    'FieldElementMismatch': ('VOID3000', "cannot assign 'int' to array initializer element of type 'string'"),
    'GenericMismatch': ('VOID3000', "cannot assign 'Box<string>' to array initializer element of type 'Box<int>'"),
    'JaggedElementMismatch': ('VOID3000', "cannot assign 'bool' to array initializer element of type 'int'"),
    'LocalElementMismatch': ('VOID3000', "cannot assign 'bool' to array initializer element of type 'int'"),
    'MalformedComma': ('VOID2001', "expected expression, found ','"),
    'MalformedDelimiter': ('VOID2001', "expected ',' or '}', found ';'"),
    'RectangularElementMismatch': ('VOID3000', "cannot assign 'bool' to rectangular array initializer element of type 'int'"),
    'RectangularExtraGroup': ('VOID3000', 'rectangular array initializer has too many nested dimensions'),
    'RectangularMissingGroup': ('VOID3000', 'rectangular array initializer requires nested braces for dimension 1'),
    'RectangularRagged': ('VOID3000', 'rectangular array initializer has inconsistent length in dimension 1'),
    'ScalarInitializer': ('VOID2001', "expected expression, found '{'"),
    'VarInitializer': ('VOID2001', "expected expression, found '{'"),
}


def check(ok: bool, description: str, *, count: bool = True) -> None:
    global COUNT
    if not ok:
        raise AssertionError(description)
    if count:
        COUNT += 1
        print('True', flush=True)


def invoke(*args: object, timeout: int = 150) -> subprocess.CompletedProcess[str]:
    return subprocess.run([str(arg) for arg in args], cwd=ROOT, capture_output=True,
                          text=True, timeout=timeout)


def run_exact(binary: Path, *, count: bool = True) -> None:
    result = invoke(binary, timeout=60)
    check(result.returncode == 0 and result.stdout == EXPECTED and not result.stderr,
          f'run {binary}: {result.returncode}\n{result.stdout}\n{result.stderr}', count=count)


def strict_compile(compiler: list[str], optimization: str, output: Path,
                   *, sanitizer: bool = False, count: bool = True) -> None:
    flags = ['-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', optimization,
             '-I' + str(ROOT / 'Runtime/include')]
    if os.name != 'nt':
        flags.append('-pthread')
    if sanitizer:
        flags += ['-fsanitize=undefined', '-fno-sanitize-recover=undefined']
    result = invoke(*compiler, *flags, GENERATED, *RUNTIME, '-lm', '-o', output, timeout=240)
    check(result.returncode == 0 and not result.stderr,
          f'strict C compile {compiler}: {result.stdout}\n{result.stderr}', count=count)


def check_diagnostic(name: str, code: str, message: str) -> None:
    source = ROOT / 'Tests' / 'ContextualArrayInitializerDiagnostics' / name / 'Program.void'
    contents = source.read_text(encoding='utf-8')
    result = invoke(VOIDC, 'check', source, '--diagnostics=json')
    diagnostics = [json.loads(line) for line in result.stderr.splitlines()
                   if line.startswith('{') and 'schemaVersion' in line]
    valid = result.returncode != 0 and len(diagnostics) == 1
    if valid:
        actual = diagnostics[0]
        span = actual.get('span') or {}
        start = (span.get('start') or {}).get('offset', -1)
        end = (span.get('end') or {}).get('offset', -1)
        valid = (actual.get('code') == code and message in actual.get('message', '') and
                 actual.get('severity') == 'error' and 0 <= start < end <= len(contents) and
                 actual.get('file', '').endswith('/' + name + '/Program.void'))
    check(valid, f'negative {name}: {result.returncode}\n{result.stderr}')


def main() -> None:
    version = invoke(VOIDC, 'version')
    check(version.returncode == 0 and version.stdout.strip() == 'voidc 0.0.381',
          f'version: {version.stdout} {version.stderr}')
    built = invoke(VOIDC, 'build', FIXTURE)
    check(built.returncode == 0 and not built.stderr, f'build: {built.stdout}\n{built.stderr}')
    run_exact(EXECUTABLE)
    first = GENERATED.read_bytes()
    rebuilt = invoke(VOIDC, 'build', FIXTURE)
    check(rebuilt.returncode == 0 and not rebuilt.stderr and GENERATED.read_bytes() == first,
          f'deterministic generated C: {rebuilt.stdout}\n{rebuilt.stderr}')
    code = first.decode('utf-8')
    check('vc_array_new(' in code and 'vc_array_new_md(' in code,
          'contextual arrays must use existing allocation paths')

    with tempfile.TemporaryDirectory(prefix='void361-contextual-arrays-') as scratch:
        folder = Path(scratch)
        cc = shlex.split(os.environ.get('CC', 'cc'))
        for optimization in ('-O0', '-O2'):
            binary = folder / ('gcc' + optimization + ('.exe' if os.name == 'nt' else ''))
            strict_compile(cc, optimization, binary)
            run_exact(binary)
        if shutil.which('clang') and os.name != 'nt':
            binary = folder / 'clang-strict'
            strict_compile(['clang'], '-O2', binary, count=False)
            run_exact(binary, count=False)
            print('Clang strict C: passed')
        else:
            print('Clang strict C: unavailable')
        if os.name != 'nt':
            binary = folder / 'ubsan'
            strict_compile(cc, '-O1', binary, sanitizer=True, count=False)
            run_exact(binary, count=False)
            print('UBSan: passed')
        else:
            print('UBSan: unavailable')

    for name, (code, message) in NEGATIVES.items():
        check_diagnostic(name, code, message)
    if COUNT != 23:
        raise AssertionError(f'expected 23 focused checks, got {COUNT}')
    print(f'# {COUNT} checks')


if __name__ == '__main__':
    main()
