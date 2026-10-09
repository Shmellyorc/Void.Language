#!/usr/bin/env python3
"""#377: OS entropy, injected zero/high-bit/failure vectors, safety, native C11."""
from __future__ import annotations
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BIN = ROOT / 'bin' / ('voidc.exe' if os.name == 'nt' else 'voidc')
FIX = ROOT / 'Tests/PortableAutomaticRandomSeeding'
MOCK = ROOT / 'Tests/PortableAutomaticRandomSeedingMock'
FAIL = ROOT / 'Tests/PortableAutomaticRandomSeedingFailure'
STUB = FIX / 'native/mock_entropy.c'
RUNTIME = sorted((ROOT / 'Runtime/src').glob('*.c'))
SANS_ENTROPY = [p for p in RUNTIME if p.name != 'vc_entropy.c']
FLAGS = ['-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2',
         '-I' + str(ROOT / 'Runtime/include')]
if os.name != 'nt':
    FLAGS += ['-pthread']
CC = shlex.split(os.environ.get('CC', 'gcc'))
N = 0


def check(ok: bool, description: str) -> None:
    global N
    if not ok:
        raise AssertionError(description)
    N += 1
    print('True', flush=True)


def run(*args: object, timeout: int = 180) -> subprocess.CompletedProcess[str]:
    return subprocess.run(list(map(str, args)), cwd=ROOT, capture_output=True,
                          text=True, encoding='utf-8', timeout=timeout)


def compile_c(cc: list[str], source: Path, destination: Path, *, mock: bool = False,
              extra: tuple[str, ...] = (), opt: str = '-O2') -> None:
    srcs = SANS_ENTROPY + [STUB] if mock else RUNTIME
    flags = [f for f in FLAGS if f != '-O2'] + [opt]
    if os.name == 'nt':
        link = ['-lbcrypt', '-lm']
    else:
        link = ['-lm']
    p = run(*cc, *flags, *extra, source, *srcs, *link, '-o', destination, timeout=240)
    check(p.returncode == 0 and not p.stderr, f'C compile {cc}: {p.stderr}\n{p.stdout}')


def bits_for_seed(seed: int):
    mask = (1 << 64) - 1
    state = 0
    def word():
        nonlocal state
        old = state
        state = (old * 6364136223846793005 + 1442695040888963407) & mask
        shift = ((old >> 18) ^ old) >> 27
        shift &= (1 << 32) - 1
        rotation = old >> 59
        return ((shift >> rotation) | (shift << ((-rotation) & 31))) & 0xffffffff
    word()
    state = (state + seed) & mask
    word()
    def bounded(bound):
        threshold = ((-bound) & 0xffffffff) % bound
        n = word()
        while n < threshold:
            n = word()
        return n % bound
    first = bounded(2147483647)
    second = bounded(2147483647)
    third = bounded(1000)
    fourth = -500 + bounded(1000)
    single = word() >> 8
    raw = [word(), word()]
    byts = [(raw[i // 4] >> (8 * (i % 4))) & 255 for i in range(5)]
    return [first, second, third, fourth, single, *byts]


def neg(name: str, code: str, message: str) -> None:
    path = ROOT / 'Tests/PortableAutomaticRandomSeedingDiagnostics' / name / 'Program.void'
    checked_path = path.parent if name == 'UserNativeOutRequiresUnsafe' else path
    p = run(BIN, 'check', checked_path, '--diagnostics=json')
    diags = [json.loads(x) for x in p.stderr.splitlines() if x.startswith('{') and 'schemaVersion' in x]
    valid = p.returncode != 0 and len(diags) == 1 and diags[0]['code'] == code and diags[0]['message'] == message
    if valid:
        span = diags[0]['span']
        valid = 0 <= span['start']['offset'] < span['end']['offset'] <= len(path.read_text())
    check(valid, f'{name}: {p.stdout}\n{p.stderr}')


def main():
    version = run(BIN, 'version')
    check(version.returncode == 0 and version.stdout.strip() == 'voidc 0.0.380', 'compiler version')
    project = (FIX / 'PortableAutomaticRandomSeeding.voidproj').read_text()
    check('"version":"0.0.377"' in project and '"libraries"' not in project, 'no project deps')
    src = (ROOT / 'StandardLibrary/Void/Random.void').read_text()
    check('public Random()' in src and 'public Random(int seed)' in src
          and 'Initialize((ulong)(uint)seed);' in src and 'private void Initialize(ulong seed)' in src,
          'explicit seed mapping and reusable initialization')
    check(src.count('[Native(') == 1 and 'vc_native_entropy_seed' in src
          and 'InvalidOperationException("Operating-system entropy is unavailable.")' in src,
          'one native entropy boundary and explicit failure')
    check('private uint NextBits()' in src and 'private uint NextBounded(uint bound)' in src
          and 'public double NextDouble()' in src and 'public float NextSingle()' in src
          and 'public void NextBytes(byte[] buffer)' in src, 'locked PCG and public APIs')
    native = (ROOT / 'Runtime/src/vc_entropy.c').read_text()
    check('getrandom(' in native and 'BCryptGenRandom(' in native and 'arc4random_buf(' in native
          and 'time(' not in native and 'rand(' not in native, 'platform OS sources only')
    check('-lbcrypt' in (ROOT / 'Compiler/src/compiler.c').read_text(), 'Windows linker integration')

    built = run(BIN, 'build', FIX)
    check(built.returncode == 0 and not built.stderr, f'build: {built.stdout}\n{built.stderr}')
    generated = FIX / '.void/PortableAutomaticRandomSeeding.c'
    baseline = generated.read_bytes()
    exe = FIX / 'bin' / ('PortableAutomaticRandomSeeding' + ('.exe' if os.name == 'nt' else ''))
    p = run(exe)
    check(p.returncode == 0 and not p.stderr and p.stdout == 'True\n' * 13, f'auto runtime: {p.stdout}\n{p.stderr}')
    rebuilt = run(BIN, 'build', FIX)
    check(rebuilt.returncode == 0 and not rebuilt.stderr and generated.read_bytes() == baseline,
          'generated C reproducibility')
    check('vc_native_entropy_seed' in baseline.decode() and 'vc_native_call' in baseline.decode(),
          'generated native call present')

    for name, code, message in [
        ('WrongSeedType', 'VOID3000', "no matching constructor for 'Random' was found"),
        ('TooManyConstructors', 'VOID3000', "no matching constructor for 'Random' was found"),
        ('PrivateEntropyImport', 'VOID3003', "type 'Random' has no matching static method 'NativeEntropySeed'"),
        ('UserNativeOutRequiresUnsafe', 'VOID3000', "unsafe method 'Read' requires an unsafe method"),
    ]:
        neg(name, code, message)

    mock_built = run(BIN, 'build', MOCK / 'Program.void')
    check(mock_built.returncode == 0 and not mock_built.stderr, f'mock source: {mock_built.stderr}')
    fail_built = run(BIN, 'build', FAIL / 'Program.void')
    check(fail_built.returncode == 0 and not fail_built.stderr, f'failure source: {fail_built.stderr}')
    with tempfile.TemporaryDirectory(prefix='void377-') as d:
        folder = Path(d)
        gccexe = folder / 'auto-gcc'
        compile_c(CC, generated, gccexe)
        p = run(gccexe)
        check(p.returncode == 0 and p.stdout == 'True\n' * 13 and not p.stderr, 'strict GCC execution')
        c0 = folder / 'auto-gcc-o0'
        compile_c(CC, generated, c0, opt='-O0')
        p = run(c0)
        check(p.returncode == 0 and p.stdout == 'True\n' * 13 and not p.stderr, 'GCC -O0')
        clang = shutil.which('clang')
        if clang:
            cl = folder / 'auto-clang'
            compile_c([clang], generated, cl)
            p = run(cl)
            check(p.returncode == 0 and p.stdout == 'True\n' * 13 and not p.stderr, 'Clang strict C11')
        else:
            print('Clang unavailable (not counted)')
        gcc_probe = run(*CC, '-fsanitize=undefined', '-I' + str(ROOT / 'Runtime/include'), '-x', 'c', '-o', folder / 'probe',
                        '-c', str(ROOT / 'Runtime/src/vc_entropy.c'))
        if gcc_probe.returncode == 0:
            sanitized = folder / 'auto-ubsan'
            compile_c(CC, generated, sanitized, extra=('-fsanitize=undefined', '-fno-sanitize-recover=undefined'), opt='-O0')
            p = run(sanitized)
            check(p.returncode == 0 and p.stdout == 'True\n' * 13 and not p.stderr, 'UBSan')
        else:
            print('UBSan unavailable (not counted)')
        mock_c = MOCK / '.void/Program.c'
        for seed in (0, 0x123456789abcdef0, 0xffffffffffffffff):
            output = folder / f'mock-{seed}'
            compile_c(CC, mock_c, output, mock=True,
                      extra=(f'-DTEST_ENTROPY_VALUE=UINT64_C(0x{seed:016x})',), opt='-O0')
            p = run(output)
            expected = '\n'.join(map(str, bits_for_seed(seed))) + '\n'
            check(p.returncode == 0 and p.stdout == expected and not p.stderr,
                  f'fixed seed {seed:x}: {p.stdout!r} != {expected!r}, {p.stderr}')
        fails = folder / 'fail'
        compile_c(CC, FAIL / '.void/Program.c', fails, mock=True, extra=('-DTEST_ENTROPY_FAIL',), opt='-O0')
        p = run(fails)
        check(p.returncode == 0 and p.stdout == 'True\n' and not p.stderr,
              f'entropy failure must throw InvalidOperationException: {p.stdout}\n{p.stderr}')
        if os.name != 'nt' and os.uname().sysname == 'Linux':
            probe = folder / 'linux-backend-test'
            p = run(*CC, *FLAGS, FIX / 'native/linux_entropy_test.c', '-o', probe)
            check(p.returncode == 0 and not p.stderr, f'Linux backend probe: {p.stderr}')
            p = run(probe)
            check(p.returncode == 0 and p.stdout == 'True\n' and not p.stderr,
                  f'Linux short/EINTR/error paths: {p.stdout}\n{p.stderr}')
    print(f'# {N} checks')


if __name__ == '__main__':
    main()
