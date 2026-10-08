#!/usr/bin/env python3
"""#364: array-only spread binding, staging, conversions, GC and diagnostics."""
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
NAME = 'ArrayCollectionSpreadExpressions'
FIXTURE = ROOT / 'Tests' / NAME
GENERATED = FIXTURE / '.void' / (NAME + '.c')
EXECUTABLE = FIXTURE / 'bin' / (NAME + ('.exe' if os.name == 'nt' else ''))
NULL_FIXTURE = ROOT / 'Tests' / 'ArrayCollectionSpreadNullFailure'
NULL_EXECUTABLE = NULL_FIXTURE / 'bin' / ('ArrayCollectionSpreadNullFailure' + ('.exe' if os.name == 'nt' else ''))
RUNTIME = sorted((ROOT / 'Runtime/src').glob('*.c'))
EXPECTED_OUTPUT = 'True\n' * 18
CHECKS = 0

NEGATIVES = {
    'NonArraySource': ('VOID3000', 'spread source must be a one-dimensional array'),
    'WrongSourceElement': ('VOID3000', "cannot convert spread array element 'string' to 'int'"),
    'WrongValueElement': ('VOID3000', "cannot assign 'string' to array initializer element of type 'int'"),
    'WrongReturnElement': ('VOID3000', "cannot convert spread array element 'string' to 'int'"),
    'WrongArgumentElement': ('VOID3003', "no matching method 'Accept'"),
    'InvalidRank': ('VOID3000', 'spread source must be a one-dimensional array'),
    'Untyped': ('VOID3000', 'requires a one-dimensional array target type'),
    'UntypedEmpty': ('VOID3000', 'requires a one-dimensional array target type'),
    'ScalarTarget': ('VOID3000', 'requires a one-dimensional array target type'),
    'ObjectTarget': ('VOID3000', 'requires a one-dimensional array target type'),
    'MissingSource': ('VOID2001', "expected expression, found ']'"),
    'MissingAfterValue': ('VOID2001', "expected expression, found ']'"),
    'DoubleDot': ('VOID3000', "range end must be convertible to 'Index'"),
    'MissingComma': ('VOID2001', "expected ']', found 'number'"),
    'SpreadOutsideCollection': ('VOID3000', "range end must be convertible to 'Index'"),
    'InvalidNestedSource': ('VOID3000', 'spread source must be a one-dimensional array'),
}


def check(value: bool, label: str, *, count: bool = True) -> None:
    global CHECKS
    if not value:
        raise AssertionError(label)
    if count:
        CHECKS += 1
        print('True', flush=True)


def run(*args: object, timeout: int = 150) -> subprocess.CompletedProcess[str]:
    return subprocess.run([str(a) for a in args], cwd=ROOT,
                          capture_output=True, text=True, timeout=timeout)


def check_output(executable: Path, *, count: bool = True) -> None:
    result = run(executable, timeout=60)
    check(result.returncode == 0 and result.stdout == EXPECTED_OUTPUT and not result.stderr,
          f'wrong output {executable}: {result.returncode}\n{result.stdout}\n{result.stderr}', count=count)


def strict_compile(compiler: list[str], optimization: str, output: Path, *,
                   sanitizer: bool = False, count: bool = True) -> None:
    flags = ['-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', optimization,
             '-I' + str(ROOT / 'Runtime/include')]
    if os.name != 'nt':
        flags.append('-pthread')
    if sanitizer:
        flags += ['-fsanitize=undefined', '-fno-sanitize-recover=undefined']
    result = run(*compiler, *flags, GENERATED, *RUNTIME, '-lm', '-o', output, timeout=240)
    check(result.returncode == 0 and not result.stderr,
          f'strict C {compiler} {optimization}: {result.returncode}\n'
          f'{result.stdout}\n{result.stderr}', count=count)


def check_negative(name: str, code: str, message: str) -> None:
    source = ROOT / 'Tests' / 'ArrayCollectionSpreadDiagnostics' / name / 'Program.void'
    result = run(VOIDC, 'check', source, '--diagnostics=json')
    reports = [json.loads(line) for line in result.stderr.splitlines()
               if line.startswith('{') and 'schemaVersion' in line]
    valid = result.returncode != 0 and len(reports) == 1
    if valid:
        item = reports[0]
        start = ((item.get('span') or {}).get('start') or {}).get('offset', -1)
        end = ((item.get('span') or {}).get('end') or {}).get('offset', -1)
        text = source.read_text(encoding='utf-8')
        valid = (item.get('code') == code and message in item.get('message', '') and
                 item.get('severity') == 'error' and
                 item.get('file', '').endswith('/' + name + '/Program.void') and
                 0 <= start <= end <= len(text))
        if valid and name in {'NonArraySource', 'WrongSourceElement',
                              'WrongReturnElement', 'InvalidNestedSource', 'InvalidRank'}:
            # Source-facing diagnostics must point to the offending spread.
            valid = 0 <= start - text.find('..') <= 2
        if valid and name == 'WrongValueElement':
            valid = text.find('"bad"') <= start <= text.find('"bad"') + 5
    check(valid, f'diagnostic {name}: {result.returncode}\n{result.stderr}')


def main() -> None:
    result = run(VOIDC, 'version')
    check(result.returncode == 0 and result.stdout.strip() == 'voidc 0.0.370',
          f'version: {result.stdout} {result.stderr}')
    result = run(VOIDC, 'build', FIXTURE)
    check(result.returncode == 0 and not result.stderr,
          f'build: {result.stdout}\n{result.stderr}')
    check_output(EXECUTABLE)
    syntax = run(VOIDC, 'parse', FIXTURE / 'Program.void')
    check(syntax.returncode == 0 and 'SpreadElement' in syntax.stdout and
          'CollectionExpression' in syntax.stdout,
          'spread must remain a target-neutral collection AST element')
    first = GENERATED.read_bytes()
    rebuilt = run(VOIDC, 'build', FIXTURE)
    check(rebuilt.returncode == 0 and not rebuilt.stderr and GENERATED.read_bytes() == first,
          'generated C must be deterministic')
    output = first.decode('utf-8')
    check('vc_array_new(sizeof(' in output and 'vc_spread_copy_' in output,
          'spread construction must share the normal array allocator')
    check('vc_gc_root_push(' in output and 'vc_array_checked_add_length(' in output,
          'spread staging must root managed sources and check bounds')
    check('vc_array_length(' in output and 'array reference is null' in output,
          'spread must retain the runtime null-array contract')
    result = run(VOIDC, 'build', NULL_FIXTURE)
    check(result.returncode == 0 and not result.stderr,
          f'null fixture build: {result.stdout}\n{result.stderr}')
    result = run(NULL_EXECUTABLE)
    check(result.returncode != 0 and 'array reference is null' in result.stderr and
          '0\n' not in result.stdout,
          f'null spread must fail: {result.returncode}\n{result.stdout}\n{result.stderr}')
    with tempfile.TemporaryDirectory(prefix='void364-array-spreads-') as directory:
        folder = Path(directory)
        cc = shlex.split(os.environ.get('CC', 'cc'))
        for opt in ('-O0', '-O2'):
            binary = folder / ('gcc' + opt + ('.exe' if os.name == 'nt' else ''))
            strict_compile(cc, opt, binary)
            check_output(binary)
        if os.name != 'nt' and shutil.which('clang'):
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
        check_negative(name, code, message)
    if CHECKS != 30:
        raise AssertionError(f'expected 30 focused checks, got {CHECKS}')
    print(f'# {CHECKS} checks')


if __name__ == '__main__':
    main()
