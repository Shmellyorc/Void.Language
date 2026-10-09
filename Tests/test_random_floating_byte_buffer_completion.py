#!/usr/bin/env python3
"""#376: PCG32 float/byte vectors, diagnostics, native C11 and UBSan."""
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
FIXTURE = ROOT / 'Tests/RandomFloatingByteBufferCompletion'
GENERATED = FIXTURE / '.void/RandomFloatingByteBufferCompletion.c'
EXECUTABLE = FIXTURE / 'bin' / ('RandomFloatingByteBufferCompletion' + ('.exe' if os.name == 'nt' else ''))
RUNTIME = sorted((ROOT / 'Runtime/src').glob('*.c'))
EXPECTED = 'True\n' * 34
NEGATIVES = {
    'NextDoubleWithArgument': ('VOID3003', "type 'Random' has no matching instance method 'NextDouble'"),
    'NextSingleWithArgument': ('VOID3003', "type 'Random' has no matching instance method 'NextSingle'"),
    'NextBytesIntegerArray': ('VOID3003', "type 'Random' has no matching instance method 'NextBytes'"),
    'NextBytesString': ('VOID3003', "type 'Random' has no matching instance method 'NextBytes'"),
    'NextBytesTooManyArguments': ('VOID3003', "type 'Random' has no matching instance method 'NextBytes'"),
    'StaticNextDouble': ('VOID3003', "instance method 'Random.NextDouble' requires an instance receiver"),
    'InvalidDoubleReturn': ('VOID3002', "cannot assign 'double' to local 'v' of type 'string'"),
    'InvalidSingleReturn': ('VOID3002', "cannot assign 'float' to local 'v' of type 'string'"),
}
COUNT = 0


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
    source = ROOT / 'Tests/RandomFloatingByteBufferCompletionDiagnostics' / name / 'Program.void'
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
          f'{cc}, {optimization} C: {result.returncode}\n{result.stdout}\n{result.stderr}')


def main() -> None:
    version = invoke(COMPILER, 'version')
    check(version.returncode == 0 and version.stdout.strip() == 'voidc 0.0.381',
          f'version: {version.stdout}\n{version.stderr}')
    project = (FIXTURE / 'RandomFloatingByteBufferCompletion.voidproj').read_text(encoding='utf-8')
    check('"version":"0.0.376"' in project and '"libraries"' not in project,
          'Random float/byte APIs should require no native library or special project configuration')
    built = invoke(COMPILER, 'build', FIXTURE)
    check(built.returncode == 0 and not built.stderr,
          f'build: {built.returncode}\n{built.stdout}\n{built.stderr}')
    baseline = verify_output(EXECUTABLE)
    emitted = GENERATED.read_bytes()
    rebuilt = invoke(COMPILER, 'build', FIXTURE)
    check(rebuilt.returncode == 0 and not rebuilt.stderr and GENERATED.read_bytes() == emitted,
          f'generated C must be deterministic: {rebuilt.stdout}\n{rebuilt.stderr}')

    source = (ROOT / 'StandardLibrary/Void/Random.void').read_text(encoding='utf-8')
    check('public double NextDouble()' in source and 'public float NextSingle()' in source
          and 'public void NextBytes(byte[] buffer)' in source,
          'public NextDouble/NextSingle/NextBytes APIs required')
    check('private uint NextBits()' in source and 'private uint NextBounded(uint bound)' in source
          and 'public int Next()' in source and 'public int Next(int maxExclusive)' in source
          and 'public int Next(int minInclusive, int maxExclusive)' in source,
          'shared original PCG32 generator and integer API must remain intact')
    check('uint high27 = NextBits() >> 5;' in source
          and 'uint low26 = NextBits() >> 6;' in source
          and '(1.0 / 9007199254740992.0)' in source,
          '53-bit NextDouble must use two ordered PCG32 draws')
    check('uint bits24 = NextBits() >> 8;' in source
          and '(1.0f / 16777216.0f)' in source,
          '24-bit NextSingle must avoid widening/double-to-float rounding')
    check('ArgumentNullException("buffer")' in source
          and '(byte)(bits & 255u)' in source and 'bits = bits >> 8;' in source
          and 'while (index < buffer.Length)' in source,
          'byte extraction, empty buffers and null rejection must be explicit')
    check('6364136223846793005ul' in source
          and '1442695040888963407ul' in source
          and 'private ulong _state;' in source and 'static ulong _state' not in source
          and source.count('[Native(') == 1 and 'vc_native_entropy_seed' in source,
          'locked per-instance PCG32 with only constructor-time entropy import must be preserved')
    generated = emitted.decode('utf-8')
    check('6364136223846793005ULL' in generated
          and '1442695040888963407ULL' in generated
          and '9007199254740992.0' in generated and '16777216.0' in generated,
          'generated C must retain native PCG32 and floating-point scale semantics')

    for name, expected in NEGATIVES.items():
        verify_negative(name, expected)

    with tempfile.TemporaryDirectory(prefix='void376-random-') as scratch:
        folder = Path(scratch)
        gcc = shlex.split(os.environ.get('CC', 'gcc'))
        gcc_binary = folder / ('random-gcc' + ('.exe' if os.name == 'nt' else ''))
        strict_compile(gcc, gcc_binary)
        check(verify_output(gcc_binary) == baseline, 'native GCC output must match voidc result')
        gcc_o0 = folder / ('random-gcc-o0' + ('.exe' if os.name == 'nt' else ''))
        strict_compile(gcc, gcc_o0, '-O0')
        check(verify_output(gcc_o0) == baseline, 'GCC -O0 and -O2 results must match')
        clang = shutil.which('clang')
        if clang:
            clang_binary = folder / ('random-clang' + ('.exe' if os.name == 'nt' else ''))
            strict_compile([clang], clang_binary)
            check(verify_output(clang_binary) == baseline, 'Clang and GCC results must match')
            clang_o0 = folder / ('random-clang-o0' + ('.exe' if os.name == 'nt' else ''))
            strict_compile([clang], clang_o0, '-O0')
            check(verify_output(clang_o0) == baseline, 'Clang -O0 and -O2 results must match')
        else:
            print('Clang not installed: native Clang validation skipped')
        probe = folder / 'probe.c'
        probe.write_text('int main(void) { return 0; }\n', encoding='utf-8')
        supported = invoke(*gcc, '-fsanitize=undefined', probe, '-o', folder / 'probe')
        if supported.returncode == 0:
            binary = folder / ('random-ubsan' + ('.exe' if os.name == 'nt' else ''))
            strict_compile(gcc, binary, '-O2', '-fsanitize=undefined', '-fno-sanitize-recover=undefined')
            check(verify_output(binary) == baseline, 'UBSan output must match GCC output')
        else:
            print('GCC UBSan unsupported: sanitizer validation skipped')
    print(f'# {COUNT} checks')


if __name__ == '__main__':
    main()
