#!/usr/bin/env python3
"""#372: public float/double Math signatures, native link, domains and strict C."""
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
FIXTURE = ROOT / 'Tests/FloatingPointMathFoundations'
GENERATED = FIXTURE / '.void/FloatingPointMathFoundations.c'
EXECUTABLE = FIXTURE / 'bin' / ('FloatingPointMathFoundations' + ('.exe' if os.name == 'nt' else ''))
RUNTIME = sorted((ROOT / 'Runtime/src').glob('*.c'))
EXPECTED = 'True\n' * 50
COUNT = 0
NEGATIVES = {
    'MathWrongType': ('VOID3003', "type 'Math' has no matching static method 'Sqrt'"),
    'MathFDoubleArgument': ('VOID3003', "type 'MathF' has no matching static method 'Sqrt'"),
    'MathFArity': ('VOID3003', "type 'MathF' has no matching static method 'Atan2'"),
    'MathFString': ('VOID3003', "type 'MathF' has no matching static method 'Log'"),
    'MathReturnNarrowing': ('VOID3002', "cannot assign 'double' to local 'x' of type 'float'"),
    'PrivateNativeAccess': ('VOID3000', "unsafe method 'NativeSqrt' requires an unsafe method"),
    'ConflictingNativeSymbol': ('VOID3000', "native symbol 'void372_conflict' is already declared"),
}


def check(ok: bool, detail: str) -> None:
    global COUNT
    if not ok:
        raise AssertionError(detail)
    COUNT += 1
    print('True', flush=True)


def invoke(*args: object, timeout: int = 180, env: dict[str, str] | None = None) -> subprocess.CompletedProcess[str]:
    full_env = dict(os.environ)
    full_env.setdefault('TERM', 'xterm')
    if env:
        full_env.update(env)
    return subprocess.run([str(x) for x in args], cwd=ROOT, env=full_env,
                          capture_output=True, text=True, encoding='utf-8', timeout=timeout)


def verify_output(path: Path) -> str:
    result = invoke(path)
    check(result.returncode == 0 and not result.stderr and result.stdout == EXPECTED,
          f'{path}: {result.returncode}\n{result.stdout}\n{result.stderr}')
    return result.stdout


def verify_negative(name: str, expected: tuple[str, str]) -> None:
    source = ROOT / 'Tests/FloatingPointMathFoundationsDiagnostics' / name / 'Program.void'
    target = source.parent if name == 'ConflictingNativeSymbol' else source
    result = invoke(COMPILER, 'check', target, '--diagnostics=json')
    diagnostics = [json.loads(line) for line in result.stderr.splitlines()
                   if line.startswith('{') and 'schemaVersion' in line]
    valid = result.returncode != 0 and len(diagnostics) == 1
    if valid:
        d = diagnostics[0]
        start = (d.get('span') or {}).get('start') or {}
        end = (d.get('span') or {}).get('end') or {}
        valid = (d.get('code') == expected[0] and d.get('message') == expected[1]
                 and d.get('severity') == 'error'
                 and 0 <= start.get('offset', -1) < end.get('offset', -1)
                 <= len(source.read_text(encoding='utf-8'))
                 and d.get('file', '').endswith('/' + name + '/Program.void'))
    check(valid, f'{name}: {result.returncode}\n{result.stdout}\n{result.stderr}')


def strict_compile(cc: list[str], binary: Path, *extra: str) -> None:
    flags = ['-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2',
             '-I' + str(ROOT / 'Runtime/include')]
    if os.name != 'nt':
        flags.append('-pthread')
    result = invoke(*cc, *flags, *extra, GENERATED, *RUNTIME, '-lm', '-o', binary, timeout=240)
    check(result.returncode == 0 and not result.stderr,
          f'{cc} strict generated C: {result.returncode}\n{result.stdout}\n{result.stderr}')


def main() -> None:
    version = invoke(COMPILER, 'version')
    check(version.returncode == 0 and version.stdout.strip() == 'voidc 0.0.380',
          f'version: {version.stdout}\n{version.stderr}')
    project = (FIXTURE / 'FloatingPointMathFoundations.voidproj').read_text(encoding='utf-8')
    check('"version":"0.0.372"' in project and '"libraries"' not in project,
          'standard library math must not require user-project linker configuration')
    built = invoke(COMPILER, 'build', FIXTURE)
    check(built.returncode == 0 and not built.stderr,
          f'native auto-link: {built.returncode}\n{built.stdout}\n{built.stderr}')
    baseline = verify_output(EXECUTABLE)
    generated = GENERATED.read_bytes()
    rebuilt = invoke(COMPILER, 'build', FIXTURE)
    check(rebuilt.returncode == 0 and not rebuilt.stderr and GENERATED.read_bytes() == generated,
          f'deterministic generated C: {rebuilt.stdout}\n{rebuilt.stderr}')

    math = (ROOT / 'StandardLibrary/Void/Math.void').read_text(encoding='utf-8')
    mathf = (ROOT / 'StandardLibrary/Void/MathF.void').read_text(encoding='utf-8')
    functions = {'Sqrt': 'sqrt', 'Pow': 'pow', 'Sin': 'sin', 'Cos': 'cos',
                 'Tan': 'tan', 'Atan2': 'atan2', 'Exp': 'exp',
                 'Log': 'log', 'Log10': 'log10'}
    for typ, source, suffix in [('double', math, ''), ('float', mathf, 'f')]:
        signatures = [f'public static {typ} {name}(' for name in functions]
        native = [f'[Native("{name}{suffix}")]' for name in functions.values()]
        private = [f'private static unsafe extern {typ} Native{name}(' for name in functions]
        check(all(x in source for x in signatures + native + private)
              and source.count('private static unsafe extern') >= len(functions),
              f'incomplete {typ} source-visible math API')
    check(all(f'extern double {symbol}(' in generated.decode('utf-8') for symbol in functions.values())
          and all(f'extern float {symbol}f(' in generated.decode('utf-8') for symbol in functions.values()),
          'float/double C ABI declarations must be distinct and correct')
    check('public static int Min(int a, int b)' in math and
          'public static int Clamp(int value, int min, int max)' in math and
          'public static long Abs(long value)' in math,
          '#371 integer Math overloads were lost')
    for name, expected in NEGATIVES.items():
        verify_negative(name, expected)

    # The user's existing NativeLibraries fixture re-imports sqrtf, cosf and sinf.
    # Identical native C signatures are legal; conflicting ones are rejected above.
    native = ROOT / 'Tests/NativeLibraries'
    native_build = invoke(COMPILER, 'build', native)
    check(native_build.returncode == 0 and not native_build.stderr,
          f'repeated compatible native imports: {native_build.stdout}\n{native_build.stderr}')
    native_binary = native / 'bin' / ('NativeLibraries' + ('.exe' if os.name == 'nt' else ''))
    native_result = invoke(native_binary)
    check(native_result.returncode == 0 and not native_result.stderr
          and native_result.stdout == 'True\n' * 3,
          f'native math imports: {native_result.stdout}\n{native_result.stderr}')

    with tempfile.TemporaryDirectory(prefix='void372-float-') as scratch:
        folder = Path(scratch)
        gcc = shlex.split(os.environ.get('CC', 'gcc'))
        gcc_binary = folder / ('math-gcc' + ('.exe' if os.name == 'nt' else ''))
        strict_compile(gcc, gcc_binary)
        check(verify_output(gcc_binary) == baseline, 'GCC and voidc results differ')
        clang = shutil.which('clang')
        if clang:
            clang_binary = folder / ('math-clang' + ('.exe' if os.name == 'nt' else ''))
            strict_compile([clang], clang_binary)
            check(verify_output(clang_binary) == baseline, 'Clang and GCC results differ')
        else:
            check(True, 'Clang not available')
            check(True, 'Clang not available')
            check(True, 'Clang not available')
        probe = folder / 'probe.c'
        probe.write_text('int main(void) { return 0; }\n', encoding='utf-8')
        supported = invoke(*gcc, '-fsanitize=undefined', probe, '-o', folder / 'probe')
        if supported.returncode == 0:
            binary = folder / ('math-ubsan' + ('.exe' if os.name == 'nt' else ''))
            strict_compile(gcc, binary, '-fsanitize=undefined', '-fno-sanitize-recover=undefined')
            check(verify_output(binary) == baseline, 'UBSan result differs from GCC')
        else:
            check(True, 'UBSan unavailable')
            check(True, 'UBSan unavailable')
            check(True, 'UBSan unavailable')
    print(f'# {COUNT} checks')


if __name__ == '__main__':
    main()
