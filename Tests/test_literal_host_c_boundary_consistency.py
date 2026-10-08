#!/usr/bin/env python3
"""#357: lexical literal identity, source diagnostics, and ISO C11 emission."""
from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
VOIDC = ROOT / 'bin' / ('voidc.exe' if os.name == 'nt' else 'voidc')
NAME = 'LiteralHostCBoundaryConsistency'
FIXTURE = ROOT / 'Tests' / NAME
GENERATED = FIXTURE / '.void' / (NAME + '.c')
RUNTIME = sorted((ROOT / 'Runtime/src').glob('*.c'))
TRUE_LINES = 31
COUNT = 0


def expect(condition: bool, explanation: str = 'check failed') -> None:
    global COUNT
    if not condition:
        raise AssertionError(explanation)
    COUNT += 1
    print('True')


def invoke(*command: object, timeout: int = 180) -> subprocess.CompletedProcess[str]:
    return subprocess.run([str(c) for c in command], cwd=ROOT,
                          capture_output=True, text=True, timeout=timeout)


def check_failure(directory: Path, expression: str, expected: str) -> None:
    directory.mkdir()
    (directory / 'Program.void').write_text('using Void;\npublic static class Program {\n'
        'public static void Main() { Console.WriteLine(' + expression + '); }\n}\n', encoding='utf-8')
    (directory / 'Negative.voidproj').write_text(
        '{"format":1,"name":"Negative","output":"exe","version":"0.0.357"}\n',
        encoding='utf-8')
    result = invoke(VOIDC, 'check', directory)
    expect(result.returncode != 0 and expected in result.stderr and
           'code: VOID' in result.stderr and 'Program.void:' in result.stderr,
           f'{expression!r} expected {expected!r}, got {result.stdout}\n{result.stderr}')


def main() -> None:
    result = invoke(VOIDC, 'version')
    expect(result.returncode == 0 and result.stdout.strip() == 'voidc 0.0.360')
    built = invoke(VOIDC, 'build', FIXTURE)
    expect(built.returncode == 0, built.stdout + built.stderr)
    c_source = GENERATED.read_bytes()
    executable = FIXTURE / 'bin' / (NAME + ('.exe' if os.name == 'nt' else ''))
    original = invoke(executable)
    expected = 'True\n' * TRUE_LINES
    expect(original.returncode == 0 and original.stdout == expected and not original.stderr,
           original.stdout + original.stderr)
    expect(b'\\303\\251' in c_source and b'\\360\\237\\230\\200' in c_source,
           'UTF-8 bytes must be escaped as fixed-width C octal bytes')
    expect(b'\\000' in c_source and b'vc_string_literal(' in c_source,
           'embedded NUL must retain explicit length')
    expect(b'0b1010' not in c_source and b'3735928559U' in c_source,
           'ISO C11 numeric spelling must not depend on host binary/hex suffix parsing')
    expect(b'vc_string_literal("\\303\\251", 2u)' in c_source,
           'escaped/direct unicode string must preserve encoded byte length')
    expect(invoke(VOIDC, 'build', FIXTURE).returncode == 0 and
           GENERATED.read_bytes() == c_source, 'generated C is nondeterministic')

    with tempfile.TemporaryDirectory(prefix='void357-literal-') as dirname:
        directory = Path(dirname)
        c_compiler = os.environ.get('CC', 'cc')
        for compiler in (c_compiler, 'clang'):
            if not shutil.which(compiler):
                continue
            for optimization in ('-O0', '-O2'):
                binary = directory / ('literal-' + compiler + optimization +
                    ('.exe' if os.name == 'nt' else ''))
                command = [compiler, '-std=c11', '-Wall', '-Wextra', '-Wpedantic',
                           '-Werror', optimization, '-I' + str(ROOT / 'Runtime/include')]
                if os.name != 'nt':
                    command.append('-pthread')
                command += [str(GENERATED), *map(str, RUNTIME)]
                if os.name != 'nt':
                    command.append('-lm')
                command += ['-o', str(binary)]
                compiled = invoke(*command)
                if compiler == c_compiler:
                    expect(compiled.returncode == 0 and not compiled.stderr,
                           f'{compiler} {optimization}: {compiled.stderr}')
                elif compiled.returncode != 0 or compiled.stderr:
                    raise AssertionError(f'{compiler} {optimization}: {compiled.stderr}')
                execution = invoke(binary)
                if compiler == c_compiler:
                    expect(execution.returncode == 0 and execution.stdout == expected and
                           not execution.stderr, execution.stdout + execution.stderr)
                elif execution.returncode != 0 or execution.stdout != expected or execution.stderr:
                    raise AssertionError(f'{compiler} {optimization}: {execution.stdout}{execution.stderr}')

        negatives = (
            (r'"\e"', 'string literal'),
            (r"'\e'", 'character literal'),
            (r'"\uD800"', 'string literal'),
            (r"'\uD800'", 'character literal'),
            (r'"\U00110000"', 'string literal'),
            (r"'\U00110000'", 'character literal'),
            (r'"\x"', 'string literal'),
            (r'"\u123"', 'string literal'),
            (r'"\777"', 'string literal'),
            ('0x', 'numeric literal'),
            ('0b102', 'numeric literal'),
            ('0xGG', 'numeric literal'),
            ('1__2', 'numeric literal'),
            ('0x_FF', 'numeric literal'),
            ('1ff', 'numeric literal'),
            ('1e+', 'numeric literal'),
            ('2147483648', 'numeric literal'),
            ('4294967296u', 'numeric literal'),
            ('9223372036854775808L', 'numeric literal'),
            ('18446744073709551616ul', 'numeric literal'),
            ('1e9999', 'numeric literal'),
            ('3.5e39f', 'numeric literal'),
            ('1e9999m', 'numeric literal'),
        )
        for i, (expression, diagnostic) in enumerate(negatives):
            check_failure(directory / f'negative-{i:02}', expression, diagnostic)
    print(f'# {COUNT} checks')


if __name__ == '__main__':
    main()
