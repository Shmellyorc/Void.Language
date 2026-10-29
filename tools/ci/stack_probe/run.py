"""Native-only LLVM 23.1.4 stack experiment. Never configures the compiler project."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import time

STACK_BYTES = 1048576
DEPTHS = (1, 2, 4, 8, 16)
BUDGETS_KIB = (64, 128, 256)


def cmake_path_definition(name, path):
    # CMake may parse source paths again internally. Native backslashes can
    # become escapes (D:\a...), even though subprocess passes one intact argv.
    return f'-D{name}:PATH={path.as_posix()}'


def generate_ir(target, layout, kib):
    static_bytes = kib * 1024
    # Four pointer-array homes + four 32-byte root slots + one 32-byte frame.
    n = (static_bytes - 160) // 32
    lines = [f'target datalayout = "{layout}"', f'target triple = "{target}"',
             '%Slot = type {ptr, i64, ptr, i64}', '%Frame = type {ptr, ptr, ptr, i64}']
    for name, value in [('static_bytes', static_bytes), ('elements', n),
                        ('slot_bytes', 32), ('frame_bytes', 32), ('pointer_bytes', 8)]:
        lines.append(f'@probe_{name} = constant i64 {value}')
    lines += [f'@offsets = private constant [{n} x i64] [' +
              ', '.join(f'i64 {i * 8}' for i in range(n)) + ']',
              'declare void @llvm.memset.p0.i64(ptr, i8, i64, i1)',
              'declare void @vd_gc_frame_enter(ptr, ptr, i64)',
              'declare void @vd_gc_frame_leave(ptr)', 'declare void @vd_gc_poll()',
              'declare i32 @probe_collect()', 'declare void @probe_observe_frame(ptr)',
              # Measurement attribute only. Do not suppress optimization/inlining.
              'define i32 @array_frame(i32 %depth) "warn-stack-size"="1" {', 'entry:']
    for i in range(4):
        lines += [f' %home{i} = alloca [{n} x ptr], align 8',
                  f' call void @llvm.memset.p0.i64(ptr %home{i}, i8 0, i64 {n * 8}, i1 false)']
    lines += [' %roots = alloca [4 x %Slot], align 8', ' %frame = alloca %Frame, align 8']
    for i in range(4):
        for j, (kind, value) in enumerate([('ptr', f'%home{i}'), ('i64', n * 8),
                                           ('ptr', '@offsets'), ('i64', n)]):
            lines += [f' %slot{i}_{j} = getelementptr inbounds [4 x %Slot], ptr %roots, i64 0, i64 {i}, i32 {j}',
                      f' store {kind} {value}, ptr %slot{i}_{j}, align 8']
    lines += [' call void @vd_gc_frame_enter(ptr %frame, ptr %roots, i64 4)',
              ' call void @probe_observe_frame(ptr %frame)',
              ' call void @vd_gc_poll()', ' %last = icmp eq i32 %depth, 1',
              ' br i1 %last, label %leaf, label %recurse', 'leaf:',
              ' %collected = call i32 @probe_collect()', ' br label %exit', 'recurse:',
              ' %next = sub i32 %depth, 1', ' %child = call i32 @array_frame(i32 %next)',
              ' %added = add i32 %child, 1', ' br label %exit', 'exit:',
              ' %result = phi i32 [%collected, %leaf], [%added, %recurse]',
              ' call void @vd_gc_frame_leave(ptr %frame)', ' ret i32 %result', '}']
    return '\n'.join(lines) + '\n'


def classify(returncode, stdout, stderr, depth, frame_bytes, kib, system, negative_control=False):
    """Reject unexplained crashes, setup errors and missing stack/ABI evidence."""
    try:
        output = [json.loads(line) for line in stdout.splitlines()]
        errors = [json.loads(line) for line in stderr.splitlines()]
        controls = [x for x in output if x['event'] == 'stack_control']
        ready = [x for x in output if x['event'] == 'ready']
        if len(controls) != 1 or len(ready) != 1:
            return 'unexpected', 'missing unique native stack/ABI evidence'
        control, start = controls[0], ready[0]
        expected_control = 'beginthreadex_reservation' if system == 'Windows' else 'pthread_explicit_mapping'
        if (control['control'] != expected_control or control['actual_bytes'] != STACK_BYTES or
                control['requested_bytes'] != STACK_BYTES or
                control['stack_high'] - control['stack_low'] != STACK_BYTES or
                start['depth'] != depth or start['static_bytes'] != kib * 1024 or
                start['slot_bytes'] != 32 or start['frame_bytes'] != 32 or
                start['elements_per_home'] != (kib * 1024 - 160) // 32):
            return 'unexpected', 'native stack/ABI differs from experiment'
        if system != 'Windows':
            if (control['guard_high'] != control['stack_low'] or
                    control['guard_high'] - control['guard_low'] != 524288 or
                    control['alternate_signal_stack_bytes'] != 65536):
                return 'unexpected', 'unverified POSIX guard or signal stack'
        elif control['exception_guarantee_request'] != 65536:
            return 'unexpected', 'missing Windows exception headroom'
        if negative_control:
            valid = (returncode == 91 and len(errors) == 1 and errors[0]['event'] == 'unexpected_fault' and
                     not any(x['event'] == 'passed' for x in output))
            return ('negative_control_pass', 'unrelated fault rejected') if valid else ('unexpected', 'negative control failed')
        passed = [x for x in output if x['event'] == 'passed']
        cleanup = [x for x in output if x['event'] == 'cleanup']
        cleanup_ok = system == 'Windows' or (
            [x['operation'] for x in cleanup] == ['sigaltstack_disable', 'sigaltstack_query_disabled', 'munmap_alternate'] and
            all(x['result'] == 0 and x['errno'] == 0 for x in cleanup))
        success = (returncode == 0 and not errors and len(passed) == 1 and
                   cleanup_ok and
                   passed[0]['live_frames'] == depth and passed[0]['balanced_cleanup'] is True and
                   passed[0]['observed_frames'] == depth and
                   passed[0]['min_stride_bytes'] == passed[0]['max_stride_bytes'] and
                   (depth == 1 or passed[0]['max_stride_bytes'] > 0))
        overflow = returncode == 90 and len(errors) == 1 and not passed and errors[0]['event'] == 'stack_overflow'
        if overflow and system == 'Windows':
            overflow = (errors[0]['evidence'] == 'windows_exception' and
                        errors[0]['exception_code'] == 0xC00000FD)
        elif overflow:
            address = int(errors[0]['fault_address'], 16)
            overflow = (errors[0]['evidence'] == 'fault_in_low_guard' and
                        errors[0]['signal'] in ('SIGSEGV', 'SIGBUS') and
                        control['guard_low'] <= address < control['guard_high'])
        # Generous pass region; classify boundary cases observationally, without
        # requiring prior Linux thresholds to hold on Windows or macOS.
        native_lower_bound = depth * frame_bytes
        if native_lower_bound <= STACK_BYTES * 3 // 4:
            return ('pass', 'within conservative pass region') if success else ('unexpected', 'small case did not complete')
        if native_lower_bound >= STACK_BYTES:
            return ('expected_overflow', 'native frames exceed entire worker stack') if overflow else ('unexpected', 'required isolated overflow not observed')
        if success:
            return 'boundary_pass', 'observed boundary outcome'
        if overflow:
            return 'boundary_overflow', 'observed verified boundary overflow'
        return 'unexpected', 'unexplained exit, signal or output'
    except (ValueError, KeyError, TypeError):
        return 'unexpected', 'invalid native JSON evidence'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--llvm-bin', type=Path, help='LLVM 23.1.4 bin directory; defaults to PATH')
    parser.add_argument('--output', type=Path, default=Path('build/phase23-stack-probe'))
    parser.add_argument('--generator', default='Ninja' if shutil.which('ninja') else 'Unix Makefiles')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    out = args.output.resolve(); artifacts = out / 'artifacts'; artifacts.mkdir(parents=True, exist_ok=True)
    commands = []

    def command(arguments, log, timeout=120, check=True):
        arguments = [str(x) for x in arguments]
        begin = time.monotonic()
        try:
            result = subprocess.run(arguments, capture_output=True, text=True, encoding='utf-8',
                                    errors='replace', timeout=timeout)
        except subprocess.TimeoutExpired as error:
            def decoded(value):
                return value.decode('utf-8', errors='replace') if isinstance(value, bytes) else (value or '')
            (artifacts / log).write_text(decoded(error.stdout) + decoded(error.stderr) + '\nCHILD TIMEOUT\n', encoding='utf-8')
            commands.append(dict(argv=arguments, exit=None, timeout=True,
                                 seconds=round(time.monotonic() - begin, 3), log=log))
            (artifacts / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n', encoding='utf-8')
            raise
        (artifacts / log).write_text(result.stdout + result.stderr, encoding='utf-8')
        commands.append(dict(argv=arguments, exit=result.returncode, seconds=round(time.monotonic() - begin, 3), log=log))
        (artifacts / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n', encoding='utf-8')
        if check and result.returncode:
            raise RuntimeError(f'Command failed; see {artifacts / log}')
        return result

    records = []
    environment = {}

    def report(error=None):
        document = dict(environment=environment, cases=records, error=error,
                        note='Native worker-thread investigation only; no array implementation or compiler limit.')
        (artifacts / 'results.json').write_text(json.dumps(document, indent=2) + '\n', encoding='utf-8')
        lines = ['# Phase 23 native stack probe', '',
                 f"Host: {environment.get('system', '?')} {environment.get('machine', '?')}; target: {environment.get('target', '?')}",
                 '', 'Worker stack: 1048576 bytes. Windows reserves this extent; POSIX explicitly maps it.',
                 'Guard/headroom and TLS/startup overhead differ by OS. This is not main-thread RLIMIT_STACK.',
                 'Actual execution is only the host shown above. No cross-target execution is inferred.', '',
                 '| KiB planned | Mode | LLVM frame bytes | Observed frame stride bytes | Native depth outcomes |',
                 '| --- | --- | --- | --- | --- |']
        for case in records:
            outcomes = ', '.join(f"{x['depth']}:{x['classification']}" for x in case.get('runs', []))
            strides = sorted({x['native_pass']['max_stride_bytes'] for x in case.get('runs', [])
                              if x.get('native_pass', {}).get('max_stride_bytes', 0)})
            lines.append(f"| {case['kib']} | {case['mode']} | {case['frame_bytes']} | {strides} | {outcomes} |")
        if error:
            lines += ['', f'ERROR: {error}']
        lines += ['', 'Probe results do not enact compiler policy; approval status is recorded in docs/ARRAYS_PROPOSAL.md. No full compiler build, CTest matrix or Phase 23 acceptance is implied.']
        (artifacts / 'summary.md').write_text('\n'.join(lines) + '\n', encoding='utf-8')

    try:
        system, machine = platform.system(), platform.machine().lower()
        native_arch = 'x64' if machine in ('x86_64', 'amd64') else 'arm64' if machine in ('arm64', 'aarch64') else None
        if (system, native_arch) not in [('Linux', 'x64'), ('Windows', 'x64'), ('Darwin', 'arm64')]:
            raise RuntimeError(f'Unsupported native host {system} {machine}')
        if not args.llvm_bin and not shutil.which('clang'):
            raise RuntimeError('LLVM 23.1.4 clang not found')
        bin_dir = args.llvm_bin.resolve() if args.llvm_bin else Path(shutil.which('clang')).resolve().parent
        suffix = '.exe' if system == 'Windows' else ''
        clang = bin_dir / ('clang' + suffix)
        cxx = bin_dir / ('clang-cl.exe' if system == 'Windows' else 'clang++')
        for name, compiler in [('clang', clang), ('cxx', cxx)]:
            version = command([compiler, '--version'], name + '-version.log').stdout
            if not re.search(r'clang version 23\.1\.4(?:\D|$)', version):
                raise RuntimeError(f'{name} must be Clang 23.1.4')
        target = command([clang, '-dumpmachine'], 'native-target.log').stdout.strip()
        if not target.startswith(('x86_64-' if native_arch == 'x64' else ('aarch64-', 'arm64-'))):
            raise RuntimeError('Compiler target architecture differs from executing host')
        if not ((system == 'Windows' and 'windows' in target) or
                (system == 'Linux' and 'linux' in target) or
                (system == 'Darwin' and 'apple' in target)):
            raise RuntimeError('Compiler target OS differs from executing host')
        empty = out / 'empty.c'; empty.write_text('', encoding='utf-8')
        native_ir = command([clang, '-S', '-emit-llvm', '-x', 'c', empty, '-o', '-'], 'native-layout.log').stdout
        layout = re.search(r'target datalayout = "([^"]+)"', native_ir).group(1)
        environment.update(system=system, machine=machine, target=target, datalayout=layout,
                           llvm_bin=str(bin_dir), python=sys.version, platform=platform.platform(),
                           commit=subprocess.check_output(['git', '-C', root, 'rev-parse', 'HEAD'], text=True).strip(),
                           source_hashes={str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
                                          for p in [Path(__file__), root / 'tools/ci/stack_probe/host.cpp',
                                                    root / 'tools/ci/stack_probe/CMakeLists.txt',
                                                    *sorted((root / 'src/runtime').glob('*.cpp')),
                                                    *sorted((root / 'include/void').glob('*.hpp'))]})
        (artifacts / 'environment.json').write_text(json.dumps(environment, indent=2) + '\n', encoding='utf-8')
        for mode in ('O0', 'O2'):
            objects = out / mode / 'objects'; objects.mkdir(parents=True, exist_ok=True)
            mode_records = []
            for kib in BUDGETS_KIB:
                text = generate_ir(target, layout, kib)
                source = artifacts / f'frame-{kib}-{mode}.ll'; source.write_text(text, encoding='utf-8')
                obj = objects / f'frame-{kib}.o'
                compiled = command([clang, '-c', f'-{mode}', source, '-o', obj], f'frame-{kib}-{mode}.log')
                sizes = re.findall(r'stack frame size \((\d+)\)', compiled.stderr)
                if len(sizes) != 1 or int(sizes[0]) < kib * 1024 or int(sizes[0]) >= 524288:
                    raise RuntimeError('Missing/unexpected frame measurement; guard size cannot be trusted')
                command([clang, '-S', f'-{mode}', source, '-o', artifacts / f'frame-{kib}-{mode}.s'],
                        f'assembly-{kib}-{mode}.log')
                case = dict(kib=kib, mode=mode, frame_bytes=int(sizes[0]), runs=[])
                records.append(case); mode_records.append(case)
            tree = out / mode / 'build'
            command(['cmake', '-S', root / 'tools/ci/stack_probe', '-B', tree, '-G', args.generator,
                     '-DCMAKE_BUILD_TYPE=Probe', cmake_path_definition('CMAKE_CXX_COMPILER', cxx),
                     f'-DPROBE_MODE={mode}', cmake_path_definition('PROBE_OBJECT_DIR', objects)], f'configure-{mode}.log')
            command(['cmake', '--build', tree, '--parallel', '3'], f'build-{mode}.log', timeout=300)
            if system != 'Windows':
                # Observe the old zero-size disable call, then require the actual
                # corrected disable/query/unmap lifecycle to complete successfully.
                lifecycle = command([tree / 'stack_probe_64', 1, STACK_BYTES, 'cleanup-check'],
                                    f'cleanup-check-{mode}.log', timeout=20)
                events = [json.loads(line) for line in lifecycle.stdout.splitlines()]
                observations = [x for x in events if x['event'] == 'zero_size_disable']
                if (len(observations) != 1 or not any(x['event'] == 'cleanup_checked' for x in events)):
                    raise RuntimeError('Missing native signal-stack lifecycle evidence')
                environment.setdefault('cleanup_checks', {})[mode] = events
            for case in mode_records:
                exe = tree / f"stack_probe_{case['kib']}{suffix}"
                # A deliberately unrelated fault must never become expected overflow.
                controls = command([exe, 1, STACK_BYTES, 'unexpected-fault'],
                                   f"negative-{case['kib']}-{mode}.log", timeout=20, check=False)
                kind, reason = classify(controls.returncode, controls.stdout, controls.stderr, 1,
                                        case['frame_bytes'], case['kib'], system, negative_control=True)
                case['negative_control'] = dict(classification=kind, reason=reason, exit=controls.returncode)
                for depth in DEPTHS:
                    log = f"run-{case['kib']}-{mode}-depth-{depth}.log"
                    try:
                        run = command([exe, depth, STACK_BYTES], log, timeout=20, check=False)
                        kind, reason = classify(run.returncode, run.stdout, run.stderr, depth,
                                                case['frame_bytes'], case['kib'], system)
                        record = dict(depth=depth, exit=run.returncode, classification=kind,
                                      reason=reason, log=log)
                        try:
                            for line in run.stdout.splitlines():
                                event = json.loads(line)
                                if event.get('event') == 'stack_control':
                                    record['native_stack'] = event
                                elif event.get('event') == 'passed':
                                    record['native_pass'] = event
                                elif event.get('event') == 'cleanup':
                                    record.setdefault('native_cleanup', []).append(event)
                        except (ValueError, AttributeError):
                            pass # Classification above already rejects malformed evidence.
                    except subprocess.TimeoutExpired:
                        record = dict(depth=depth, classification='unexpected', reason='child timeout')
                    case['runs'].append(record)
                    report()
                print(f"{case['kib']} KiB {mode}: " + ', '.join(f"{x['depth']}={x['classification']}" for x in case['runs']), flush=True)
        failures = [case for case in records if case['negative_control']['classification'] != 'negative_control_pass' or
                    any(x['classification'] == 'unexpected' for x in case['runs']) or
                    not any(x['classification'] == 'expected_overflow' for x in case['runs'])]
        if failures:
            raise RuntimeError('Unexpected probe outcomes; inspect per-case native evidence')
        report()
        print(artifacts / 'summary.md', flush=True)
        return 0
    except Exception as error:
        report(f'{type(error).__name__}: {error}')
        print(f'Probe failed: {error}; artifacts: {artifacts}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
