#!/usr/bin/env python3
"""#380 independent PCG vectors, fenv, lifetime integration and clock limits.

Builds all generated probes and binaries in isolated temporary directories.
Uses the existing strict-native compilation and process helpers from #378.
"""
from __future__ import annotations
import json
import os
from pathlib import Path
import shutil
import tempfile
import test_high_resolution_stopwatch_foundation as harness

ROOT = harness.ROOT
FIXTURE = ROOT / 'Tests/MathRandomTimingIntegrationAudit'


class Reference:
    """Python arbitrary-width PCG reference; all wrapping is explicit."""
    def __init__(self, seed):
        self.state = 1442695040888963407
        self.state = (self.state + (seed & 0xffffffff)) % (1 << 64)
        self.word()
        self.rejections = 0

    def word(self):
        old = self.state
        self.state = (old * 6364136223846793005 + 1442695040888963407) % (1 << 64)
        x = (((old // (1 << 18)) ^ old) // (1 << 27)) % (1 << 32)
        r = old // (1 << 59)
        # Rotate using concatenation, independently of unsigned negated shifts.
        return ((x + (x << 32)) >> r) % (1 << 32)

    def bounded(self, width):
        cutoff = (1 << 32) % width
        while True:
            word = self.word()
            if word >= cutoff:
                return word % width
            self.rejections += 1


def reference_source():
    lines = ['using Void;', 'public static class AuditReference { public static void Run() {']
    rejections = 0
    for index, seed in enumerate((0, 1, -1, -2147483648, 2147483647, 12345)):
        ref = Reference(seed)
        name = f'r{index}'
        seed_literal = 'int.MinValue' if seed == -2147483648 else str(seed)
        lines += [f'Random {name} = new Random({seed_literal});']
        for cycle in range(12):
            # Width 2^31+1 exercises substantial, deterministic rejection.
            n = -2147483648 + ref.bounded(2147483649)
            lines += [f'Console.WriteLine({name}.Next(int.MinValue, 1) == {n});']
            bits53 = (ref.word() >> 5) * (1 << 26) + (ref.word() >> 6)
            lines += [f'Console.WriteLine({name}.NextDouble() * 9007199254740992.0 == {bits53}.0);']
            bits24 = ref.word() >> 8
            lines += [f'Console.WriteLine({name}.NextSingle() * 16777216.0f == {bits24}.0f);']
            count = cycle % 9
            raw = b''.join(ref.word().to_bytes(4, 'little') for _ in range((count + 3) // 4))[:count]
            array = f'b{index}_{cycle}'
            lines += [f'byte[] {array} = new byte[{count}];', f'{name}.NextBytes({array});']
            condition = ' && '.join(f'{array}[{i}] == (byte){v}' for i, v in enumerate(raw)) or 'true'
            lines += [f'Console.WriteLine({condition});']
            # Zero/equal bounds and failed requests must consume no words.
            lines += [f'{name}.Next(0);', f'{name}.Next(7, 7);',
                      f'try {{ {name}.Next(-1); }} catch (ArgumentOutOfRangeException) {{ }}',
                      f'try {{ {name}.Next(1, -1); }} catch (ArgumentOutOfRangeException) {{ }}',
                      f'try {{ {name}.NextBytes(null); }} catch (ArgumentNullException) {{ }}']
            next_value = ref.bounded(2147483647)
            lines += [f'Console.WriteLine({name}.Next() == {next_value});']
        rejections += ref.rejections
    assert rejections > 30, rejections
    lines += ['} }']
    return '\n'.join(lines) + '\n'


def clock_checks(dest):
    if os.name == 'nt':
        print('POSIX clock injection unavailable on Windows (not counted)', flush=True)
        return
    # Arbitrary-width Python multiplication supplies independent exact quotients
    # for the Windows fraction arithmetic, executed as a C unit on this host.
    vectors = [(0, 1), (1, 3), (9999999, 10000000),
               ((1 << 63) - 2, (1 << 63) - 1),
               ((1 << 64) - 2, (1 << 64) - 1)]
    for i in range(1, 257):
        frequency = ((1 << 64) - 1) // i
        remainder = (frequency * (i % 7 + 1)) // 9
        vectors.append((remainder, frequency))
    rows = [f'{{UINT64_C({r}), UINT64_C({f}), UINT64_C({r * 1000000000 // f})}}'
            for r, f in vectors]
    clock_source = (ROOT / 'Runtime/src/vc_thread.c').read_text()
    start = clock_source.index('static inline uint64_t vc_native_clock_fraction_ns(')
    end = clock_source.index('\n#ifndef WIN32_LEAN_AND_MEAN', start)
    # Compile the exact Windows arithmetic helper, without emulating Windows APIs.
    (dest / 'clock_vectors.h').write_text(clock_source[start:end]
        + 'static const uint64_t vectors[][3] = {\n' + ',\n'.join(rows) + '\n};\n')
    for compiler in (harness.CC, [shutil.which('clang')] if shutil.which('clang') else None):
        if compiler is None:
            continue
        for opt in ('-O0', '-O2'):
            binary = dest / 'clock'
            result = harness.run(*compiler, '-std=c11', '-Wall', '-Wextra', '-Wpedantic',
                                 '-Werror', opt, '-pthread', '-I' + str(ROOT / 'Runtime/include'),
                                 '-I' + str(dest), FIXTURE / 'native_clock.c', '-o', binary)
            harness.check(result.returncode == 0 and not result.stderr, result.stderr)
            result = harness.run(binary)
            harness.check(result.returncode == 0 and result.stdout == 'True\n' and not result.stderr,
                          f'clock conversion: {result.returncode} {result.stderr}')
    binary = dest / 'clock-ubsan'
    result = harness.run(*harness.CC, '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                         '-O2', '-pthread', '-fsanitize=undefined', '-fno-sanitize-recover=all',
                         '-I' + str(ROOT / 'Runtime/include'), '-I' + str(dest),
                         FIXTURE / 'native_clock.c', '-o', binary)
    harness.check(result.returncode == 0 and not result.stderr, result.stderr)
    result = harness.run(binary)
    harness.check(result.returncode == 0 and result.stdout == 'True\n' and not result.stderr,
                  f'clock UBSan: {result.returncode} {result.stderr}')


def main():
    check = harness.check
    run = harness.run
    version = run(harness.BIN, 'version')
    check(version.returncode == 0 and version.stdout.strip() == 'voidc 0.0.381', 'version')
    with tempfile.TemporaryDirectory(prefix='void380-') as temporary:
        dest = Path(temporary)
        clock_checks(dest)
        for operator in ('&=', '|=', '^='):
            negative = dest / 'Program.void'
            negative.write_text('using Void; public static class Invalid { public static void Main() { '
                                f'double x = 1.0; x {operator} 2.0;'
                                ' } }')
            result = run(harness.BIN, 'check', negative, '--diagnostics=json')
            diagnostics = [json.loads(line) for line in result.stderr.splitlines()
                           if line.startswith('{') and 'schemaVersion' in line]
            valid = result.returncode != 0 and len(diagnostics) == 1
            if valid:
                diagnostic = diagnostics[0]
                span = diagnostic['span']
                valid = (diagnostic['severity'] == 'error'
                         and "is not defined for 'double' and 'double'" in diagnostic['message']
                         and 0 <= span['start']['offset'] < span['end']['offset']
                         <= len(negative.read_text()))
            check(valid, f'invalid bitwise floating compound: {result.stderr}')
        project = dest / 'Audit'
        project.mkdir()
        shutil.copy2(FIXTURE / 'Program.void', project / 'Program.void')
        (project / 'Reference.void').write_text(reference_source())
        # Native fenv import is linked only into this isolated test project.
        (project / 'native').mkdir()
        native = project / 'native/fenv.o'
        result = run(*harness.CC, '-std=c11', '-c', FIXTURE / 'native_fenv.c', '-o', native)
        check(result.returncode == 0 and not result.stderr, result.stderr)
        result = run('ar', 'rcs', project / 'native/libauditfenv.a', native)
        check(result.returncode == 0 and not result.stderr, result.stderr)
        (project / 'Audit.voidproj').write_text(json.dumps({
            'format': 1, 'name': 'Audit', 'output': 'exe', 'version': '0.0.381',
            'sources': ['Program.void', 'Reference.void'],
            'compiler': {'unsafe': True, 'libraryPaths': ['native'],
                         'libraries': ['auditfenv']}}))
        result = run(harness.BIN, 'build', project)
        check(result.returncode == 0 and not result.stderr, result.stdout + result.stderr)
        generated = project / '.void/Audit.c'
        baseline = generated.read_bytes()
        result = run(harness.BIN, 'build', project)
        check(result.returncode == 0 and not result.stderr and generated.read_bytes() == baseline,
              'deterministic C emission')
        expected = 'True\n' * (360 + 64)
        executable = project / 'bin' / ('Audit.exe' if os.name == 'nt' else 'Audit')
        result = run(executable)
        check(result.returncode == 0 and result.stdout == expected and not result.stderr,
              f'integration runtime: {result.returncode}\n{result.stdout}\n{result.stderr}')
        # Keep fixture C beside generated source for the existing strict helper.
        source = dest / 'program.c'
        source.write_text(baseline.decode() + '\n' + (FIXTURE / 'native_fenv.c').read_text())
        for compiler in (harness.CC, [shutil.which('clang')] if shutil.which('clang') else None):
            if compiler is None:
                print('Clang unavailable (not counted)', flush=True)
                continue
            for opt in ('-O0', '-O2'):
                binary = dest / 'strict'
                harness.compile_c(compiler, source, binary, opt=opt, extra=('-frounding-math',))
                result = run(binary)
                check(result.returncode == 0 and result.stdout == expected and not result.stderr,
                      f'{compiler} {opt}: {result.stdout}\n{result.stderr}')
        for sanitizer in ('undefined', 'address'):
            binary = dest / sanitizer
            harness.compile_c(harness.CC, source, binary, opt='-O1', extra=(
                f'-fsanitize={sanitizer}', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer'))
            if sanitizer == 'address':
                # LSan cannot run under the desktop host's tracing environment.
                # Keep ASan memory-access checking; leak checking is separate.
                import subprocess
                env = dict(os.environ)
                env['ASAN_OPTIONS'] = 'detect_leaks=0'
                result = subprocess.run([str(binary)], cwd=ROOT, env=env,
                                        capture_output=True, text=True, timeout=240)
                print('# ASan: leak detection disabled; memory-access checks enabled', flush=True)
            else:
                result = run(binary)
            check(result.returncode == 0 and result.stdout == expected and not result.stderr,
                  f'{sanitizer}: {result.returncode}\n{result.stdout}\n{result.stderr}')
    print(f'# {harness.N} checks', flush=True)


if __name__ == '__main__':
    main()
