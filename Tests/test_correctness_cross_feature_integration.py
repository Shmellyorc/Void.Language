#!/usr/bin/env python3
"""#358: application-style composition of the locked #351-#357 semantic rules."""
from __future__ import annotations

import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
VOIDC = ROOT / 'bin' / ('voidc.exe' if os.name == 'nt' else 'voidc')
NAME = 'CorrectnessCrossFeatureIntegration'
FIXTURE = ROOT / 'Tests' / NAME
GENERATED = FIXTURE / '.void' / f'{NAME}.c'
EXE = FIXTURE / 'bin' / (NAME + ('.exe' if os.name == 'nt' else ''))
RUNTIME = sorted((ROOT / 'Runtime/src').glob('*.c'))
EXPECTED = 'True\n' * 36
COUNT = 0


def expect(condition: bool, details: str, *, counted: bool = True) -> None:
    global COUNT
    if not condition:
        raise AssertionError(details)
    if counted:
        COUNT += 1
        print('True')


def invoke(*command: object, timeout: int = 180) -> subprocess.CompletedProcess[str]:
    return subprocess.run([str(arg) for arg in command], cwd=ROOT,
                          capture_output=True, text=True, timeout=timeout)


def run_exact(binary: Path, *, counted: bool = True) -> None:
    result = invoke(binary, timeout=45)
    expect(result.returncode == 0 and result.stdout == EXPECTED and not result.stderr,
           f'{binary}: exit={result.returncode}\n{result.stdout}\n{result.stderr}', counted=counted)


def compile_native(compiler: list[str], optimization: str, output: Path,
                   *, sanitize: bool = False, counted: bool = True) -> None:
    flags = ['-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', optimization,
             '-I' + str(ROOT / 'Runtime/include')]
    if os.name != 'nt':
        flags.append('-pthread')
    if sanitize:
        flags += ['-fsanitize=undefined', '-fno-sanitize-recover=undefined']
    result = invoke(*compiler, *flags, GENERATED, *RUNTIME, '-lm', '-o', output)
    expect(result.returncode == 0 and not result.stderr,
           f'{compiler} {optimization}: {result.stderr}', counted=counted)


def unassigned_case(folder: Path, source: str, local: str) -> None:
    folder.mkdir()
    path = folder / 'Program.void'
    path.write_text(source, encoding='utf-8')
    result = invoke(VOIDC, 'check', path, '--diagnostics=json')
    records = [json.loads(line) for line in result.stderr.splitlines()
               if line.startswith('{') and 'schemaVersion' in line]
    valid = result.returncode != 0 and len(records) == 1
    if valid:
        record = records[0]
        span = record.get('span') or {}
        start = (span.get('start') or {}).get('offset', -1)
        end = (span.get('end') or {}).get('offset', -1)
        valid = (record.get('code') == 'VOID3000' and
                 'definitely assigned' in record.get('message', '') and
                 0 <= start < end <= len(source) and source[start:end] == local)
    expect(valid, f'Expected an unassigned {local} read, got:\n{result.stderr}')


def main() -> None:
    result = invoke(VOIDC, 'version')
    expect(result.returncode == 0 and result.stdout.strip() == 'voidc 0.0.370',
           result.stdout + result.stderr)
    built = invoke(VOIDC, 'build', FIXTURE)
    expect(built.returncode == 0, built.stdout + built.stderr)
    run_exact(EXE)
    first_c = GENERATED.read_bytes()
    second = invoke(VOIDC, 'build', FIXTURE)
    expect(second.returncode == 0 and GENERATED.read_bytes() == first_c,
           'generated C must remain deterministic across repeated builds')

    with tempfile.TemporaryDirectory(prefix='void358-cross-') as base:
        directory = Path(base)
        cc = shlex.split(os.environ.get('CC', 'cc'))
        for optimization in ('-O0', '-O2'):
            exe = directory / ('gcc' + optimization + ('.exe' if os.name == 'nt' else ''))
            compile_native(cc, optimization, exe)
            run_exact(exe)
        # Supplementary supported host verification. GCC/CC remains authoritative.
        if shutil.which('clang') and os.name != 'nt':
            exe = directory / 'clang-strict'
            compile_native(['clang'], '-O2', exe, counted=False)
            run_exact(exe, counted=False)
            print('Clang strict C: passed')
        else:
            print('Clang strict C: not available')
        if os.name != 'nt':
            exe = directory / 'ubsan'
            compile_native(cc, '-O1', exe, sanitize=True, counted=False)
            run_exact(exe, counted=False)
            print('UBSan: passed')
        else:
            print('UBSan: not available on Windows')

        # The binding/flow interaction must remain a semantic error at the read,
        # never a successful build followed by uninitialized generated C.
        unassigned_case(directory / 'negative-conditional', '''using Void;
using Alias = Void.Index;
public sealed class Worker {
    private int Next() { return 1; }
    public int Read(bool take) {
        int value;
        if (take && Next() > 0) value = new Alias(1).Value;
        return value + 1;
    }
}
public static class Program { public static void Main() { Console.WriteLine(new Worker().Read(true)); } }
''', 'value')
        unassigned_case(directory / 'negative-short-circuit', '''using Void;
using Alias = Void.Index;
public static class Program {
    private static int Compute(bool ready) {
        int value;
        if (ready && ((value = new Alias(3).Value) > 0)) { }
        return value;
    }
    public static void Main() { Console.WriteLine(Compute(true)); }
}
''', 'value')
        unassigned_case(directory / 'negative-async', '''using Void;
using Void.Threading.Tasks;
using Alias = Void.Index;
public sealed class Worker {
    private int Next() { return 1; }
    public async Task<int> Read(bool take) {
        int result;
        await Task.Yield();
        if (take) result = Next() + new Alias(1).Value;
        return result;
    }
}
public static class Program { public static void Main() { Console.WriteLine(new Worker().Read(true)); } }
''', 'result')
        unassigned_case(directory / 'negative-switch', '''using Void;
using Alias = Void.Index;
public static class Program {
    private static int Compute(int choice) {
        int value;
        switch (choice) {
            case 1: value = new Alias(5).Value; break;
            default: break;
        }
        return value;
    }
    public static void Main() { Console.WriteLine(Compute(1)); }
}
''', 'value')
    print(f'# {COUNT} checks')


if __name__ == '__main__':
    main()
