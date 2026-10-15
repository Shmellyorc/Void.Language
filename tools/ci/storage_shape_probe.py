"""Native Linux comparison of the exact staged and unchanged public AOT fixtures.

No source native arrays, CI activation or alternate IR producer. The compiler
fixture entry reuses the public AOT module/adapter producer and the staged auditor.
"""
import argparse
import json
from pathlib import Path
import platform
import resource
import subprocess


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build', required=True, type=Path)
    p.add_argument('--root', required=True, type=Path)
    p.add_argument('--cxx', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    p.add_argument('--llvm-bin', required=True, type=Path)
    a = p.parse_args()
    if platform.system() != 'Linux' or platform.machine() != 'x86_64':
        p.error('this execution witness requires native Linux x64')
    build, root, cxx, out = a.build.resolve(), a.root.resolve(), a.cxx.absolute(), a.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    commands, results = [], []
    (out / 'complete.json').write_text('{"passed": false}\n')

    def run(argv, label, stack=None, expected=0):
        def limit():
            resource.setrlimit(resource.RLIMIT_STACK, (stack, stack))
        r = subprocess.run([str(x) for x in argv], text=True, encoding='utf-8', errors='strict',
                           capture_output=True, timeout=90, preexec_fn=limit if stack else None)
        (out / (label + '.log')).write_text(r.stdout + r.stderr)
        commands.append(dict(argv=[str(x) for x in argv], code=r.returncode, stack=stack, log=label+'.log'))
        (out / 'commands.json').write_text(json.dumps(commands, indent=2)+'\n')
        if expected is not None and r.returncode != expected:
            raise RuntimeError(f'{label}: expected {expected}, got {r.returncode}: {r.stderr}')
        return r.returncode, r.stdout, r.stderr

    run([a.llvm_bin / 'llvm-objdump', '--version'], 'objdump-version')
    arguments = [[], ['10'], ['-4'], ['oops'], ['1', '2'], ['2147483648'], ['-2147483649'], ['2.0']]
    for mode in ('normal', 'O2'):
        for name in ('main', 'handoffs', 'null_failure'):
            source = root / 'examples/managed_structs' / (name+'.void')
            case = name+'-'+mode
            binaries = []
            for kind in ('original', 'certified'):
                label = case+'-'+kind
                obj, exe = out / (label+'.o'), out / label
                if kind == 'original':
                    run([build/'voidc', 'emit', source, '-o', obj, *(['--no-opt'] if mode=='normal' else [])],label+'-emit')
                else:
                    run([build/'void_arrays_storage_audit','--emit-proof',source,obj,mode],label+'-emit')
                    run([a.llvm_bin/'llvm-objdump','-d',obj],label+'-disassembly')
                    cert=json.loads(Path(str(obj)+'.certificate.json').read_text())
                    if not cert['certified_delivery']:
                        raise RuntimeError('no certified delivery')
                    for f in cert['functions'].values():
                        if f['has_variable_objects'] or f['opaque_sp_adjustment']:
                            raise RuntimeError('dynamic stack accepted')
                run([cxx,obj,build/'libvoid_runtime.a','-pthread','-ldl','-o',exe],label+'-link')
                binaries.append(exe)
            comparisons=0
            for n,args in enumerate(arguments):
                for stack in (None,1048576):
                    values=[run([exe,*args],case+f'-{n}-{stack}-'+str(i),stack=stack,expected=None) for i,exe in enumerate(binaries)]
                    if values[0] != values[1] or values[0][0] not in (0,1,2):
                        raise RuntimeError('observable CLI behavior differs or unexpected crash: '+case)
                    comparisons+=1
            results.append(dict(case=case,comparisons=comparisons,identical=True))
            (out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
            print(case,comparisons,'comparisons passed',flush=True)
    (out/'complete.json').write_text(json.dumps(dict(passed=True,comparisons=sum(r['comparisons'] for r in results),
                                                   native_runs=sum(r['comparisons']*2 for r in results)),indent=2)+'\n')


if __name__ == '__main__':
    main()
