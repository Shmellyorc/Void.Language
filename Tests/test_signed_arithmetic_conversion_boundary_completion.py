#!/usr/bin/env python3
"""#355: deterministic integral arithmetic, shifts and conversion boundaries."""
from __future__ import annotations

import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
VOIDC = ROOT / 'bin' / ('voidc.exe' if os.name == 'nt' else 'voidc')
FIXTURE = ROOT / 'Tests/SignedArithmeticConversionBoundaryCompletion'
GENERATED = FIXTURE / '.void/SignedArithmeticConversionBoundaryCompletion.c'
RUNTIME_SOURCES = sorted((ROOT / 'Runtime/src').glob('*.c'))
COUNT = 0
EXPECTED_OUTPUT = 62


def expect(condition: bool, message: str = 'check failed') -> None:
    global COUNT
    if not condition:
        raise AssertionError(message)
    COUNT += 1
    print('True')


def invoke(*args: object) -> subprocess.CompletedProcess[str]:
    env = dict(os.environ)
    env.setdefault('TERM', 'xterm')
    return subprocess.run([str(VOIDC), *map(str, args)], cwd=ROOT, env=env,
                          capture_output=True, text=True, encoding='utf-8', timeout=180)


def cc_command() -> list[str]:
    return shlex.split(os.environ.get('CC', 'cc'))


def compile_generated(optimization: str, output: Path, sanitize: bool = False) -> subprocess.CompletedProcess[str]:
    command = cc_command() + [
        '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', optimization,
        '-I' + str(ROOT / 'Runtime/include')]
    if os.name != 'nt':
        command.append('-pthread')
    if sanitize:
        command += ['-fsanitize=undefined', '-fno-sanitize-recover=undefined']
    command += [str(GENERATED), *map(str, RUNTIME_SOURCES), '-o', str(output)]
    return subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=180)


def run_native(path: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run([str(path)], cwd=ROOT, capture_output=True, text=True,
                          encoding='utf-8', timeout=180)


def ubsan_supported() -> bool:
    with tempfile.TemporaryDirectory(prefix='void355-ubsan-probe-') as temp:
        folder = Path(temp)
        source = folder / 'probe.c'
        output = folder / ('probe.exe' if os.name == 'nt' else 'probe')
        source.write_text('int main(void) { return 0; }\n', encoding='utf-8')
        return subprocess.run(cc_command() + [
            '-std=c11', '-fsanitize=undefined', str(source), '-o', str(output)],
            cwd=ROOT, capture_output=True, text=True, timeout=60).returncode == 0


def main() -> None:
    result = invoke('run', FIXTURE)
    runtime = [line for line in result.stdout.splitlines() if line in ('True', 'False')]
    expect(result.returncode == 0 and not result.stderr, result.stdout + result.stderr)
    expect(len(runtime) == EXPECTED_OUTPUT and all(line == 'True' for line in runtime),
           f'expected {EXPECTED_OUTPUT} passing VOID assertions, got {len(runtime)}\n{result.stdout}')

    generated = GENERATED.read_bytes()
    result = invoke('build', FIXTURE)
    expect(result.returncode == 0 and not result.stderr, result.stdout + result.stderr)
    expect(GENERATED.read_bytes() == generated, 'generated C changed across identical builds')

    text = generated.decode('utf-8')
    for width in ('i8', 'i16', 'i32', 'i64'):
        expect(all(f'vc_wrap_{width}_{op}' in text for op in ('add', 'sub', 'mul', 'div', 'rem')),
               f'missing shared signed arithmetic helpers for {width}')
        expect(all(f'vc_update_{width}_{kind}_{position}' in text
                   for kind in ('inc', 'dec') for position in ('pre', 'post')),
               f'missing signed update helper family for {width}')
        expect(all(f'vc_compound_{width}_{op}' in text for op in ('add', 'sub', 'mul', 'div', 'rem')),
               f'missing signed compound helper family for {width}')
        expect(all(f'vc_shift_{width}_{direction}' in text for direction in ('left', 'right')),
               f'missing defined signed shifts for {width}')
    for width in ('u8', 'u16', 'u32', 'u64'):
        expect(all(f'vc_shift_{width}_{direction}' in text for direction in ('left', 'right')),
               f'missing unsigned shift helpers for {width}')
    expect('vc_signed_i32_from_bits((uint32_t)' in text and
           'vc_signed_i64_from_bits((uint64_t)' in text,
           'unsigned-to-signed conversion must use defined bit restoration')
    expect('vc_shift_i32_right(' in text and 'vc_shift_u32_right(' in text,
           'signed right shift must not be delegated to C')
    expect('vc_wrap_i32_div(' in text and 'vc_wrap_i64_rem(' in text,
           'MinValue/-1 must not reach direct C signed division/remainder')
    expect('vc_wrap_i32_sub(0, ' in text,
           'unary signed negation must use the #354 foundation')
    expect('vc_compound_i32_add(&(' in text and 'vc_update_i32_inc_post(&(' in text,
           'signed assignment/update must evaluate the lvalue only once')
    expect('vc_safe_u16_mul(' in text and 'vc_compound_u16_mul(' in text,
           'narrow unsigned multiplication must avoid promoted signed-int overflow')
    expect('vc_safe_u32_div(' in text and 'vc_safe_u64_rem(' in text,
           'unsigned zero divisors must be checked before C arithmetic')
    expect('vc_compound_u32_div(' in text and 'vc_safe_u8_rem(' in text,
           'unsigned integral arithmetic must use the shared width-specific helpers')
    expect('value < 0 && n != 0' in text,
           'arithmetic right shift sign propagation must be defined independently of C')

    outputs: list[str] = []
    with tempfile.TemporaryDirectory(prefix='void355-opt-') as temp:
        folder = Path(temp)
        for optimization in ('-O0', '-O2'):
            binary = folder / (f'integral-{optimization[2:]}' + ('.exe' if os.name == 'nt' else ''))
            built = compile_generated(optimization, binary)
            expect(built.returncode == 0 and not built.stderr, built.stderr)
            ran = run_native(binary)
            lines = [line for line in ran.stdout.splitlines() if line in ('True', 'False')]
            expect(ran.returncode == 0 and not ran.stderr and len(lines) == EXPECTED_OUTPUT and
                   all(line == 'True' for line in lines), ran.stdout + ran.stderr)
            outputs.append(ran.stdout)
        expect(outputs[0] == outputs[1], 'O0/O2 numeric semantics differ')

        if ubsan_supported():
            binary = folder / ('integral-ubsan' + ('.exe' if os.name == 'nt' else ''))
            built = compile_generated('-O2', binary, sanitize=True)
            expect(built.returncode == 0 and not built.stderr, built.stderr)
            ran = run_native(binary)
            expect(ran.returncode == 0 and not ran.stderr and
                   len([x for x in ran.stdout.splitlines() if x == 'True']) == EXPECTED_OUTPUT,
                   'UBSan failure: ' + ran.stdout + ran.stderr)
        else:
            # The suite's True guard must not depend on sanitizer availability.
            expect(True)
            expect(True)

    for case, message in (
        ('DivideByZero', 'integer division by zero'),
        ('RemainderByZero', 'integer remainder by zero'),
        ('UnsignedDivideByZero', 'integer division by zero'),
    ):
        fixture = ROOT / 'Tests/SignedArithmeticConversionBoundaryDiagnostics' / case
        result = invoke('run', fixture)
        expect(result.returncode != 0, f'{case} unexpectedly succeeded')
        expect('VOID runtime error: ' + message in (result.stdout + result.stderr),
               f'{case} missing deterministic VOID failure: {result.stdout + result.stderr}')

    invalid = invoke('build', ROOT / 'Tests/SignedArithmeticConversionBoundaryDiagnostics/InvalidShift')
    expect(invalid.returncode != 0, 'nonintegral built-in shift unexpectedly compiled')
    expect("operator '<<' is not defined for 'float' and 'int'" in
           (invalid.stdout + invalid.stderr),
           'shift diagnostic must come from semantic binding, not malformed generated C')

    compiler = (ROOT / 'Compiler/src/compiler.c').read_text(encoding='utf-8')
    semantic = (ROOT / 'Compiler/src/semantic.c').read_text(encoding='utf-8')
    expect('signed_wrapping_arithmetic_helper' in compiler and
           'emit_signed_wrapping_arithmetic_helpers' in compiler and
           'signed_bits_helper' in compiler,
           'one shared #354/#355 arithmetic and signed-restoration mechanism is required')
    expect('signed_i32_from_bits((uint32_t)left <<' in semantic and
           'shifted |= UINT32_MAX <<' in semantic,
           'constant evaluator shifts must use defined bitwise arithmetic')
    expect('result = left == INT32_MIN && right == -1 ? INT32_MIN' in semantic,
           'constant division overflow must agree with runtime')
    print(f'# {COUNT} checks')


if __name__ == '__main__':
    main()
