#!/usr/bin/env python3
"""#368: cross-feature collection expressions, shared async/iterator traversal."""
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
FIXTURE = ROOT / 'Tests/CollectionExpressionCrossFeatureIntegration'
NAME = 'CollectionExpressionCrossFeatureIntegration'
GENERATED = FIXTURE / '.void' / (NAME + '.c')
EXECUTABLE = FIXTURE / 'bin' / (NAME + ('.exe' if os.name == 'nt' else ''))
RUNTIME = sorted((ROOT / 'Runtime/src').glob('*.c'))
NEGATIVE = {
    'NestedInvalidElement': ('VOID3000', "cannot assign 'string' to array initializer element"),
    'GenericInvalidElement': ('VOID3000', "cannot assign 'string' to array initializer element"),
    'InvalidAddElement': ('VOID3000', "no matching instance method 'Add'"),
    'InvalidSpanField': ('VOID3000', 'ref struct field'),
    'UnsupportedEnumerableSpread': ('VOID3000', 'spread source must be a one-dimensional array'),
    'AmbiguousCollection': ('VOID3003', "call to 'Pick' is ambiguous"),
    'UntypedCollection': ('VOID3000', 'collection expression requires'),
}
EXPECTED_OUTPUT = 'True\n' * 25
checks = 0


def check(ok: bool, label: str, *, count: bool = True) -> None:
    global checks
    if not ok:
        raise AssertionError(label)
    if count:
        checks += 1
        print('True', flush=True)


def run(*args: object, timeout: int = 220, env: dict[str, str] | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run([str(x) for x in args], cwd=ROOT, capture_output=True,
                          text=True, timeout=timeout, env=env)


def verify_run(executable: Path, *, count: bool = True, env: dict[str, str] | None = None) -> None:
    result = run(executable, timeout=90, env=env)
    check(result.returncode == 0 and result.stdout == EXPECTED_OUTPUT and not result.stderr,
          f'{executable}: exit={result.returncode}\n{result.stdout}\n{result.stderr}', count=count)


def strict_compile(cc: list[str], optimization: str, dest: Path, *, sanitizer: str | None = None,
                   count: bool = True) -> None:
    flags = ['-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', optimization,
             '-I' + str(ROOT / 'Runtime/include')]
    if os.name != 'nt':
        flags.append('-pthread')
    if sanitizer:
        flags += [f'-fsanitize={sanitizer}', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
    result = run(*cc, *flags, GENERATED, *RUNTIME, '-lm', '-o', dest, timeout=300)
    check(result.returncode == 0 and not result.stderr,
          f'{cc} {optimization} {sanitizer}: {result.returncode}\n{result.stdout}\n{result.stderr}',
          count=count)


def invalid(name: str, code: str, message: str) -> None:
    source = ROOT / 'Tests/CollectionExpressionCrossFeatureDiagnostics' / name / 'Program.void'
    result = run(COMPILER, 'check', source, '--diagnostics=json', timeout=100)
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
              report.get('severity') == 'error' and report.get('file', '').endswith('/' + name + '/Program.void')
              and 0 <= a < b <= len(text))
        if ok and name == 'UnsupportedEnumerableSpread':
            ok = text.index('..original') <= a <= text.index('..original') + 2
    check(ok, f'negative {name}: {result.returncode}\n{result.stderr}')


def main() -> None:
    version = run(COMPILER, 'version')
    check(version.returncode == 0 and version.stdout.strip() == 'voidc 0.0.370',
          f'version: {version.stdout}\n{version.stderr}')
    built = run(COMPILER, 'build', FIXTURE, timeout=240)
    check(built.returncode == 0 and not built.stderr,
          f'fixture: {built.returncode}\n{built.stdout}\n{built.stderr}')
    verify_run(EXECUTABLE)
    parsed = run(COMPILER, 'parse', FIXTURE / 'Program.void', timeout=120)
    check(parsed.returncode == 0 and 'CollectionExpression' in parsed.stdout and
          'SpreadElement' in parsed.stdout, 'parser must retain target-neutral collection nodes')
    first = GENERATED.read_bytes()
    rebuilt = run(COMPILER, 'build', FIXTURE, timeout=240)
    check(rebuilt.returncode == 0 and not rebuilt.stderr and GENERATED.read_bytes() == first,
          'generated C must be deterministic')
    c_text = first.decode('utf-8')
    check('vc_collection_spread_' in c_text and 'vc_gc_root_push(' in c_text and
          'vc_array_new(sizeof(' in c_text, 'shared spread, rooting and array allocation not emitted')
    for name, (code, message) in NEGATIVE.items():
        invalid(name, code, message)
    with tempfile.TemporaryDirectory(prefix='void368-cross-feature-') as folder:
        tmp = Path(folder)
        gcc = shlex.split(os.environ.get('CC', 'cc'))
        for optimization in ('-O0', '-O2'):
            dest = tmp / ('gcc-' + optimization[1:])
            strict_compile(gcc, optimization, dest)
            verify_run(dest)
        if os.name != 'nt' and shutil.which('clang'):
            dest = tmp / 'clang'
            strict_compile(['clang'], '-O2', dest, count=False)
            verify_run(dest, count=False)
            print('Clang strict C: passed', flush=True)
        else:
            print('Clang strict C: unavailable', flush=True)
        if os.name != 'nt':
            for sanitizer in ('undefined', 'address'):
                dest = tmp / sanitizer
                strict_compile(gcc, '-O1', dest, sanitizer=sanitizer, count=False)
                env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0:abort_on_error=1') if sanitizer == 'address' else None
                verify_run(dest, count=False, env=env)
                print(('UBSan' if sanitizer == 'undefined' else 'ASan') + ': passed', flush=True)
        else:
            print('ASan/UBSan: unavailable', flush=True)
    assert checks == 17, checks
    print(f'# {checks} checks', flush=True)


if __name__ == '__main__':
    main()
