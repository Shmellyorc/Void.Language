#!/usr/bin/env python3
"""#359 application stabilization: volatile unwind storage and composed #351-#358 rules."""
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
FIXTURE = ROOT / 'Tests' / 'CorrectnessBlockStabilization'
NAME = 'CorrectnessBlockStabilization'
GENERATED = FIXTURE / '.void' / f'{NAME}.c'
BINARY = FIXTURE / 'bin' / (NAME + ('.exe' if os.name == 'nt' else ''))
RUNTIME = sorted((ROOT / 'Runtime/src').glob('*.c'))
EXPECTED = 'True\n' * 37
COUNT = 0


def check(condition: bool, message: str, *, count: bool = True) -> None:
    global COUNT
    if not condition:
        raise AssertionError(message)
    if count:
        COUNT += 1
        print('True', flush=True)


def invoke(*args: object, timeout: int = 180) -> subprocess.CompletedProcess[str]:
    return subprocess.run([str(arg) for arg in args], cwd=ROOT, capture_output=True,
                          text=True, timeout=timeout)


def run_exact(binary: Path, *, count: bool = True) -> None:
    result = invoke(binary, timeout=45)
    check(result.returncode == 0 and result.stdout == EXPECTED and not result.stderr,
          f'{binary}: {result.returncode}\nstdout:\n{result.stdout}\nstderr:\n{result.stderr}',
          count=count)


def strict_compile(compiler: list[str], optimization: str, dest: Path,
                   *, ubsan: bool = False, count: bool = True) -> None:
    flags = ['-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', optimization,
             '-I' + str(ROOT / 'Runtime/include')]
    if os.name != 'nt':
        flags += ['-pthread']
    if ubsan:
        flags += ['-fsanitize=undefined', '-fno-sanitize-recover=undefined']
    result = invoke(*compiler, *flags, GENERATED, *RUNTIME, '-lm', '-o', dest, timeout=240)
    check(result.returncode == 0 and not result.stderr,
          f'{compiler} {optimization}: {result.stdout}\n{result.stderr}', count=count)


def unassigned_read(folder: Path, source: str, name: str) -> None:
    folder.mkdir()
    program = folder / 'Program.void'
    program.write_text(source, encoding='utf-8')
    result = invoke(VOIDC, 'check', program, '--diagnostics=json', timeout=60)
    diagnostics = [json.loads(line) for line in result.stderr.splitlines()
                   if line.startswith('{') and 'schemaVersion' in line]
    valid = result.returncode != 0 and len(diagnostics) == 1
    if valid:
        diagnostic = diagnostics[0]
        span = diagnostic.get('span') or {}
        begin = (span.get('start') or {}).get('offset', -1)
        end = (span.get('end') or {}).get('offset', -1)
        valid = (diagnostic.get('code') == 'VOID3000' and
                 'definitely assigned' in diagnostic.get('message', '') and
                 0 <= begin < end <= len(source) and source[begin:end] == name)
    check(valid, f'Expected read-site VOID3000 for {name}, got:\n{result.stderr}')


def main() -> None:
    version = invoke(VOIDC, 'version')
    check(version.returncode == 0 and version.stdout.strip() == 'voidc 0.0.381',
          f'version: {version.stdout} {version.stderr}')
    built = invoke(VOIDC, 'build', FIXTURE)
    check(built.returncode == 0 and not built.stderr,
          f'build: {built.stdout}\n{built.stderr}')
    run_exact(BINARY)
    first = GENERATED.read_bytes()
    second = invoke(VOIDC, 'build', FIXTURE)
    check(second.returncode == 0 and not second.stderr and GENERATED.read_bytes() == first,
          f'generated C must be deterministic: {second.stdout}\n{second.stderr}')

    with tempfile.TemporaryDirectory(prefix='void359-stabilization-') as scratch:
        base = Path(scratch)
        cc = shlex.split(os.environ.get('CC', 'cc'))
        for optimization in ('-O0', '-O2'):
            executable = base / ('gcc-' + optimization + ('.exe' if os.name == 'nt' else ''))
            strict_compile(cc, optimization, executable)
            run_exact(executable)
        if shutil.which('clang') and os.name != 'nt':
            executable = base / 'clang-strict'
            strict_compile(['clang'], '-O2', executable, count=False)
            run_exact(executable, count=False)
            print('Clang strict C: passed')
        else:
            print('Clang strict C: unavailable')
        if os.name != 'nt':
            executable = base / 'ubsan'
            strict_compile(cc, '-O1', executable, ubsan=True, count=False)
            run_exact(executable, count=False)
            print('UBSan: passed')
        else:
            print('UBSan: unavailable')

        unassigned_read(base / 'zero-iteration', '''using Void;
public static class Program {
    private static int Compute() {
        int output;
        for (int i = 0; i < 0; i++) {
            try { output = i; } finally { Console.WriteLine("end"); }
        }
        return output;
    }
    public static void Main() { Console.WriteLine(Compute()); }
}
''', 'output')
        unassigned_read(base / 'short-circuit', '''using Void;
public static class Program {
    private static int Compute(bool ready) {
        int output;
        if (ready || ((output = 7) > 0)) { }
        return output;
    }
    public static void Main() { Console.WriteLine(Compute(false)); }
}
''', 'output')
        # Repeat composed control flow and GC-visible async/delegate execution to
        # make nondeterministic state/lifetime mistakes reproducible.
        for _ in range(3):
            run_exact(BINARY, count=False)
        check(True, 'repeated executions must remain identical')
    print(f'# {COUNT} checks')


if __name__ == '__main__':
    main()
