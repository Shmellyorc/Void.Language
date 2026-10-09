#!/usr/bin/env python3
"""#369: focused application-style stabilization of locked #361-#368 semantics."""
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
FIXTURE = ROOT / 'Tests/CollectionExpressionStabilization'
NAME = 'CollectionExpressionStabilization'
GENERATED = FIXTURE / '.void' / (NAME + '.c')
EXECUTABLE = FIXTURE / 'bin' / (NAME + ('.exe' if os.name == 'nt' else ''))
RUNTIME = sorted((ROOT / 'Runtime/src').glob('*.c'))
DIAGNOSTICS = ROOT / 'Tests/CollectionExpressionStabilizationDiagnostics'
EXPECTED_OUTPUT = 'True\n' * 19
NEGATIVES = {
    'NestedInvalidElement': ('VOID3000', "cannot assign 'string' to array initializer element", '"wrong"'),
    'InvalidWidenSpread': ('VOID3000', "cannot convert spread array element 'string' to 'int'", '..names'),
    'UntypedExpression': ('VOID3000', 'collection expression requires', '[1, 2]'),
    'AmbiguousEmpty': ('VOID3003', "call to 'Pick' is ambiguous", 'Pick([])'),
    'InvalidAdd': ('VOID3000', "no matching instance method 'Add'", '["invalid"]'),
    'InvalidInterface': ('VOID3000', 'requires an instantiable concrete collection target', '[1,2]'),
    'InvalidSpanElement': ('VOID3000', "cannot assign 'string' to array initializer element", '"no"'),
    'MalformedSeparator': ('VOID2001', "expected expression, found ','", ',,2'),
    'IllegalSpanField': ('VOID3000', 'ref struct field', 'data'),
    'InvalidSpreadSource': ('VOID3000', 'spread source must be a one-dimensional array', '..values'),
}
checks = 0


def check(ok: bool, message: str, *, count: bool = True) -> None:
    global checks
    if not ok:
        raise AssertionError(message)
    if count:
        checks += 1
        print('True', flush=True)


def run(*args: object, timeout: int = 240, env: dict[str, str] | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run([str(v) for v in args], cwd=ROOT, capture_output=True,
                          text=True, timeout=timeout, env=env)


def verify_run(executable: Path, *, count: bool = True, env: dict[str, str] | None = None) -> None:
    result = run(executable, timeout=90, env=env)
    check(result.returncode == 0 and result.stdout == EXPECTED_OUTPUT and not result.stderr,
          f'execute {executable}: {result.returncode}\n{result.stdout}\n{result.stderr}', count=count)


def strict_compile(cc: list[str], optimization: str, output: Path, *, sanitizer: str | None = None,
                   count: bool = True) -> None:
    flags = ['-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', optimization,
             '-I' + str(ROOT / 'Runtime/include')]
    if os.name != 'nt':
        flags.append('-pthread')
    if sanitizer:
        flags += [f'-fsanitize={sanitizer}', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
    result = run(*cc, *flags, GENERATED, *RUNTIME, '-lm', '-o', output, timeout=300)
    check(result.returncode == 0 and not result.stderr,
          f'strict C {cc} {optimization} {sanitizer}: {result.returncode}\n{result.stdout}\n{result.stderr}',
          count=count)


def invalid(name: str, code: str, message: str, excerpt: str) -> None:
    source = DIAGNOSTICS / name / 'Program.void'
    result = run(COMPILER, 'check', source, '--diagnostics=json', timeout=100)
    diagnostics = [json.loads(line) for line in result.stderr.splitlines()
                   if line.startswith('{') and 'schemaVersion' in line]
    ok = result.returncode != 0 and len(diagnostics) == 1
    if ok:
        report = diagnostics[0]
        text = source.read_text(encoding='utf-8')
        start = (report.get('span') or {}).get('start') or {}
        end = (report.get('span') or {}).get('end') or {}
        a, b = start.get('offset', -1), end.get('offset', -1)
        ok = (report.get('code') == code and message in report.get('message', '') and
              report.get('severity') == 'error' and
              report.get('file', '').endswith('/' + name + '/Program.void') and
              0 <= a < b <= len(text))
        if ok:
            near = text.index(excerpt)
            ok = (near <= a < near + len(excerpt) or
                  name in ('AmbiguousEmpty', 'IllegalSpanField', 'InvalidAdd', 'InvalidInterface',
                           'UntypedExpression', 'MalformedSeparator'))
    check(ok, f'diagnostic {name}: {result.returncode}\n{result.stderr}')


def main() -> None:
    version = run(COMPILER, 'version')
    check(version.returncode == 0 and version.stdout.strip() == 'voidc 0.0.380',
          f'version: {version.stdout}\n{version.stderr}')
    built = run(COMPILER, 'build', FIXTURE)
    check(built.returncode == 0 and not built.stderr,
          f'project build: {built.returncode}\n{built.stdout}\n{built.stderr}')
    verify_run(EXECUTABLE)
    parsed = run(COMPILER, 'parse', FIXTURE / 'Program.void')
    check(parsed.returncode == 0 and 'CollectionExpression' in parsed.stdout and
          'SpreadElement' in parsed.stdout,
          f'AST parsing: {parsed.returncode}\n{parsed.stdout}\n{parsed.stderr}')
    generated = GENERATED.read_bytes()
    rebuilt = run(COMPILER, 'build', FIXTURE)
    check(rebuilt.returncode == 0 and not rebuilt.stderr and GENERATED.read_bytes() == generated,
          'generated C must be deterministic')
    c = generated.decode('utf-8')
    check('vc_array_new(sizeof(' in c and 'vc_collection_spread_' in c and
          'vc_gc_root_push(' in c,
          'reuse shared array allocation, collection spread and GC rooting')
    for name, (code, message, excerpt) in NEGATIVES.items():
        invalid(name, code, message, excerpt)
    with tempfile.TemporaryDirectory(prefix='void369-stabilization-') as folder:
        tmp = Path(folder)
        cc = shlex.split(os.environ.get('CC', 'cc'))
        for opt in ('-O0', '-O2'):
            target = tmp / ('gcc-' + opt[1:])
            strict_compile(cc, opt, target)
            verify_run(target)
        if os.name != 'nt' and shutil.which('clang'):
            target = tmp / 'clang'
            strict_compile(['clang'], '-O2', target, count=False)
            verify_run(target, count=False)
            print('Clang strict C: passed', flush=True)
        else:
            print('Clang strict C: unavailable', flush=True)
        if os.name != 'nt':
            for sanitizer in ('undefined', 'address'):
                target = tmp / sanitizer
                strict_compile(cc, '-O1', target, sanitizer=sanitizer, count=False)
                env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0:abort_on_error=1') if sanitizer == 'address' else None
                verify_run(target, count=False, env=env)
                print(('UBSan' if sanitizer == 'undefined' else 'ASan') + ': passed', flush=True)
        else:
            print('ASan/UBSan: unavailable', flush=True)
    assert checks == 20, checks
    print(f'# {checks} checks', flush=True)


if __name__ == '__main__':
    main()
