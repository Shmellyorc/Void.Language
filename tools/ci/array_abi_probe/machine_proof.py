"""Pinned MachineFunction/encoded-object proof. No production compiler changes.

Linux executes independently linked certified objects. Windows/macOS are only
cross-compiled here. All deliberate failures must return 2 and deliver NO object.
"""
from pathlib import Path
import argparse
import hashlib
import json
import platform
import re
import resource
import shlex
import subprocess
from address import ir, semantics


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--llvm-bin', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--cross-targets', action='store_true')
    args = parser.parse_args()
    if platform.system() != 'Linux' or platform.machine() != 'x86_64':
        parser.error('native Linux x64 host required; cross results are not executions')
    output, tools = args.output.resolve(), args.llvm_bin.resolve()
    if output.exists():
        parser.error('use a fresh output directory to prevent stale delivery artifacts')
    output.mkdir(parents=True)
    here = Path(__file__).resolve().parent
    files = ('machine_emit.cpp', 'machine_proof.py', 'test_machine.py', 'address.py', 'host.c')
    sources = {p: (here / p).read_bytes() for p in files}
    (output / 'sources').mkdir()
    for p, data in sources.items():
        (output / 'sources' / p).write_bytes(data)
    commands, results, controls = [], [], []
    (output / 'complete.json').write_text('{"passed": false}\n')

    def run(argv, label, expected=0, stack=None, stdin=None):
        def limit():
            resource.setrlimit(resource.RLIMIT_STACK, (stack, stack))
        proc = subprocess.run([str(a) for a in argv], input=stdin, text=True,
                              encoding='utf-8', errors='strict', capture_output=True,
                              timeout=120, preexec_fn=limit if stack else None)
        log = output / (label + '.log')
        log.parent.mkdir(parents=True, exist_ok=True)
        log.write_text(proc.stdout + proc.stderr)
        commands.append(dict(argv=[str(a) for a in argv], returncode=proc.returncode,
                             expected=expected, stack=stack, log=str(log.relative_to(output))))
        (output / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
        if proc.returncode != expected:
            raise RuntimeError(f'{label}: wanted {expected}, got {proc.returncode}\n{proc.stdout}{proc.stderr}')
        return proc.stdout

    for name in ('clang', 'clang++', 'llc', 'opt', 'lli', 'llvm-link', 'llvm-objdump'):
        if not re.search(r'version 19\.1\.7(?:\D|$)', run([tools / name, '--version'], name + '-version')):
            raise RuntimeError('LLVM 19.1.7 required: ' + name)
    if run([tools / 'llvm-config', '--version'], 'sdk-version').strip() != '19.1.7':
        raise RuntimeError('LLVM 19.1.7 SDK required')
    flags = shlex.split(run([tools / 'llvm-config', '--cxxflags'], 'cxxflags'))
    libs = shlex.split(run([tools / 'llvm-config', '--ldflags', '--libs', 'all', '--system-libs'], 'linkflags'))
    emitter = output / 'machine-emit'
    run([tools / 'clang++', *flags, '-std=c++17', output / 'sources' / 'machine_emit.cpp', *libs, '-o', emitter], 'build-emitter')
    native = run([tools / 'clang', '-dumpmachine'], 'native-target').strip()
    if native != 'x86_64-pc-linux-gnu':
        raise RuntimeError('unsupported native compiler triple')
    targets = [native]
    if args.cross_targets:
        targets += ['x86_64-pc-windows-msvc', 'arm64-apple-macosx14.0.0']

    def emit(source, mode, label, accepted=True, fault='none', proof=None):
        path = output / (label + '.ll')
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(source)
        run([tools / 'opt', '-passes=verify', '-disable-output', path], label + '-verify')
        report, obj = output / (label + '.json'), output / (label + '.o')
        argv = [emitter, path, mode, report, obj, fault]
        if proof:
            argv.append(proof)
        run(argv, label + '-emit', expected=0 if accepted else 2)
        data = json.loads(report.read_text())
        if data['certified'] != accepted or obj.exists() != accepted:
            raise RuntimeError('fail-closed delivery assertion failed: ' + label)
        if accepted:
            digest = hashlib.sha256(obj.read_bytes()).hexdigest().upper()
            if digest != data['private_object_sha256']:
                raise RuntimeError('delivered object differs from certificate')
            run([tools / 'llvm-objdump', '-d', obj], label + '-disassembly')
        return data, report, obj

    for target in targets:
        witness = run([tools / 'clang', '-target', target, '-S', '-emit-llvm', '-x', 'c', '-', '-o', '-'],
                      target + '/layout', stdin='_Static_assert(sizeof(int)==4 && _Alignof(int)==4 && sizeof(void*)==8, "layout");\n')
        layout = re.search(r'^target datalayout = "([^"]+)"', witness, re.M).group(1)
        prefix = f'target triple = "{target}"\ntarget datalayout = "{layout}"\n'
        for mode in ('normal', 'O2'):
            base = f'{target}/{mode}'
            sc, caller_cert, caller_obj = emit(semantics(target, layout, True), mode, base + '/semantics-caller')
            sf, _, callee_obj = emit(semantics(target, layout, False), mode, base + '/semantics-callee', proof=caller_cert)
            if 'windows' in target:
                rejected, _, _ = emit(semantics(target, layout, False), mode, base + '/unproved-incoming', accepted=False)
                assert any('incoming ownership' in e for es in rejected['errors'].values() for e in es)
                f = sf['functions']['checked_roundtrip']
                assert f['incoming'] == [dict(base_register='RSP', callee_entry_sp_offset=40,
                                              caller_owned=True, offset=32, size=8, sp_reference=f['frame'] + 40)]
                assert sc['functions']['snapshot_case']['machine_calls']['checked_roundtrip'] == 40
                assert sc['functions']['snapshot_case']['proved_five_arg_edges'] == ['checked_roundtrip']
                controls.append(dict(target=target, mode=mode, control='unproved incoming caller ownership', rejected=True))
            for count in (4096, 5440, 5461):
                case = base + '/' + str(count)
                callee, _, co = emit(ir(target, layout, count, False), mode, case + '/callee')
                accepted = count != 5461
                caller, _, ao = emit(ir(target, layout, count, True), mode, case + '/caller', accepted=accepted)
                for report in (caller, callee):
                    for f in report['functions'].values():
                        assert f['C'] == (f['P'] + f['frame'] - f['R'] + f['E'] + f['A'] - 1) // f['A'] * f['A']
                        assert f['sp_writes'] == f['encoded_sp_writes']
                record = dict(target=target, mode=mode, count=count,
                              evidence='native Linux x64' if target == native else 'compile-only',
                              caller=caller, callee=callee, semantic_caller=sc, semantic_callee=sf, runs=[])
                if accepted and target == native:
                    exe = output / (case + '/roundtrip')
                    run([tools / 'clang', '-O2', '-std=c11', '-Wall', '-Wextra', '-Werror', '-DADDRESS_PROBE',
                         here / 'host.c', ao, co, caller_obj, callee_obj, '-o', exe], case + '/link')
                    for stack in (None, 1048576):
                        text = run([exe], case + ('/run-default' if stack is None else '/run-1m'), stack=stack)
                        assert text == 'native array roundtrip passed\n'
                        record['runs'].append(dict(engine='independently linked AOT', stack=stack))
                    host_ir = output / (case + '/host.ll')
                    run([tools / 'clang', '-O2', '-DADDRESS_PROBE', '-S', '-emit-llvm', here / 'host.c', '-o', host_ir], case + '/host-ir')
                    text = run([tools / 'lli', '--jit-kind=orc', *['--extra-object=' + str(o) for o in (ao, co, caller_obj, callee_obj)],
                                host_ir], case + '/orc-certified-objects', stack=1048576)
                    assert text == 'native array roundtrip passed\n'
                    record['runs'].append(dict(engine='standalone ORC certified-object loading', stack=1048576))
                results.append(record)
                (output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
                print(target, mode, count, caller['functions']['entry']['C'], 'delivered' if accepted else 'rejected', flush=True)

            # Reconcile the original address experiment when ordinary O2
            # combines modules. Inlined homes cannot impersonate originals.
            pair = []
            for name, is_caller in (('linked-caller', True), ('linked-callee', False)):
                path = output / (base + '/' + name + '.ll')
                path.write_text(ir(target, layout, 4096, is_caller))
                pair.append(path)
            linked = output / (base + '/linked.ll')
            run([tools / 'llvm-link', '-S', *pair, '-o', linked], base + '/link-ir')
            combined, _, _ = emit(linked.read_text(), mode, base + '/combined', accepted=mode == 'normal')
            cf = combined['functions']['entry']
            assert cf['P'] == 49152
            if mode == 'O2':
                assert any('unmatched allocation' in e for e in combined['errors']['entry'])
            controls.append(dict(target=target, mode=mode, control='combined original address modules',
                                 rejected=mode == 'O2', P=cf['P'], F=cf['frame'], R=cf['R'], C=cf['C']))
            # Fully ordinary inlining into a function whose original inventory is empty.
            source = prefix + '''declare void @touch(ptr, i64)
define void @worker() {
 %home = alloca [256 x i32], align 4
 call void @touch(ptr %home, i64 256)
 ret void
}
define void @originally_array_free() {
 call void @worker()
 ret void
}
'''
            inline, _, _ = emit(source, mode, base + '/inline-array-free', accepted=mode == 'normal')
            if mode == 'O2':
                assert inline['functions']['originally_array_free']['P'] == 0
                assert inline['functions']['originally_array_free']['R'] == 0
                assert any('unmatched allocation' in e for e in inline['errors']['originally_array_free'])
            controls.append(dict(target=target, mode=mode, control='array-free inlining', rejected=mode == 'O2'))
            # Real stack coloring with nonoverlapping lifetimes. No R credit is
            # awarded, even if only one original allocation identity survives.
            reused = prefix + '''declare void @touch(ptr, i64)
declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture)
declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture)
define void @reused() {
 %first = alloca [64 x i32], align 4
 %second = alloca [64 x i32], align 4
 call void @llvm.lifetime.start.p0(i64 256, ptr %first)
 call void @touch(ptr %first, i64 64)
 call void @llvm.lifetime.end.p0(i64 256, ptr %first)
 call void @llvm.lifetime.start.p0(i64 256, ptr %second)
 call void @touch(ptr %second, i64 64)
 call void @llvm.lifetime.end.p0(i64 256, ptr %second)
 ret void
}
'''
            reused_report, _, _ = emit(reused, mode, base + '/reuse')
            rf = reused_report['functions']['reused']
            assert rf['P'] == 512 and rf['R'] == 0 and rf['lifetime_no_credit']
            controls.append(dict(target=target, mode=mode, control='reuse', P=rf['P'], F=rf['frame'], R=rf['R'], C=rf['C'], objects=rf['objects']))
            dead_source = ir(target, layout, 4096, True).replace(' %input = alloca', ' %dead = alloca [16 x i8], align 4\n %input = alloca', 1)
            dead, _, _ = emit(dead_source, mode, base + '/dead-home')
            df = dead['functions']['entry']
            assert df['P'] == 49168
            controls.append(dict(target=target, mode=mode, control='dead home remains charged', P=df['P'], R=df['R'], C=df['C']))
            omitted, _, _ = emit(ir(target, layout, 4096, True), mode, base + '/omitted-inventory', accepted=False, fault='omit')
            assert any('unmatched allocation' in e for es in omitted['errors'].values() for e in es)
            controls.append(dict(target=target, mode=mode, control='omitted inventory home', rejected=True))
            if target == native:
                encoded, _, _ = emit(ir(target, layout, 4096, True), mode, base + '/late-encoded', accepted=False, fault='encoded')
                assert any('encoded SP writes differ' in e for es in encoded['errors'].values() for e in es)
                assert not any('changed during final emission' in e for es in encoded['errors'].values() for e in es)
                controls.append(dict(target=target, mode=mode, control='encoded object mutation after machine checks', rejected=True))
            for fault in ('frame', 'sp'):
                rejected, _, _ = emit(ir(target, layout, 4096, True), mode, base + '/late-' + fault, accepted=False, fault=fault)
                assert any('changed during final emission' in e for es in rejected['errors'].values() for e in es)
                if fault == 'sp':
                    assert any('encoded SP writes differ' in e for es in rejected['errors'].values() for e in es)
                controls.append(dict(target=target, mode=mode, control='late ' + fault + ' mutation', rejected=True))
    (output / 'controls.json').write_text(json.dumps(controls, indent=2) + '\n')
    if any((here / p).read_bytes() != data for p, data in sources.items()):
        raise RuntimeError('investigation sources changed during execution')
    (output / 'sources.sha256').write_text(''.join(hashlib.sha256(data).hexdigest() + '  ' + p + '\n' for p, data in sources.items()))
    (output / 'complete.json').write_text(json.dumps(dict(passed=True, cases=len(results), controls=len(controls),
                                                        commands=len(commands)), indent=2) + '\n')
    print('Machine proof complete. Production emitter, ABI and ordinary CI unchanged.')


if __name__ == '__main__':
    main()
