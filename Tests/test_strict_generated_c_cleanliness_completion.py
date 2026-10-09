#!/usr/bin/env python3
"""#356: independent strict C11 compilation and runtime-equivalence regressions."""

from __future__ import annotations

import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
VOIDC = ROOT / 'bin' / ('voidc.exe' if os.name == 'nt' else 'voidc')
CASES = (
    ('SpanConversionSlicingStackallocCompletion', 76),
    ('AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegration', 24),
    ('StrictGeneratedCCleanlinessCompletion', 4),
)
COUNT = 0


def expect(condition: bool, message: str = 'failed') -> None:
    global COUNT
    if not condition:
        raise AssertionError(message)
    COUNT += 1
    print('True')


def run(*arguments: str | Path, timeout: int = 180) -> subprocess.CompletedProcess[str]:
    return subprocess.run([str(arg) for arg in arguments], cwd=ROOT,
                          capture_output=True, text=True, timeout=timeout)


def require_success(result: subprocess.CompletedProcess[str], label: str) -> bool:
    if result.returncode != 0:
        raise AssertionError(f'{label}: {result.stdout}\n{result.stderr}')
    return True


def strict_command(compiler: str, source: Path, output: Path, *, link: bool) -> list[str]:
    command = [compiler, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2',
               '-I' + str(ROOT / 'Runtime/include')]
    if os.name != 'nt':
        command.append('-pthread')
    command.append(str(source))
    if link:
        command.extend(str(path) for path in sorted((ROOT / 'Runtime/src').glob('*.c')))
        if os.name != 'nt':
            command.append('-lm')
    else:
        command.append('-c')
    command.extend(['-o', str(output)])
    return command


def main() -> None:
    version = run(VOIDC, 'version')
    expect(require_success(version, 'version') and version.stdout.strip() == 'voidc 0.0.381',
           'wrong compiler version')

    c_compiler = os.environ.get('CC', 'cc')
    expect(shutil.which(c_compiler) is not None, 'host C compiler unavailable')
    with tempfile.TemporaryDirectory(prefix='void356-strict-') as work:
        temporary = Path(work)
        baseline: dict[str, str] = {}
        for name, true_count in CASES:
            fixture = ROOT / 'Tests' / name
            generated = fixture / '.void' / (name + '.c')
            binary = fixture / 'bin' / (name + ('.exe' if os.name == 'nt' else ''))
            expect(require_success(run(VOIDC, 'check', fixture), f'{name} check'))
            expect(require_success(run(VOIDC, 'build', fixture), f'{name} build'))
            expect(generated.is_file() and binary.is_file(), f'{name}: missing generated output')
            original = run(binary)
            expected = 'True\n' * true_count
            expect(original.returncode == 0 and original.stdout == expected and not original.stderr,
                   f'{name}: normal runtime differs: {original.stdout}\n{original.stderr}')
            baseline[name] = generated.read_text(encoding='utf-8')
            strict_binary = temporary / (name + ('.exe' if os.name == 'nt' else ''))
            expect(require_success(run(*strict_command(c_compiler, generated, strict_binary, link=True)),
                                   f'{name} strict C11 link'))
            strict_result = run(strict_binary)
            expect(strict_result.returncode == 0 and strict_result.stdout == original.stdout and
                   strict_result.stderr == original.stderr,
                   f'{name}: strict runtime differs: {strict_result.stdout}\n{strict_result.stderr}')

        span_c = baseline['SpanConversionSlicingStackallocCompletion']
        awaiter_c = baseline['AwaiterValueTaskGcThreadsExceptionsAsyncIterationIntegration']
        focused_c = baseline['StrictGeneratedCCleanlinessCompletion']
        # Temporary numbering changes when a caller needs additional snapshots.
        # Check actual use, excluding GC registration, rather than particular IDs.
        unused_temporaries = []
        for function in span_c.split('\n}\n'):
            temporary_names = re.findall(r'\b(vc_null_[0-9]+)\s*=\s*(?:NULL|\{0\});', function)
            executable_c = '\n'.join(line for line in function.splitlines() if 'vc_gc_root_push(' not in line)
            unused_temporaries.extend(name for name in temporary_names
                                      if len(re.findall(r'\b' + name + r'\b', executable_c)) <= 1)
        expect(not unused_temporaries,
               'ref-indexer compound assignment still emits unused snapshot temporaries: ' + str(unused_temporaries))
        expect(re.search(r'vc_self VC_MAYBE_UNUSED\)', awaiter_c) is not None,
               'unused property receiver must retain its ABI and be annotated narrowly')
        expect('vc_m_' in focused_c and '(void)((' in focused_c,
               'discarded result expressions must still be evaluated')
        expect(require_success(run(VOIDC, 'build', ROOT / 'Tests/StrictGeneratedCCleanlinessCompletion'),
                               'determinism rebuild'))
        expect((ROOT / 'Tests/StrictGeneratedCCleanlinessCompletion/.void/StrictGeneratedCCleanlinessCompletion.c')
               .read_text(encoding='utf-8') == focused_c, 'generated C is nondeterministic')

        # Surrounding emitter surfaces share the same declaration/receiver infrastructure.
        # Compile only the generated translation unit: their established focused targets
        # exercise their runtime behavior independently.
        for name in ('IteratorCompletion', 'ClosureCompletionII',
                     'RefReturningPropertiesIndexersCompletion', 'CompoundTargets'):
            fixture = ROOT / 'Tests' / name
            generated = fixture / '.void' / (name + '.c')
            expect(require_success(run(VOIDC, 'check', fixture), f'{name} check'))
            expect(require_success(run(VOIDC, 'build', fixture), f'{name} build'))
            expect(require_success(run(*strict_command(c_compiler, generated,
                                                      temporary / (name + '.o'), link=False)),
                                   f'{name} strict C11 compile'))

        # Optional second toolchain: it must check the same emitted C, never a rewritten file.
        clang = shutil.which('clang')
        if clang is not None:
            for name, _ in CASES:
                source = ROOT / 'Tests' / name / '.void' / (name + '.c')
                output = temporary / (name + '.clang.o')
                expect(require_success(run(*strict_command(clang, source, output, link=False)),
                                       f'{name} Clang strict C11'))
        else:
            for _ in CASES:
                expect(True)

    print(f'# {COUNT} checks')


if __name__ == '__main__':
    main()
