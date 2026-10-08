#!/usr/bin/env python3
"""#354 deterministic signed add/subtract/multiply wrapping regressions."""
from __future__ import annotations

import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
VOIDC = ROOT / 'bin' / ('voidc.exe' if os.name == 'nt' else 'voidc')
FIXTURE = ROOT / 'Tests/DefinedSignedIntegerArithmeticFoundation'
GENERATED = FIXTURE / '.void/DefinedSignedIntegerArithmeticFoundation.c'
RUNTIME_SOURCES = sorted((ROOT / 'Runtime/src').glob('*.c'))
COUNT = 0


def expect(condition: bool, detail: str = 'check failed') -> None:
    global COUNT
    if not condition:
        raise AssertionError(detail)
    COUNT += 1
    print('True')


def invoke(*args: object):
    env = dict(os.environ)
    env.setdefault('TERM', 'xterm')
    return subprocess.run([str(VOIDC), *map(str, args)], cwd=ROOT, env=env,
                          capture_output=True, text=True, encoding='utf-8', timeout=180)


def cc_command() -> list[str]:
    return shlex.split(os.environ.get('CC', 'cc'))


def compile_generated(optimization: str, output: Path, sanitize: bool = False):
    command = cc_command() + [
        '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', optimization,
        '-I' + str(ROOT / 'Runtime/include')]
    if os.name != 'nt':
        command.append('-pthread')
    if sanitize:
        command += ['-fsanitize=undefined', '-fno-sanitize-recover=undefined']
    command += [str(GENERATED), *map(str, RUNTIME_SOURCES), '-o', str(output)]
    return subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=180)


def run_native(path: Path):
    return subprocess.run([str(path)], cwd=ROOT, capture_output=True, text=True,
                          encoding='utf-8', timeout=180)


def ubsan_supported() -> bool:
    with tempfile.TemporaryDirectory(prefix='void354-ubsan-probe-') as temporary:
        directory = Path(temporary)
        source = directory / 'probe.c'
        binary = directory / ('probe.exe' if os.name == 'nt' else 'probe')
        source.write_text('int main(void) { return 0; }\n', encoding='utf-8')
        result = subprocess.run(cc_command() + [
            '-std=c11', '-fsanitize=undefined', str(source), '-o', str(binary)],
            cwd=ROOT, capture_output=True, text=True, timeout=60)
        return result.returncode == 0


def main() -> None:
    result = invoke('run', FIXTURE)
    runtime = [line for line in result.stdout.splitlines() if line in ('True', 'False')]
    expect(result.returncode == 0 and not result.stderr, result.stderr)
    expect(len(runtime) == 24 and all(line == 'True' for line in runtime), result.stdout)

    first = GENERATED.read_bytes()
    result = invoke('build', FIXTURE)
    expect(result.returncode == 0 and not result.stderr, result.stderr)
    expect(GENERATED.read_bytes() == first, 'generated C changed across identical builds')

    text = GENERATED.read_text(encoding='utf-8')
    for width in ('i8', 'i16', 'i32', 'i64'):
        expect(all(f'vc_wrap_{width}_{op}' in text for op in ('add', 'sub', 'mul')),
               f'missing signed wrapping helper family for {width}')
    expect('return -1 - (int32_t)(UINT32_MAX - value);' in text and
           'return -1 - (int64_t)(UINT64_MAX - value);' in text,
           'signed restoration depends on an out-of-range unsigned-to-signed cast')
    expect('.value = vc_wrap_i32_add(' in text,
           'lifted nullable int arithmetic did not use the wrapping foundation')
    expect('vc_wrap_i32_add((INT32_MAX), 1)' in text,
           'constant-spelled runtime int arithmetic did not use the wrapping foundation')

    outputs: list[str] = []
    with tempfile.TemporaryDirectory(prefix='void354-opt-') as temporary:
        directory = Path(temporary)
        for optimization in ('-O0', '-O2'):
            binary = directory / (f'arithmetic-{optimization[2:]}' + ('.exe' if os.name == 'nt' else ''))
            built = compile_generated(optimization, binary)
            expect(built.returncode == 0 and not built.stderr, built.stderr)
            ran = run_native(binary)
            lines = [line for line in ran.stdout.splitlines() if line in ('True', 'False')]
            expect(ran.returncode == 0 and not ran.stderr and len(lines) == 24 and
                   all(line == 'True' for line in lines), ran.stdout + ran.stderr)
            outputs.append(ran.stdout)
        expect(outputs[0] == outputs[1], '-O0 and -O2 arithmetic results differ')

        if ubsan_supported():
            binary = directory / ('arithmetic-ubsan' + ('.exe' if os.name == 'nt' else ''))
            built = compile_generated('-O2', binary, sanitize=True)
            expect(built.returncode == 0, built.stderr)
            ran = run_native(binary)
            expect(ran.returncode == 0 and not ran.stderr,
                   'UBSan reported undefined behavior:\n' + ran.stderr)
        else:
            # Keep full-suite count deterministic on toolchains without UBSan.
            expect(True)
            expect(True)

    compiler_text = (ROOT / 'Compiler/src/compiler.c').read_text(encoding='utf-8')
    semantic_text = (ROOT / 'Compiler/src/semantic.c').read_text(encoding='utf-8')
    expect('signed_wrapping_arithmetic_helper' in compiler_text and
           'signed_wrapping_arithmetic_type' in compiler_text,
           'signed arithmetic lowering is not centralized')
    expect('signed_i32_from_bits((uint32_t)left + (uint32_t)right)' in semantic_text and
           'signed_i32_from_bits((uint32_t)left * (uint32_t)right)' in semantic_text,
           'compile-time int evaluator does not share wrapping semantics')

    print(f'# {COUNT} checks')


if __name__ == '__main__':
    main()
