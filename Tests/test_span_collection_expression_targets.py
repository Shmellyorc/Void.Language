#!/usr/bin/env python3
"""#365: span collection targets share array construction/conversions and ref safety."""
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
FIXTURE = ROOT / 'Tests' / 'SpanCollectionExpressionTargets'
NAME = 'SpanCollectionExpressionTargets'
GENERATED = FIXTURE / '.void' / (NAME + '.c')
EXECUTABLE = FIXTURE / 'bin' / (NAME + ('.exe' if os.name == 'nt' else ''))
NULL_FIXTURE = ROOT / 'Tests' / 'SpanCollectionSpreadNullFailure'
NULL_EXE = NULL_FIXTURE / 'bin' / ('SpanCollectionSpreadNullFailure' + ('.exe' if os.name == 'nt' else ''))
RUNTIME = sorted((ROOT / 'Runtime/src').glob('*.c'))
EXPECTED_OUTPUT = 'True\n' * 20
CHECKS = 0

NEGATIVES = {
    'InvalidElement': ('VOID3000', "cannot assign 'string' to array initializer element of type 'int'"),
    'InvalidReadonlyElement': ('VOID3000', "cannot assign 'string' to array initializer element of type 'int'"),
    'InvalidSpreadElement': ('VOID3000', "cannot convert spread array element 'string' to 'int'"),
    'InvalidSpreadSource': ('VOID3000', 'spread source must be a one-dimensional array'),
    'Untyped': ('VOID3000', 'requires a one-dimensional array target type'),
    'NonSpanTarget': ('VOID3000', 'requires a one-dimensional array target type'),
    'ReadOnlyMutation': ('VOID3004', 'cannot assign through a readonly receiver'),
    'ClassFieldEscape': ('VOID3000', "ref struct field 'Items' is allowed only as instance storage"),
    'StaticFieldEscape': ('VOID3000', "ref struct field 'Items' is allowed only as instance storage"),
    'AsyncRefStruct': ('VOID3000', "ref struct local 'items' cannot be used in async or iterator"),
    'SpanSourceSpread': ('VOID3000', 'spread source must be a one-dimensional array'),
    'MissingElement': ('VOID2001', "expected expression, found ','"),
    'MissingBracket': ('VOID2001', "expected ']', found ';'"),
    'AmbiguousEmptyOverload': ('VOID3003', "call to 'Accept' is ambiguous"),
    'ReturnLocalRef': ('VOID3000', 'cannot return ref struct value that refers to scoped or local storage'),
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


def check_output(executable: Path, *, count: bool = True, env: dict[str, str] | None = None) -> None:
    result = run(executable, timeout=90, env=env)
    check(result.returncode == 0 and result.stdout == EXPECTED_OUTPUT and not result.stderr,
          f'wrong runtime output {executable}: {result.returncode}\n{result.stdout}\n{result.stderr}', count=count)


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
          f'strict-C {compiler} {opt} {sanitizer}: {result.returncode}\n'
          f'{result.stdout}\n{result.stderr}', count=count)


def negative(name: str, code: str, message: str) -> None:
    source = ROOT / 'Tests' / 'SpanCollectionExpressionDiagnostics' / name / 'Program.void'
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
        # Element and spread errors must locate the offending source token.
        if valid and name in {'InvalidElement', 'InvalidReadonlyElement'}:
            valid = text.find('"bad"') <= a <= text.find('"bad"') + 5
        if valid and name in {'InvalidSpreadElement', 'InvalidSpreadSource', 'SpanSourceSpread'}:
            valid = text.rfind('..') <= a <= text.rfind('..') + 2
    check(valid, f'negative {name}: {result.returncode}\n{result.stderr}')


def main() -> None:
    result = run(VOIDC, 'version')
    check(result.returncode == 0 and result.stdout.strip() == 'voidc 0.0.380',
          f'compiler version: {result.stdout}\n{result.stderr}')
    result = run(VOIDC, 'build', FIXTURE, timeout=240)
    check(result.returncode == 0 and not result.stderr,
          f'build fixture: {result.stdout}\n{result.stderr}')
    check_output(EXECUTABLE)
    syntax = run(VOIDC, 'parse', FIXTURE / 'Program.void')
    check(syntax.returncode == 0 and 'CollectionExpression' in syntax.stdout and
          'SpreadElement' in syntax.stdout, 'parser must preserve target-neutral collection syntax')
    generated = GENERATED.read_bytes()
    result = run(VOIDC, 'build', FIXTURE, timeout=240)
    check(result.returncode == 0 and not result.stderr and GENERATED.read_bytes() == generated,
          'generated C must be deterministic')
    c_text = generated.decode('utf-8')
    check('vc_array_new(sizeof(' in c_text and 'vc_spread_copy_' in c_text and
          'vc_array_checked_add_length(' in c_text,
          'span spreads must share checked array allocation and typed spread-copy machinery')
    check('vc_gc_root_push(' in c_text and '.owner = (void *)' in c_text,
          'span backing storage needs established GC roots and managed ref owner')
    check('){0})' in c_text, 'empty spans should use existing default zero-length representation')
    result = run(VOIDC, 'build', NULL_FIXTURE, timeout=240)
    check(result.returncode == 0 and not result.stderr,
          f'null spread fixture build: {result.stdout}\n{result.stderr}')
    result = run(NULL_EXE)
    check(result.returncode != 0 and 'array reference is null' in result.stderr and
          result.stdout == '',
          f'null spread must keep established runtime failure: {result.returncode}\n'
          f'{result.stdout}\n{result.stderr}')
    with tempfile.TemporaryDirectory(prefix='void365-span-collection-') as temporary:
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
            # ASan is a focused optional safety gate, not an excuse to skip UBSan.
            target = folder / 'asan'
            strict_compile(gcc, '-O1', target, sanitizer='address', count=False)
            asan_env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0:abort_on_error=1')
            check_output(target, count=False, env=asan_env)
            print('ASan: passed', flush=True)
        else:
            print('ASan/UBSan: unavailable', flush=True)
    for name, (code, message) in NEGATIVES.items():
        negative(name, code, message)
    print(f'# {CHECKS} checks')


if __name__ == '__main__':
    main()
