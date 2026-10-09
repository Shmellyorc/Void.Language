#!/usr/bin/env python3
"""#363: target-typed array collections, overload checks, and shared construction."""
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
NAME = 'CollectionExpressionTargetTyping'
FIXTURE = ROOT / 'Tests' / NAME
GENERATED = FIXTURE / '.void' / (NAME + '.c')
EXECUTABLE = FIXTURE / 'bin' / (NAME + ('.exe' if os.name == 'nt' else ''))
RUNTIME = sorted((ROOT / 'Runtime/src').glob('*.c'))
EXPECTED_OUTPUT = 'True\n' * 19
CHECKS = 0

NEGATIVES = {
    'VarNonempty': ('VOID3000', 'requires a one-dimensional array target type'),
    'VarEmpty': ('VOID3000', 'requires a one-dimensional array target type'),
    'InvalidLocal': ('VOID3000', "cannot assign 'string' to array initializer element of type 'int'"),
    'InvalidAssignment': ('VOID3000', "cannot assign 'string' to array initializer element of type 'int'"),
    'InvalidReturn': ('VOID3000', "cannot assign 'string' to array initializer element of type 'int'"),
    'InvalidArgument': ('VOID3003', "no matching method 'Accept'"),
    'EmptyAmbiguous': ('VOID3003', "call to 'Pick' is ambiguous"),
    'InvalidOverloads': ('VOID3003', "no matching method 'Pick'"),
    'ScalarTarget': ('VOID3000', 'requires a one-dimensional array target type'),
    'ObjectTarget': ('VOID3000', 'requires a one-dimensional array target type'),
    'RectangularTarget': ('VOID2000', 'collection expressions require a one-dimensional array target'),
    'InterfaceTarget': ('VOID3000', 'requires an instantiable concrete collection target'),
    'MemoryTarget': ('VOID3000', 'requires an accessible instance Add method'),
    'MissingBracket': ('VOID2001', "expected ']', found ';'"),
    'DoubleComma': ('VOID2001', "expected expression, found ','"),
    'Spread': ('VOID3000', 'spread source must be a one-dimensional array'),
    'GenericMismatch': ('VOID3000', "cannot assign 'Cell<string>' to array initializer element of type 'Cell<int>'"),
    'ReturnObject': ('VOID3000', 'requires a one-dimensional array target type'),
}


def check(condition: bool, label: str, *, count: bool = True) -> None:
    global CHECKS
    if not condition:
        raise AssertionError(label)
    if count:
        print('True', flush=True)
        CHECKS += 1


def run(*arguments: object, timeout: int = 150) -> subprocess.CompletedProcess[str]:
    return subprocess.run([str(arg) for arg in arguments], cwd=ROOT,
                          capture_output=True, text=True, timeout=timeout)


def check_output(binary: Path, *, count: bool = True) -> None:
    result = run(binary, timeout=60)
    check(result.returncode == 0 and result.stdout == EXPECTED_OUTPUT and not result.stderr,
          f'{binary}: {result.returncode}\n{result.stdout}\n{result.stderr}', count=count)


def strict_compile(compiler: list[str], optimization: str, executable: Path,
                   *, sanitizer: bool = False, count: bool = True) -> None:
    flags = ['-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', optimization,
             '-I' + str(ROOT / 'Runtime/include')]
    if os.name != 'nt':
        flags.append('-pthread')
    if sanitizer:
        flags += ['-fsanitize=undefined', '-fno-sanitize-recover=undefined']
    result = run(*compiler, *flags, GENERATED, *RUNTIME, '-lm', '-o', executable, timeout=240)
    check(result.returncode == 0 and not result.stderr,
          f'strict C {compiler}: {result.returncode}\n{result.stdout}\n{result.stderr}', count=count)


def check_diagnostic(name: str, code: str, message: str) -> None:
    source = ROOT / 'Tests' / 'CollectionExpressionTargetTypingDiagnostics' / name / 'Program.void'
    result = run(VOIDC, 'check', source, '--diagnostics=json')
    reports = [json.loads(line) for line in result.stderr.splitlines()
               if line.startswith('{') and 'schemaVersion' in line]
    valid = result.returncode != 0 and len(reports) == 1
    if valid:
        diagnostic = reports[0]
        span = diagnostic.get('span') or {}
        start = (span.get('start') or {}).get('offset', -1)
        end = (span.get('end') or {}).get('offset', -1)
        valid = (diagnostic.get('code') == code and message in diagnostic.get('message', '') and
                 diagnostic.get('severity') == 'error' and
                 0 <= start <= end <= len(source.read_text(encoding='utf-8')) and
                 diagnostic.get('file', '').endswith('/' + name + '/Program.void'))
        # Conversion failures should indicate the element, not the whole declaration.
        if valid and name in {'InvalidLocal', 'InvalidAssignment', 'InvalidReturn', 'GenericMismatch'}:
            text = source.read_text(encoding='utf-8')
            element = '"bad"' if name != 'GenericMismatch' else 'new Cell<string>()'
            valid = text.find(element) <= start <= text.find(element) + len(element)
    check(valid, f'diagnostic {name}: {result.returncode}\n{result.stderr}')


def main() -> None:
    version = run(VOIDC, 'version')
    check(version.returncode == 0 and version.stdout.strip() == 'voidc 0.0.380',
          f'version: {version.stdout} {version.stderr}')
    result = run(VOIDC, 'build', FIXTURE)
    check(result.returncode == 0 and not result.stderr,
          f'build: {result.stdout}\n{result.stderr}')
    check_output(EXECUTABLE)
    parsed = run(VOIDC, 'parse', ROOT / 'Tests' /
                 'CollectionExpressionTargetTypingDiagnostics' / 'VarNonempty' / 'Program.void')
    check(parsed.returncode == 0 and 'CollectionExpression' in parsed.stdout,
          'untyped syntax must stay target-neutral in AST')
    first = GENERATED.read_bytes()
    rebuilt = run(VOIDC, 'build', FIXTURE)
    check(rebuilt.returncode == 0 and not rebuilt.stderr and GENERATED.read_bytes() == first,
          'generated C must be deterministic')
    check('vc_array_new(' in first.decode('utf-8'),
          'collection syntax must share native array allocator')

    with tempfile.TemporaryDirectory(prefix='void363-collections-') as scratch:
        folder = Path(scratch)
        cc = shlex.split(os.environ.get('CC', 'cc'))
        for optimization in ('-O0', '-O2'):
            binary = folder / ('gcc' + optimization + ('.exe' if os.name == 'nt' else ''))
            strict_compile(cc, optimization, binary)
            check_output(binary)
        if shutil.which('clang') and os.name != 'nt':
            binary = folder / 'clang-strict'
            strict_compile(['clang'], '-O2', binary, count=False)
            check_output(binary, count=False)
            print('Clang strict C: passed')
        else:
            print('Clang strict C: unavailable')
        if os.name != 'nt':
            binary = folder / 'ubsan'
            strict_compile(cc, '-O1', binary, sanitizer=True, count=False)
            check_output(binary, count=False)
            print('UBSan: passed')
        else:
            print('UBSan: unavailable')

    for name, (code, message) in NEGATIVES.items():
        check_diagnostic(name, code, message)
    if CHECKS != 28:
        raise AssertionError(f'expected 28 focused checks, got {CHECKS}')
    print(f'# {CHECKS} checks')


if __name__ == '__main__':
    main()
