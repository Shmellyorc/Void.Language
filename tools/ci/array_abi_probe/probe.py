"""Compare raw aggregates with explicit address-based LLVM array calls. Does not enable VOID native arrays.

Normal means no LLVM IR optimization; O2 uses default<O2>. Both use llc's
-O2 machine-code pipeline, matching the current AOT TargetMachine default.
Targets other than the executing Linux x64 host are compile-only evidence.
"""
from pathlib import Path
import argparse
import json
import platform
import re
import resource
import subprocess
import shlex
from address import ir as address_ir, semantics
from audit import inspect, require_covered


def ir(target, layout, count, caller):
    value = f'[{count} x i32]'
    prefix = f'target triple = "{target}"\ntarget datalayout = "{layout}"\n'
    if not caller:
        return prefix + f'''declare void @touch(ptr, i64)
define {value} @roundtrip({value} %a, ptr %failure) "warn-stack-size"="1" {{
 %home = alloca {value}
 store {value} %a, ptr %home
 call void @touch(ptr %home, i64 {count})
 %value = load {value}, ptr %home
 ret {value} %value
}}
'''
    return prefix + f'''declare {value} @roundtrip({value}, ptr)
declare void @fill(ptr, i64, i32)
declare i32 @check(ptr, i64, i32)
define i32 @entry(i32 %seed, ptr %failure) "warn-stack-size"="1" {{
 %input = alloca {value}
 %output = alloca {value}
 call void @fill(ptr %input, i64 {count}, i32 %seed)
 %a = load {value}, ptr %input
 %r = call {value} @roundtrip({value} %a, ptr %failure)
 store {value} %r, ptr %output
 %checked = call i32 @check(ptr %output, i64 {count}, i32 %seed)
 ret i32 %checked
}}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--llvm-bin', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--cross-targets', action='store_true')
    parser.add_argument('--calling', choices=('raw', 'address', 'both'), default='both')
    parser.add_argument('--counts', type=int, nargs='+')
    parser.add_argument('--jit', action='store_true', help='also execute Linux address cases with LLVM lli ORC')
    args = parser.parse_args()
    if args.counts and any(n <= 0 or n > 65536 for n in args.counts):
        parser.error('probe counts must be 1..65536')
    if platform.system() != 'Linux' or platform.machine().lower() not in ('x86_64', 'amd64'):
        parser.error('this investigation host requires native Linux x64')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    tools = args.llvm_bin.resolve()
    commands, results = [], []
    (output / 'results.json').write_text('[]\n')
    (output / 'complete.json').write_text('{"passed": false}\n')

    def command(argv, directory, label, stack_bytes=None, stdin=None):
        def stack_limit():
            resource.setrlimit(resource.RLIMIT_STACK, (stack_bytes, stack_bytes))
        argv = [str(x) for x in argv]
        try:
            result = subprocess.run(argv, input=stdin, text=True, encoding='utf-8',
                                    errors='strict', capture_output=True, timeout=120,
                                    preexec_fn=stack_limit if stack_bytes else None)
        except subprocess.TimeoutExpired:
            commands.append(dict(argv=argv, returncode=None, timeout=True))
            (output / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
            raise
        (directory / (label + '.log')).write_text(result.stdout + result.stderr, encoding='utf-8')
        commands.append(dict(argv=argv, stack_bytes=stack_bytes, returncode=result.returncode,
                             log=str(directory.relative_to(output) / (label + '.log'))))
        (output / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
        if result.returncode:
            raise RuntimeError(f'{label} failed: {result.stderr}')
        return result.stdout

    for tool in ('clang', 'clang++', 'opt', 'llc', *(('lli',) if args.jit else ())):
        version = command([tools / tool, '--version'], output, tool + '-version')
        if not re.search(r'version 23\.1\.4(?:\D|$)', version):
            raise RuntimeError(f'{tool} must be LLVM 23.1.4')
    if command([tools / 'llvm-config', '--version'], output, 'llvm-config-version').strip() != '23.1.4':
        raise RuntimeError('development SDK must be LLVM 23.1.4')
    cxxflags = shlex.split(command([tools / 'llvm-config', '--cxxflags'], output, 'sdk-cxxflags'))
    linkflags = shlex.split(command([tools / 'llvm-config', '--ldflags', '--libs', 'core', 'support',
                                    'target', 'x86', 'aarch64', '--system-libs'], output, 'sdk-linkflags'))
    facts_tool = output / 'target-facts'
    command([tools / 'clang++', *cxxflags, '-std=c++20', Path(__file__).with_name('target_facts.cpp').resolve(),
             *linkflags, '-o', facts_tool], output, 'build-target-facts')
    profiles = {}
    native = command([tools / 'clang', '-dumpmachine'], output, 'native-target').strip()
    if not native.startswith('x86_64-') or 'linux' not in native:
        raise RuntimeError('compiler target differs from executing Linux x64 host')
    targets = [native]
    if args.cross_targets:
        targets += ['x86_64-pc-windows-msvc', 'arm64-apple-macosx14.0.0']
    host = Path(__file__).with_name('host.c').resolve()
    families = ('raw', 'address') if args.calling == 'both' else (args.calling,)

    def module(source, directory, name, mode, target, homes):
        path = directory / (name + '.ll')
        path.write_text(source, encoding='utf-8')
        command([tools / 'opt', '-passes=verify', '-disable-output', path], directory, name + '-verify')
        if mode == 'O2':
            optimized = directory / (name + '-O2.ll')
            command([tools / 'opt', '-S', '-passes=default<O2>', path, '-o', optimized], directory, name + '-opt')
            path = optimized
        mir = directory / (name + '.mir')
        command([tools / 'llc', '-O2', '-relocation-model=pic', '-stop-after=prologepilog', path, '-o', mir],
                directory, name + '-frame')
        text = mir.read_text(encoding='utf-8')
        report = inspect(text, homes, -profiles[target]['local_area_offset'])
        (directory / (name + '-audit.json')).write_text(json.dumps(report, indent=2) + '\n')
        if target == native:
            command([tools / 'llc', '-O2', '-filetype=obj', '-relocation-model=pic', path,
                     '-o', directory / (name + '.o')], directory, name + '-object')
            warnings = re.findall(r'stack frame size \((\d+)\)',
                                  (directory / (name + '-object.log')).read_text())
            if warnings != [str(report['frame_bytes'])]:
                raise RuntimeError('native object frame warning differs from MIR')
        return report

    for target in targets:
        witness = '_Static_assert(sizeof(int)==4 && _Alignof(int)==4 && sizeof(void*)==8, "layout");\n'
        layout_ir = command([tools / 'clang', '-target', target, '-S', '-emit-llvm',
                             '-x', 'c', '-', '-o', '-'], output, 'layout-' + target, stdin=witness)
        layout = re.search(r'^target datalayout = "([^"]+)"', layout_ir, re.M).group(1)
        facts = json.loads(command([facts_tool, target], output, 'facts-' + target))
        if (facts['data_layout'] != layout or facts['i32_bytes'] != 4 or facts['i32_alignment'] != 4 or
                facts['pointer_bytes'] != 8 or facts['stack_alignment'] != 16 or not facts['stack_grows_down'] or
                facts['local_area_offset'] not in (0, -facts['pointer_bytes'])):
            raise RuntimeError('unsupported target layout/frame prefix')
        # For these pinned X86/AArch64 targets, FrameLowering's local-area
        # prefix accounts for the entry return slot not in getStackSize.
        profiles[target] = facts
        (output / 'target-profiles.json').write_text(json.dumps(profiles, indent=2) + '\n')
        if '-S128' not in layout:
            raise RuntimeError('unproved target stack alignment')
        for family in families:
            counts = args.counts or ((1024, 4096) if family == 'raw' else (1024, 4096, 5440, 5461))
            for count in counts:
                for mode in ('normal', 'O2'):
                    directory = output / f'{target}-{family}-{count}-{mode}'
                    directory.mkdir(exist_ok=True)
                    reports = {}
                    for name, caller in (('callee', False), ('caller', True)):
                        names = ('input', 'snapshot', 'output') if caller and family == 'address' else ('input', 'output') if caller else ('home',)
                        homes = {n: dict(bytes=count * 4, alignment=4) for n in names}
                        generator = address_ir if family == 'address' else ir
                        report = module(generator(target, layout, count, caller), directory, name, mode, target, homes)
                        if family == 'address':
                            require_covered(report)
                        reports[name] = report
                    native_runs, semantic_reports, semantic_objects, semantic_sources = [], {}, [], []
                    if family == 'address' and (target == native or count == counts[0]):
                        for name, caller in (('semantics-callee', False), ('semantics-caller', True)):
                            names = ('input', 'snapshot', 'output') if caller else ('home',)
                            semantic_reports[name] = module(semantics(target, layout, caller), directory,
                                name, mode, target, {n: dict(bytes=64, alignment=4) for n in names})
                            semantic_objects.append(directory / (name + '.o'))
                            semantic_sources.append(directory / (name + ('-O2.ll' if mode == 'O2' else '.ll')))
                    if target == native:
                        objects = [directory / 'caller.o', directory / 'callee.o', *semantic_objects]
                        executable = directory / 'roundtrip'
                        defines = ['-DADDRESS_PROBE'] if family == 'address' else []
                        command([tools / 'clang', '-O2', '-std=c11', '-Wall', '-Wextra', '-Werror',
                                 *defines, host, *objects, '-o', executable], directory, 'link')
                        for stack in (None, 1048576):
                            stdout = command([executable], directory,
                                             'run-default' if stack is None else 'run-1m', stack_bytes=stack)
                            if stdout != 'native array roundtrip passed\n':
                                raise RuntimeError('native result validation missing')
                            native_runs.append(dict(engine='independently linked AOT', stack_bytes=stack, passed=True))
                        if args.jit and family == 'address':
                            host_ir = directory / 'host.ll'
                            command([tools / 'clang', '-O2', '-DADDRESS_PROBE', '-S', '-emit-llvm',
                                     host, '-o', host_ir], directory, 'host-ir')
                            suffix = '-O2.ll' if mode == 'O2' else '.ll'
                            sources = [directory / ('caller' + suffix), directory / ('callee' + suffix), *semantic_sources]
                            stdout = command([tools / 'lli', '--jit-kind=orc', '--mcpu=generic', '-O2',
                                             *['--extra-module=' + str(p) for p in sources], host_ir], directory, 'orc-run')
                            if stdout != 'native array roundtrip passed\n':
                                raise RuntimeError('ORC validation missing')
                            messages = re.findall(r"stack frame size \((\d+)\).*?function '([^']+)'",
                                                  (directory / 'orc-run.log').read_text())
                            measured = {name: int(size) for size, name in messages}
                            expected = dict(entry=reports['caller']['frame_bytes'],
                                            roundtrip=reports['callee']['frame_bytes'],
                                            snapshot_case=semantic_reports['semantics-caller']['frame_bytes'],
                                            checked_roundtrip=semantic_reports['semantics-callee']['frame_bytes'])
                            if len(messages) != 4 or measured != expected:
                                raise RuntimeError('configured ORC machine frames differ from AOT audit')
                            native_runs.append(dict(engine='standalone lli ORC', stack_bytes=None,
                                                    machine_frames=measured, passed=True))
                    record = dict(target=target, data_layout=layout, target_profile=facts, calling=family, mode=mode,
                                  elements=count, payload_bytes=count * 4, audit=reports, semantic_audit=semantic_reports,
                                  execution='native Linux x64' if target == native else 'compile-only',
                                  native_runs=native_runs)
                    results.append(record)
                    (output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
                    print(json.dumps(dict(target=target, calling=family, count=count, mode=mode,
                                          frames={n: r['frame_bytes'] for n, r in reports.items()},
                                          totals={n: r['conservative_total_bytes'] for n, r in reports.items()},
                                          covered={n: r['covered'] for n, r in reports.items()})), flush=True)
    (output / 'complete.json').write_text(json.dumps(dict(cases=len(results), passed=True)) + '\n')
    print('Investigation passed. No compiler storage policy or native-array support enabled.')


if __name__ == '__main__':
    main()
