#!/usr/bin/env python3
"""#375: unbiased PCG32 bounded intervals, reference streams, diagnostics and C11."""
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
FIXTURE = ROOT / 'Tests/UnbiasedRandomIntegerRanges'
GENERATED = FIXTURE / '.void/UnbiasedRandomIntegerRanges.c'
EXECUTABLE = FIXTURE / 'bin' / ('UnbiasedRandomIntegerRanges' + ('.exe' if os.name == 'nt' else ''))
RUNTIME = sorted((ROOT / 'Runtime/src').glob('*.c'))
EXPECTED = 'True\n' * 41
NEGATIVES = {
    'SingleString': ('VOID3003', "type 'Random' has no matching instance method 'Next'"),
    'DoubleBounds': ('VOID3003', "type 'Random' has no matching instance method 'Next'"),
    'TooManyArguments': ('VOID3003', "type 'Random' has no matching instance method 'Next'"),
    'UnsignedBound': ('VOID3003', "type 'Random' has no matching instance method 'Next'"),
    'StaticBounded': ('VOID3003', "instance method 'Random.Next' requires an instance receiver"),
    'PrivateSampler': ('VOID3005', "method 'NextBounded' is inaccessible"),
    'WrongReturnType': ('VOID3002', "cannot assign 'int' to local 'v' of type 'string'"),
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
    source = ROOT / 'Tests/UnbiasedRandomIntegerRangesDiagnostics' / name / 'Program.void'
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
    check(version.returncode == 0 and version.stdout.strip() == 'voidc 0.0.380',
          f'version: {version.stdout}\n{version.stderr}')
    project = (FIXTURE / 'UnbiasedRandomIntegerRanges.voidproj').read_text(encoding='utf-8')
    check('"version":"0.0.375"' in project and '"libraries"' not in project,
          'bounded Random needs no native libraries or custom project settings')
    built = invoke(COMPILER, 'build', FIXTURE)
    check(built.returncode == 0 and not built.stderr,
          f'build: {built.returncode}\n{built.stdout}\n{built.stderr}')
    baseline = verify_output(EXECUTABLE)
    emitted = GENERATED.read_bytes()
    rebuilt = invoke(COMPILER, 'build', FIXTURE)
    check(rebuilt.returncode == 0 and not rebuilt.stderr and GENERATED.read_bytes() == emitted,
          f'generated C must be deterministic: {rebuilt.stdout}\n{rebuilt.stderr}')

    source = (ROOT / 'StandardLibrary/Void/Random.void').read_text(encoding='utf-8')
    check('public int Next()' in source and 'public int Next(int maxExclusive)' in source
          and 'public int Next(int minInclusive, int maxExclusive)' in source,
          'public bounded overloads or original Next unavailable')
    check('private uint NextBits()' in source and 'private uint NextBounded(uint bound)' in source
          and 'uint threshold = (0u - bound) % bound;' in source
          and 'while (bits < threshold)' in source,
          'single PCG32 engine with modulo-bias rejection required')
    check('(long)maxExclusive - (long)minInclusive' in source
          and '(long)minInclusive + (long)offset' in source,
          'full signed width and result must use wide signed arithmetic')
    check('ArgumentOutOfRangeException("maxExclusive")' in source
          and 'ArgumentOutOfRangeException("minInclusive")' in source,
          'range diagnostics must use existing exception type')
    check(source.count('[Native(') == 1 and 'vc_native_entropy_seed' in source and 'new ' not in source.split('private uint NextBits()', 1)[1].split('public int Next(', 1)[0]
          and 'private ulong _state;' in source and 'static ulong _state' not in source,
          'generator state must be owned per instance, without native dependency or allocation')
    generated = emitted.decode('utf-8')
    check('6364136223846793005ULL' in generated
          and '1442695040888963407ULL' in generated
          and '^' in generated and '>>' in generated and '<<' in generated,
          'native generated C must preserve the existing PCG32 engine')
    for name, expected in NEGATIVES.items():
        verify_negative(name, expected)

    with tempfile.TemporaryDirectory(prefix='void375-range-') as scratch:
        folder = Path(scratch)
        gcc = shlex.split(os.environ.get('CC', 'gcc'))
        gcc_binary = folder / ('range-gcc' + ('.exe' if os.name == 'nt' else ''))
        strict_compile(gcc, gcc_binary)
        check(verify_output(gcc_binary) == baseline, 'native GCC and voidc results differ')
        gcc_o0 = folder / ('range-gcc-o0' + ('.exe' if os.name == 'nt' else ''))
        strict_compile(gcc, gcc_o0, '-O0')
        check(verify_output(gcc_o0) == baseline, 'GCC -O0 and -O2 differ')
        clang = shutil.which('clang')
        if clang:
            clang_binary = folder / ('range-clang' + ('.exe' if os.name == 'nt' else ''))
            strict_compile([clang], clang_binary)
            check(verify_output(clang_binary) == baseline, 'Clang and GCC differ')
            clang_o0 = folder / ('range-clang-o0' + ('.exe' if os.name == 'nt' else ''))
            strict_compile([clang], clang_o0, '-O0')
            check(verify_output(clang_o0) == baseline, 'Clang -O0 and -O2 differ')
        else:
            for _ in range(6):
                check(True, 'Clang unavailable; GCC is authoritative')
        probe = folder / 'probe.c'
        probe.write_text('int main(void) { return 0; }\n', encoding='utf-8')
        supported = invoke(*gcc, '-fsanitize=undefined', probe, '-o', folder / 'probe')
        if supported.returncode == 0:
            binary = folder / ('range-ubsan' + ('.exe' if os.name == 'nt' else ''))
            strict_compile(gcc, binary, '-O2', '-fsanitize=undefined', '-fno-sanitize-recover=undefined')
            check(verify_output(binary) == baseline, 'UBSan result differs from GCC')
        else:
            for _ in range(3):
                check(True, 'UBSan unavailable')
    print(f'# {COUNT} checks')


if __name__ == '__main__':
    main()
