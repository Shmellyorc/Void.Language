"""Exercise existing AOT producers without enabling public certification.

Native Linux witness. Integration CMake scripts run unchanged through a compiler
capture wrapper: their original output/status is authoritative, while a separate
staged attempt records success or rejection. Rejected objects are never linked.
Family lists are read from CMake, not copied here. Library/reload modules use
their original fixture producers with an explicit test-only final argument.
"""
import argparse
import hashlib
import json
import platform
from pathlib import Path
import re
import shlex
import subprocess
import sys


def run(argv, cwd=None):
    return subprocess.run([str(x) for x in argv], cwd=cwd, capture_output=True,
                          text=True, encoding='utf-8', errors='strict', timeout=240)


def attempt(config, command, original, kind, source, mode):
    out = Path(config['out'])
    records = out / 'records.jsonl'
    index = len(records.read_text().splitlines()) if records.exists() else 0
    prefix = out / f'{index:04d}-{kind}-{mode}'
    obj = Path(str(prefix)+'.o')
    # Existing destinations must survive rejection. The sentinel is never a
    # valid native object, and is checked before any linking operation.
    obj.write_bytes(b'owner destination\n')
    argv = [str(x).replace('@OBJECT@', str(obj)) for x in command]
    result = run(argv)
    prefix.with_suffix('.log').write_text(result.stdout+result.stderr)
    cert_path = Path(str(obj)+'.certificate.json')
    cert = json.loads(cert_path.read_text()) if cert_path.exists() and cert_path.stat().st_size else None
    if result.returncode == 0:
        if not cert or not cert['certified_delivery']:
            raise RuntimeError('successful emission without certificate')
        def digest(path):
            return hashlib.sha256(path.read_bytes()).hexdigest().upper()
        if digest(obj) != cert['object_sha256']:
            raise RuntimeError('object digest mismatch')
        for suffix, field in (('.entry.ll', 'frozen_module_sha256'),
                              ('.optimized.ll', 'optimized_module_sha256')):
            if digest(Path(str(obj)+suffix)) != cert[field]:
                raise RuntimeError('module digest mismatch: '+field)
        if hashlib.sha256(cert['inventory'].encode('utf-8')).hexdigest().upper() != cert['inventory_sha256']:
            raise RuntimeError('inventory digest mismatch')
        profile = json.loads(cert['profile'])
        if profile['llvm'] != '19.1.7' or profile['ir_pipeline'] != (
                'verify-only' if mode == 'normal' else 'default<O2>;PassBuilder(no-TM)'):
            raise RuntimeError('profile mismatch')
    elif result.returncode != 1 or obj.read_bytes() != b'owner destination\n':
        raise RuntimeError('unexpected crash or damaged rejection destination: '+str(prefix))
    record = dict(index=index, kind=kind, source=str(source), mode=mode,
                  command=argv, original=str(original), object=str(obj),
                  accepted=result.returncode == 0, certificate=str(cert_path),
                  error=result.stderr if result.returncode else '')
    with records.open('a') as f:
        f.write(json.dumps(record)+'\n')
    return record


def capture(config, args):
    result = run([config['compiler'], *args])
    sys.stdout.write(result.stdout)
    sys.stderr.write(result.stderr)
    if result.returncode == 0 and args and args[0] in ('emit', '--emit-proof'):
        architecture = args[0] == '--emit-proof'
        source = 'architecture_proof()' if architecture else str(Path(args[1]).resolve())
        original = args[1] if architecture else args[args.index('-o')+1]
        # Also exercise both profiles for the two integration fixtures whose
        # ordinary scripts emit only O2 (architecture proof and edited source).
        modes = ('normal', 'O2') if architecture or Path(original).name == 'edited.o' else (
            'normal' if '--no-opt' in args else 'O2',)
        for mode in modes:
            command = [config['audit'], '--emit-architecture-proof', '@OBJECT@', mode] if architecture else [
                config['audit'], '--emit-proof', source, '@OBJECT@', mode]
            attempt(config, command, Path(original).resolve(), 'integration', source, mode)
    return result.returncode


def main():
    if len(sys.argv)>1 and sys.argv[1] == '--capture':
        return capture(json.loads(Path(sys.argv[2]).read_text()), sys.argv[3:])
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build', type=Path, required=True)
    p.add_argument('--root', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--cxx', type=Path, required=True)
    a = p.parse_args()
    if platform.system() != 'Linux' or platform.machine() != 'x86_64':
        p.error('native Linux x64 only; no cross-target execution claim')
    build, root, out = a.build.resolve(), a.root.resolve(), a.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    (out/'complete.json').write_text('{"complete": false}\n')
    config = dict(out=str(out), compiler=str(build/'voidc'), audit=str(build/'void_arrays_storage_audit'))
    config_path = out/'config.json'
    config_path.write_text(json.dumps(config))
    wrapper = out/'capture-compiler'
    wrapper.write_text('#!'+sys.executable+'\nimport subprocess,sys\nsys.exit(subprocess.call('+repr([
        sys.executable, str(Path(__file__).resolve()), '--capture', str(config_path)])+'+sys.argv[1:]))\n')
    wrapper.chmod(0o755)
    tests = json.loads(run(['ctest','--test-dir',build,'--show-only=json-v1']).stdout)['tests']
    # These are all integration scripts that emit production objects. Family
    # CLI tests run prebuilt outputs, whose producers are enumerated below.
    scripts = [t for t in tests if any(x.startswith('-DCOMPILER=') for x in t['command'])]
    for test in scripts:
        command = [('-DCOMPILER='+str(wrapper) if x.startswith('-DCOMPILER=') else
                    '-DOUTPUT='+str(out/test['name']) if x.startswith('-DOUTPUT=') else x)
                   for x in test['command']]
        result = run(command)
        (out/(test['name']+'.log')).write_text(result.stdout+result.stderr)
        if result.returncode:
            raise RuntimeError('original integration script failed: '+test['name'])
        print(test['name'], 'original checks and staged attempts complete', flush=True)
    families = re.findall(r'void_managed_source_tests\((\w+) (\w+) (\w+) "([^"]+)"\)',
                          (root/'CMakeLists.txt').read_text())
    if len(families)!=7:
        raise RuntimeError('family producer coverage changed; review manifest')
    replacements = {}
    for family, folder, library, programs in families:
        for mode, native_mode in (('normal','normal'),('O2','optimized')):
            source = root/'examples'/folder/(library+'.void')
            original = build/f'{family}-library-{native_mode}.o'
            record = attempt(config,[build/f'void_{family}_tests','--emit-library',source,'@OBJECT@',
                                      native_mode,'--storage-proof'],original,'library',source,mode)
            replacements[str(original)] = record
            if family in ('inheritance','interfaces','strings'):
                original = build/f'{family}-reloaded-{native_mode}.o'
                record = attempt(config,[build/f'void_{family}_tests','--emit-reload',source,'@OBJECT@',
                                          native_mode,'--storage-proof'],original,'reload',source,mode)
                replacements[str(original)] = record
            for program in programs.split(';'):
                source = root/'examples'/folder/(program+'.void')
                original = out/f'{family}-{program}-{mode}-original.o'
                result = run([build/'voidc','emit',source,'-o',original,
                              *(['--no-opt'] if mode=='normal' else [])])
                if result.returncode:
                    raise RuntimeError('original family CLI emission failed: '+str(source))
                attempt(config,[config['audit'],'--emit-proof',source,'@OBJECT@',mode],
                        original,'cli',source,mode)
        print(family, 'all CLI/library/reload producers complete', flush=True)
    for mode, native_mode in (('normal','normal'),('O2','optimized')):
        original = build/f'runtime-{native_mode}.o'
        record = attempt(config,[build/'void_runtime_generated','@OBJECT@',native_mode,'--storage-proof'],
                          original,'runtime','runtime_test::proof()',mode)
        replacements[str(original)] = record
    # Independently relink the existing native library harnesses with certified
    # objects only. Keep their compiled C++ callers, instrumentation and runtime.
    harnesses = []
    for family in [f[0] for f in families]+['runtime']:
        for mode in ('normal','optimized'):
            target = f'void_{family}_aot_{mode}'
            argv = shlex.split((build/'CMakeFiles'/(target+'.dir')/'link.txt').read_text())
            required = [replacements[str((build/arg).resolve())] for arg in argv
                        if str((build/arg).resolve()) in replacements]
            if not required:
                raise RuntimeError('no production objects found for '+target)
            if not all(r['accepted'] for r in required):
                harnesses.append(dict(target=target, status='rejected; not linked'))
                continue
            argv = [replacements[str((build/arg).resolve())]['object']
                    if str((build/arg).resolve()) in replacements else arg for arg in argv]
            exe = out/target
            argv[argv.index('-o')+1] = str(exe)
            # Do not overwrite the build's linker dependency file.
            argv = [x.replace('CMakeFiles/'+target+'.dir/link.d', str(out/(target+'.link.d'))) for x in argv]
            linked = run(argv, cwd=build)
            (out/(target+'-link.log')).write_text(linked.stdout+linked.stderr)
            if linked.returncode:
                raise RuntimeError('certified harness link failed: '+target)
            original = run([build/target], cwd=out)
            certified = run([exe], cwd=out)
            # Harness progress may contain timings/addresses. All existing
            # assertions and exit status remain authoritative for both runs.
            (out/(target+'-original.log')).write_text(original.stdout+original.stderr)
            (out/(target+'-certified.log')).write_text(certified.stdout+certified.stderr)
            if original.returncode or certified.returncode:
                raise RuntimeError('native harness assertion failed: '+target)
            harnesses.append(dict(target=target,status='both assertion harnesses passed'))
    records = [json.loads(x) for x in (out/'records.jsonl').read_text().splitlines()]
    comparisons = 0
    for r in records:
        if r['kind'] not in ('integration','cli') or not r['accepted']:
            continue
        binaries = []
        for kind, obj in (('original',r['original']),('certified',r['object'])):
            exe = out/f"{r['index']:04d}-{kind}-native"
            result = run([a.cxx,obj,build/'libvoid_runtime.a','-pthread','-ldl','-o',exe])
            if result.returncode:
                raise RuntimeError('CLI native link failed: '+result.stderr)
            binaries.append(exe)
        for args in ([],['0'],['1'],['10'],['oops'],['1','2']):
            values = [run([exe,*args]) for exe in binaries]
            if [(x.returncode,x.stdout,x.stderr) for x in values][0] != (
                    values[1].returncode,values[1].stdout,values[1].stderr) or values[0].returncode not in (0,1,2):
                raise RuntimeError('native observable mismatch: '+r['object'])
            comparisons += 1
    shapes, errors = {}, {}
    for r in records:
        path = Path(r['certificate'])
        if not path.exists() or not path.stat().st_size:
            continue
        cert = json.loads(path.read_text())
        for fn, f in cert['functions'].items():
            shape = dict(variable=f['has_variable_objects'], opaque=f['opaque_sp_adjustment'],
                         incoming=bool(f['incoming']), zero=bool(f['zero_extent_homes']),
                         unmatched=bool(f['unmatched_homes']),
                         cli_moves=bool(f['entry_home_moves']), lifetime=f['lifetime_no_credit'],
                         kinds=sorted(set(h['kind'] for h in f['immutable_homes'])))
            key = json.dumps(shape,sort_keys=True)
            entry = shapes.setdefault(key,dict(shape=shape,count=0,example=[r['index'],fn]))
            entry['count'] += 1
        for fn, failures in cert['errors'].items():
            for error in failures:
                errors.setdefault(error,[]).append([r['index'],fn])
    summary = dict(complete=True, attempts=len(records), accepted=sum(r['accepted'] for r in records),
                   rejected=sum(not r['accepted'] for r in records), comparisons=comparisons,
                   native_cli_runs=2*comparisons,harnesses=harnesses,
                   shapes=list(shapes.values()),errors=errors)
    (out/'complete.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps({k:v for k,v in summary.items() if k not in ('shapes','errors','harnesses')},indent=2))
    return 0


if __name__ == '__main__':
    sys.exit(main())
