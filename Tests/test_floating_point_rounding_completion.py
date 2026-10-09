#!/usr/bin/env python3
"""#373: source-visible C11 rounding, midpoint-to-even and floating-point edge contracts."""
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
FIXTURE = ROOT / 'Tests/FloatingPointRoundingCompletion'
GENERATED = FIXTURE / '.void/FloatingPointRoundingCompletion.c'
EXECUTABLE = FIXTURE / 'bin' / ('FloatingPointRoundingCompletion' + ('.exe' if os.name == 'nt' else ''))
RUNTIME = sorted((ROOT / 'Runtime/src').glob('*.c'))
EXPECTED = 'True\n' * 80
COUNT = 0
NEGATIVES = {
    'MathStringArgument': ('VOID3003', "type 'Math' has no matching static method 'Round'"),
    'MathFDoubleArgument': ('VOID3003', "type 'MathF' has no matching static method 'Round'"),
    'MathFWrongArity': ('VOID3003', "type 'MathF' has no matching static method 'Floor'"),
    'MathReturnNarrowing': ('VOID3002', "cannot assign 'double' to local 'x' of type 'float'"),
    'MathIntegerNarrowing': ('VOID3002', "cannot assign 'double' to local 'x' of type 'int'"),
    'MathFPrivateNative': ('VOID3000', "unsafe method 'NativeRemainder' requires an unsafe method"),
}


def check(ok: bool, detail: str) -> None:
    global COUNT
    if not ok:
        raise AssertionError(detail)
    COUNT += 1
    print('True', flush=True)


def invoke(*args: object, timeout: int = 180) -> subprocess.CompletedProcess[str]:
    env = dict(os.environ)
    env.setdefault('TERM', 'xterm')
    return subprocess.run([str(x) for x in args], cwd=ROOT, env=env,
                          capture_output=True, text=True, encoding='utf-8', timeout=timeout)


def verify_output(path: Path) -> str:
    result = invoke(path)
    check(result.returncode == 0 and not result.stderr and result.stdout == EXPECTED,
          f'{path}: {result.returncode}\n{result.stdout}\n{result.stderr}')
    return result.stdout


def verify_negative(name: str, expected: tuple[str, str]) -> None:
    source = ROOT / 'Tests/FloatingPointRoundingCompletionDiagnostics' / name / 'Program.void'
    result = invoke(COMPILER, 'check', source, '--diagnostics=json')
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


def strict_compile(cc: list[str], binary: Path, optimization: str = '-O2', *extra: str) -> None:
    flags = ['-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', optimization,
             '-I' + str(ROOT / 'Runtime/include')]
    if os.name != 'nt':
        flags.append('-pthread')
    result = invoke(*cc, *flags, *extra, GENERATED, *RUNTIME, '-lm', '-o', binary, timeout=240)
    check(result.returncode == 0 and not result.stderr,
          f'{cc}, {optimization} generated C: {result.returncode}\n{result.stdout}\n{result.stderr}')


def main() -> None:
    version = invoke(COMPILER, 'version')
    check(version.returncode == 0 and version.stdout.strip() == 'voidc 0.0.381',
          f'version: {version.stdout}\n{version.stderr}')
    project = (FIXTURE / 'FloatingPointRoundingCompletion.voidproj').read_text(encoding='utf-8')
    check('"version":"0.0.373"' in project and '"libraries"' not in project,
          'math-library linking must not require per-project configuration')
    built = invoke(COMPILER, 'build', FIXTURE)
    check(built.returncode == 0 and not built.stderr,
          f'native auto-link: {built.returncode}\n{built.stdout}\n{built.stderr}')
    baseline = verify_output(EXECUTABLE)
    generated = GENERATED.read_bytes()
    rebuilt = invoke(COMPILER, 'build', FIXTURE)
    check(rebuilt.returncode == 0 and not rebuilt.stderr and GENERATED.read_bytes() == generated,
          f'deterministic C: {rebuilt.stdout}\n{rebuilt.stderr}')

    math = (ROOT / 'StandardLibrary/Void/Math.void').read_text(encoding='utf-8')
    mathf = (ROOT / 'StandardLibrary/Void/MathF.void').read_text(encoding='utf-8')
    functions = {'Floor': 'floor', 'Ceiling': 'ceil', 'Truncate': 'trunc'}
    for typ, source, suffix in [('double', math, ''), ('float', mathf, 'f')]:
        signatures = [f'public static {typ} {name}({typ} value)' for name in (*functions, 'Round')]
        native = [f'[Native("{name}{suffix}")]' for name in (*functions.values(), 'fmod')]
        private = [f'private static unsafe extern {typ} Native{name}(' for name in functions]
        check(all(text in source for text in signatures + native + private)
              and f'private static unsafe extern {typ} NativeRemainder(' in source,
              f'{typ} source API or private native C bindings missing')
    generated_text = generated.decode('utf-8')
    check(all(f'extern double {native}(' in generated_text for native in (*functions.values(), 'fmod'))
          and all(f'extern float {native}f(' in generated_text for native in (*functions.values(), 'fmod')),
          'strict C ABI must use actual float and double math symbols')
    check('nearbyint' not in math + mathf and '[Native("round' not in math + mathf
          and 'NativeRemainder(whole' in math and 'NativeRemainder(whole' in mathf,
          'midpoint-to-even must be independent of the host floating-point rounding mode')
    check('public static int Min(int a, int b)' in math
          and 'public static long Abs(long value)' in math
          and 'public static double Sqrt(double value)' in math
          and 'public static float Sqrt(float value)' in mathf,
          'previous integer or floating-point math APIs changed')
    for name, expected in NEGATIVES.items():
        verify_negative(name, expected)

    with tempfile.TemporaryDirectory(prefix='void373-rounding-') as scratch:
        folder = Path(scratch)
        gcc = shlex.split(os.environ.get('CC', 'gcc'))
        gcc_binary = folder / ('round-gcc' + ('.exe' if os.name == 'nt' else ''))
        strict_compile(gcc, gcc_binary)
        check(verify_output(gcc_binary) == baseline, 'GCC and voidc results differ')
        gcc_o0 = folder / ('round-gcc-o0' + ('.exe' if os.name == 'nt' else ''))
        strict_compile(gcc, gcc_o0, '-O0')
        check(verify_output(gcc_o0) == baseline, 'GCC -O0 and -O2 results differ')
        clang = shutil.which('clang')
        if clang:
            clang_binary = folder / ('round-clang' + ('.exe' if os.name == 'nt' else ''))
            strict_compile([clang], clang_binary)
            check(verify_output(clang_binary) == baseline, 'Clang and GCC results differ')
            clang_o0 = folder / ('round-clang-o0' + ('.exe' if os.name == 'nt' else ''))
            strict_compile([clang], clang_o0, '-O0')
            check(verify_output(clang_o0) == baseline, 'Clang -O0 and -O2 results differ')
        else:
            for _ in range(6):
                check(True, 'Clang is unavailable; GCC remains authoritative')
        probe = folder / 'probe.c'
        probe.write_text('int main(void) { return 0; }\n', encoding='utf-8')
        supported = invoke(*gcc, '-fsanitize=undefined', probe, '-o', folder / 'probe')
        if supported.returncode == 0:
            binary = folder / ('round-ubsan' + ('.exe' if os.name == 'nt' else ''))
            strict_compile(gcc, binary, '-O2', '-fsanitize=undefined', '-fno-sanitize-recover=undefined')
            check(verify_output(binary) == baseline, 'UBSan result differs from GCC')
        else:
            for _ in range(3):
                check(True, 'UBSan unavailable')
    print(f'# {COUNT} checks')


if __name__ == '__main__':
    main()
