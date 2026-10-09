#!/usr/bin/env python3
"""#366: collection expressions reuse new + instance Add for concrete targets."""
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
FIXTURE = ROOT / 'Tests/ListCustomCollectionExpressionConstruction'
NAME = 'ListCustomCollectionExpressionConstruction'
GENERATED = FIXTURE / '.void' / (NAME + '.c')
EXECUTABLE = FIXTURE / 'bin' / (NAME + ('.exe' if os.name == 'nt' else ''))
RUNTIME = sorted((ROOT / 'Runtime/src').glob('*.c'))
EXPECTED_OUTPUT = 'True\n' * 25
CHECKS = 0
NEGATIVES = {
    'MissingAdd': ('VOID3000', 'requires an accessible instance Add method'),
    'PrivateAdd': ('VOID3000', 'requires an accessible instance Add method'),
    'NoConstructor': ('VOID3000', 'requires an accessible parameterless constructor'),
    'PrivateConstructor': ('VOID3000', 'requires an accessible parameterless constructor'),
    'InvalidElement': ('VOID3000', "has no matching instance method 'Add'"),
    'InvalidGenericElement': ('VOID3000', "has no matching instance method 'Add'"),
    'StaticAdd': ('VOID3000', 'requires an accessible instance Add method'),
    'InterfaceTarget': ('VOID3000', 'requires an instantiable concrete collection target'),
    'Untyped': ('VOID3000', 'requires a one-dimensional array target type'),
    'UntypedEmpty': ('VOID3000', 'requires a one-dimensional array target type'),
    'EnumerableSpreadTarget': ('VOID3000', 'collection spread source must be a one-dimensional array or Span/ReadOnlySpan'),
    'MissingElement': ('VOID2001', "expected expression, found ','"),
    'MissingBracket': ('VOID2001', "expected ']', found ';'"),
    'AmbiguousEmptyOverload': ('VOID3003', "call to 'Accept' is ambiguous"),
    'AmbiguousAdd': ('VOID3000', "collection initializer call to 'Add' is ambiguous"),
}


def check(value: bool, label: str, *, count: bool = True) -> None:
    global CHECKS
    if not value:
        raise AssertionError(label)
    if count:
        CHECKS += 1
        print('True', flush=True)


def run(*args: object, timeout: int = 180, env: dict[str, str] | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run([str(arg) for arg in args], cwd=ROOT, capture_output=True,
                          text=True, timeout=timeout, env=env)


def check_output(executable: Path, *, count: bool = True,
                 env: dict[str, str] | None = None) -> None:
    result = run(executable, timeout=90, env=env)
    check(result.returncode == 0 and result.stdout == EXPECTED_OUTPUT and not result.stderr,
          f'wrong runtime output: {executable} ({result.returncode})\n'
          f'{result.stdout}\n{result.stderr}', count=count)


def strict_compile(compiler: list[str], opt: str, target: Path,
                   *, sanitizer: str | None = None, count: bool = True) -> None:
    flags = ['-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', opt,
             '-I' + str(ROOT / 'Runtime/include')]
    if os.name != 'nt':
        flags.append('-pthread')
    if sanitizer:
        flags += [f'-fsanitize={sanitizer}', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
    result = run(*compiler, *flags, GENERATED, *RUNTIME, '-lm', '-o', target, timeout=300)
    check(result.returncode == 0 and not result.stderr,
          f'strict-C {compiler} {opt} {sanitizer} failed: {result.returncode}\n'
          f'{result.stdout}\n{result.stderr}', count=count)


def negative(name: str, code: str, message: str) -> None:
    source = ROOT / 'Tests/ListCustomCollectionExpressionDiagnostics' / name / 'Program.void'
    result = run(VOIDC, 'check', source, '--diagnostics=json', timeout=100)
    reports = [json.loads(line) for line in result.stderr.splitlines()
               if line.startswith('{') and 'schemaVersion' in line]
    valid = result.returncode != 0 and len(reports) == 1
    if valid:
        report = reports[0]
        text = source.read_text(encoding='utf-8')
        start = (report.get('span') or {}).get('start') or {}
        end = (report.get('span') or {}).get('end') or {}
        a, b = start.get('offset', -1), end.get('offset', -1)
        valid = (report.get('code') == code and message in report.get('message', '') and
                 report.get('severity') == 'error' and
                 report.get('file', '').endswith('/' + name + '/Program.void') and
                 0 <= a <= b <= len(text))
        if valid and name in {'InvalidElement', 'InvalidGenericElement'}:
            valid = text.find('"bad"') <= a <= text.find('"bad"') + 5
        if valid and name == 'EnumerableSpreadTarget':
            valid = text.rfind('..') <= a <= text.rfind('..') + 2
    check(valid, f'negative {name}: {result.returncode}\n{result.stderr}')


def main() -> None:
    version = run(VOIDC, 'version')
    check(version.returncode == 0 and version.stdout.strip() == 'voidc 0.0.380',
          f'version: {version.stdout}\n{version.stderr}')
    result = run(VOIDC, 'build', FIXTURE, timeout=240)
    check(result.returncode == 0 and not result.stderr,
          f'build fixture: {result.stdout}\n{result.stderr}')
    check_output(EXECUTABLE)
    parsed = run(VOIDC, 'parse', FIXTURE / 'Program.void')
    check(parsed.returncode == 0 and 'CollectionExpression' in parsed.stdout,
          'collection AST must remain target-neutral')
    generated = GENERATED.read_bytes()
    again = run(VOIDC, 'build', FIXTURE, timeout=240)
    check(again.returncode == 0 and not again.stderr and
          GENERATED.read_bytes() == generated, 'generated C must be deterministic')
    content = generated.decode('utf-8')
    check('vc_gc_root_push(' in content and 'Add()' in content and
          'vc_new_' in content, 'construction must use existing rooted new/Add emission')
    check('vc_array_new(sizeof(' in content,
          'existing array/span collection lowering must remain active')
    with tempfile.TemporaryDirectory(prefix='void366-list-custom-') as temporary:
        folder = Path(temporary)
        gcc = shlex.split(os.environ.get('CC', 'cc'))
        for opt in ('-O0', '-O2'):
            target = folder / ('gcc-' + opt[1:])
            strict_compile(gcc, opt, target)
            check_output(target)
        if os.name != 'nt' and shutil.which('clang'):
            target = folder / 'clang-strict'
            strict_compile(['clang'], '-O2', target, count=False)
            check_output(target, count=False)
            print('Clang strict C: passed', flush=True)
        else:
            print('Clang strict C: unavailable', flush=True)
        if os.name != 'nt':
            target = folder / 'ubsan'
            strict_compile(gcc, '-O1', target, sanitizer='undefined', count=False)
            check_output(target, count=False)
            print('UBSan: passed', flush=True)
            target = folder / 'asan'
            strict_compile(gcc, '-O1', target, sanitizer='address', count=False)
            check_output(target, count=False,
                         env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0:abort_on_error=1'))
            print('ASan: passed', flush=True)
        else:
            print('ASan/UBSan: unavailable', flush=True)
    for name, (code, message) in NEGATIVES.items():
        negative(name, code, message)
    assert CHECKS == 26, CHECKS
    print(f'# {CHECKS} checks')


if __name__ == '__main__':
    main()
