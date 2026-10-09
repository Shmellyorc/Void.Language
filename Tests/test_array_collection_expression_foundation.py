#!/usr/bin/env python3
"""#362: target-independent bracket syntax adapts typed array declarations to shared construction."""
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
NAME = 'ArrayCollectionExpressionFoundation'
FIXTURE = ROOT / 'Tests' / NAME
GENERATED = FIXTURE / '.void' / (NAME + '.c')
EXECUTABLE = FIXTURE / 'bin' / (NAME + ('.exe' if os.name == 'nt' else ''))
RUNTIME = sorted((ROOT / 'Runtime/src').glob('*.c'))
EXPECTED = 'True\n' * 15
COUNT = 0

NEGATIVES = {
    'DoubleComma': ('VOID2001', "expected expression, found ','"),
    'FieldMismatch': ('VOID3000', "cannot assign 'int' to array initializer element of type 'string'"),
    'GenericMismatch': ('VOID3000', "cannot assign 'Box<string>' to array initializer element of type 'Box<int>'"),
    'MissingBracket': ('VOID2001', "expected ']', found ';'"),
    'InterfaceTarget': ('VOID3000', 'collection expression requires an instantiable concrete collection target'),
    'MemoryTarget': ('VOID3000', 'requires an accessible instance Add method'),
    'RectangularTarget': ('VOID2000', 'collection expressions require a one-dimensional array target'),
    'PrimitiveMismatch': ('VOID3000', "cannot assign 'bool' to array initializer element of type 'int'"),
    'ScalarTarget': ('VOID3000', 'collection expression requires a one-dimensional array target type'),
    'Spread': ('VOID3000', 'spread source must be a one-dimensional array'),
    'VarUntyped': ('VOID3000', 'collection expression requires a one-dimensional array target type'),
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
    source = ROOT / 'Tests' / 'ArrayCollectionExpressionFoundationDiagnostics' / name / 'Program.void'
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
                 actual.get('severity') == 'error' and 0 <= start <= end <= len(contents) and
                 actual.get('file', '').endswith('/' + name + '/Program.void'))
    check(valid, f'negative {name}: {result.returncode}\n{result.stderr}')


def main() -> None:
    version = invoke(VOIDC, 'version')
    check(version.returncode == 0 and version.stdout.strip() == 'voidc 0.0.381',
          f'version: {version.stdout} {version.stderr}')
    built = invoke(VOIDC, 'build', FIXTURE)
    check(built.returncode == 0 and not built.stderr, f'build: {built.stdout}\n{built.stderr}')
    run_exact(EXECUTABLE)
    parsed = invoke(VOIDC, 'parse', ROOT / 'Tests' /
                    'ArrayCollectionExpressionFoundationDiagnostics' / 'VarUntyped' / 'Program.void')
    check(parsed.returncode == 0 and 'CollectionExpression' in parsed.stdout and
          'Number 1' in parsed.stdout and 'Number 2' in parsed.stdout,
          'the general expression AST must preserve unbound bracket elements')
    first = GENERATED.read_bytes()
    rebuilt = invoke(VOIDC, 'build', FIXTURE)
    check(rebuilt.returncode == 0 and not rebuilt.stderr and GENERATED.read_bytes() == first,
          f'deterministic generated C: {rebuilt.stdout}\n{rebuilt.stderr}')
    code = first.decode('utf-8')
    check('vc_array_new(' in code,
          'bracket arrays must use the existing allocation path')

    with tempfile.TemporaryDirectory(prefix='void362-collection-arrays-') as scratch:
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
    # These #362 initially-invalid contexts become positive in #363.
    for name in ('Assignment', 'CallArgument', 'ReturnExpression', 'EmptyArray'):
        source = ROOT / 'Tests' / 'ArrayCollectionExpressionFoundationDiagnostics' / name / 'Program.void'
        result = invoke(VOIDC, 'check', source)
        check(result.returncode == 0, f'newly-supported {name}: {result.stderr}')
    if COUNT != 25:
        raise AssertionError(f'expected 25 focused checks, got {COUNT}')
    print(f'# {COUNT} checks')


if __name__ == '__main__':
    main()
