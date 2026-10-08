#!/usr/bin/env python3
"""#367: structural constructor/Add plus array/span spread insertion."""
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
FIXTURE = ROOT / 'Tests/GeneralCollectionConstructionExtensibility'
NAME = 'GeneralCollectionConstructionExtensibility'
GENERATED = FIXTURE / '.void' / (NAME + '.c')
EXECUTABLE = FIXTURE / 'bin' / (NAME + ('.exe' if os.name == 'nt' else ''))
NULL_FIXTURE = ROOT / 'Tests/GeneralCollectionSpreadNullFailure'
NULL_EXECUTABLE = NULL_FIXTURE / 'bin' / ('GeneralCollectionSpreadNullFailure' + ('.exe' if os.name == 'nt' else ''))
RUNTIME = sorted((ROOT / 'Runtime/src').glob('*.c'))
EXPECTED_OUTPUT = 'True\n' * 38
NEGATIVES = {
    'UnsupportedEnumerable': ('VOID3000', 'spread source must be a one-dimensional array or Span/ReadOnlySpan'),
    'InvalidSpreadElement': ('VOID3000', "no matching instance method 'Add'"),
    'InvalidSpanElement': ('VOID3000', "no matching instance method 'Add'"),
    'InvalidSpreadRank': ('VOID3000', 'spread source must be a one-dimensional array or Span/ReadOnlySpan'),
    'MissingAdd': ('VOID3000', 'requires an accessible instance Add method'),
    'PrivateAdd': ('VOID3000', 'requires an accessible instance Add method'),
    'PrivateConstructor': ('VOID3000', 'requires an accessible parameterless constructor'),
    'AbstractTarget': ('VOID3000', 'requires an instantiable concrete collection target'),
    'InterfaceTarget': ('VOID3000', 'requires an instantiable concrete collection target'),
    'InvalidValue': ('VOID3000', "no matching instance method 'Add'"),
    'Untyped': ('VOID3000', 'requires a one-dimensional array target type'),
    'Malformed': ('VOID2001', "expected expression, found ']'"),
    'AmbiguousAdd': ('VOID3000', "collection initializer call to 'Add' is ambiguous"),
    'NonCollectionEmpty': ('VOID3000', 'requires an accessible instance Add method'),
}
CHECKS = 0


def check(ok: bool, label: str, *, count: bool = True) -> None:
    global CHECKS
    if not ok:
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
          f'runtime output {executable}: {result.returncode}\n{result.stdout}\n{result.stderr}',
          count=count)


def strict_compile(cc: list[str], opt: str, target: Path, *,
                   sanitizer: str | None = None, count: bool = True) -> None:
    flags = ['-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', opt,
             '-I' + str(ROOT / 'Runtime/include')]
    if os.name != 'nt':
        flags.append('-pthread')
    if sanitizer:
        flags += [f'-fsanitize={sanitizer}', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
    result = run(*cc, *flags, GENERATED, *RUNTIME, '-lm', '-o', target, timeout=300)
    check(result.returncode == 0 and not result.stderr,
          f'strict C {cc} {opt} {sanitizer}: {result.returncode}\n{result.stdout}\n{result.stderr}',
          count=count)


def negative(name: str, code: str, message: str) -> None:
    source = ROOT / 'Tests/GeneralCollectionConstructionDiagnostics' / name / 'Program.void'
    result = run(VOIDC, 'check', source, '--diagnostics=json', timeout=100)
    reports = [json.loads(line) for line in result.stderr.splitlines()
               if line.startswith('{') and 'schemaVersion' in line]
    ok = result.returncode != 0 and len(reports) == 1
    if ok:
        report = reports[0]
        text = source.read_text(encoding='utf-8')
        start = (report.get('span') or {}).get('start') or {}
        end = (report.get('span') or {}).get('end') or {}
        a, b = start.get('offset', -1), end.get('offset', -1)
        ok = (report.get('code') == code and message in report.get('message', '') and
              report.get('severity') == 'error' and
              report.get('file', '').endswith('/' + name + '/Program.void') and
              0 <= a <= b <= len(text))
        if ok and name in {'UnsupportedEnumerable', 'InvalidSpreadRank',
                           'InvalidSpanElement', 'InvalidSpreadElement'}:
            ok = text.rfind('..') <= a <= text.rfind('..') + 2
    check(ok, f'negative {name}: {result.returncode}\n{result.stderr}')


def main() -> None:
    version = run(VOIDC, 'version')
    check(version.returncode == 0 and version.stdout.strip() == 'voidc 0.0.370',
          f'version: {version.stdout}\n{version.stderr}')
    built = run(VOIDC, 'build', FIXTURE, timeout=240)
    check(built.returncode == 0 and not built.stderr,
          f'fixture build: {built.stdout}\n{built.stderr}')
    check_output(EXECUTABLE)
    parsed = run(VOIDC, 'parse', FIXTURE / 'Program.void')
    check(parsed.returncode == 0 and 'CollectionExpression' in parsed.stdout and
          'SpreadElement' in parsed.stdout, 'target-independent AST lost spread syntax')
    generated = GENERATED.read_bytes()
    rebuilt = run(VOIDC, 'build', FIXTURE, timeout=240)
    check(rebuilt.returncode == 0 and not rebuilt.stderr and
          GENERATED.read_bytes() == generated, 'generated C must be deterministic')
    c_text = generated.decode('utf-8')
    check('vc_collection_spread_' in c_text and 'vc_array_length(src)' in c_text and
          'src.vc_f_' in c_text and 'vc_gc_root_push(' in c_text,
          'spread insertion must reuse bound Add with rooted array/span sources')
    check('vc_array_new(sizeof(' in c_text and 'vc_spread_copy_' in c_text,
          'prior array spread and allocation paths must remain available')
    built_null = run(VOIDC, 'build', NULL_FIXTURE, timeout=240)
    check(built_null.returncode == 0 and not built_null.stderr,
          f'null-spread fixture build: {built_null.stdout}\n{built_null.stderr}')
    null = run(NULL_EXECUTABLE, timeout=90)
    check(null.returncode != 0 and null.stdout == '' and
          'array reference is null' in null.stderr,
          f'null array spread must use established runtime failure: {null.returncode}\n'
          f'{null.stdout}\n{null.stderr}')
    with tempfile.TemporaryDirectory(prefix='void367-structural-collection-') as temporary:
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
            for sanitizer in ('undefined', 'address'):
                target = folder / sanitizer
                strict_compile(gcc, '-O1', target, sanitizer=sanitizer, count=False)
                run_env = (dict(os.environ, ASAN_OPTIONS='detect_leaks=0:abort_on_error=1')
                           if sanitizer == 'address' else None)
                check_output(target, count=False, env=run_env)
                print(('UBSan' if sanitizer == 'undefined' else 'ASan') + ': passed', flush=True)
        else:
            print('ASan/UBSan: unavailable', flush=True)
    for name, (code, message) in NEGATIVES.items():
        negative(name, code, message)
    assert CHECKS == 27, CHECKS
    print(f'# {CHECKS} checks', flush=True)


if __name__ == '__main__':
    main()
