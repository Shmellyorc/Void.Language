from hashlib import file_digest
from pathlib import Path
from xml.etree import ElementTree
import json
import subprocess
import sys

root = Path('docs/projects/evidence/slice7-g4-linux/linux')
profiles = sys.argv[1:] or ('release','debug','strict')
assert set(profiles) <= {'release','debug','strict'}
for profile in profiles:
    build = Path('build')/f'llvm23-{profile}'
    bundle = build/'ffi-g4'
    result = ElementTree.parse(root/f'{profile}-ctest.xml').getroot()
    assert tuple(result.get(k) for k in ('tests','failures','disabled','skipped')) == ('52','0','0','0')
    names = {case.get('name') for case in result}
    assert {'ffi_g1_primitive_jit_ir','ffi_g2_jit_ir','ffi_g3_jit_ir','ffi_g4_jit_ir',
            'ffi_g4_aot_normal','ffi_g4_aot_optimized','ffi_g4_aot_startup'} <= names
    last = (root/f'{profile}-LastTest.txt').read_text()
    for marker in ('G4 normal JIT returned 104', 'G4 O2 JIT returned 104',
                   'normal verified relocated dynamic AOT with its compile source removed',
                   'optimized verified relocated dynamic AOT with its compile source removed',
                   'startup verified relocated dynamic AOT with its compile source removed',
                   'real initializer would exit 121 if mapped',
                   'new Runtime after on-disk dependency replacement',
                   'different closure in live Runtime: C dynamic closure is incompatible',
                   'null and refused acquisition, refused release and retry preserved exact ownership'):
        assert marker in last,(profile,marker)
    assert last.count('verified sealed SHA-256') == 4

    files = [build/name for name in ('void_g4_tests','void_g4_aot_normal','void_g4_aot_optimized',
             'void_g4_aot_startup','void_g1_tests','void_g2_tests','void_g3_tests')]
    files += [bundle/name for name in ('closure.txt','dynamic.descriptor','g4_descriptor.h',
              'native/libvoid_g4_main.so','native/libvoid_g4_dep.so','outside/libvoid_g4_dep.so',
              'initializer/libvoid_g4_dep.so')]
    for mode in ('normal','optimized','startup'):
        files += [bundle/f'application-{mode}.o',bundle/f'application-{mode}.o.source.txt']
    with (root/f'{profile}-artifacts.sha256').open('w') as output:
        for file in files:
            with file.open('rb') as raw:
                output.write(f'{file_digest(raw,"sha256").hexdigest()}  {file}\n')
    (root/f'{profile}-closure.txt').write_bytes((bundle/'closure.txt').read_bytes())
    sections = []
    for image in files:
        if image.suffix == '.so' or image.name.startswith('void_g4_aot_'):
            raw = subprocess.check_output(['readelf','-W','-l','-d','-r','--dyn-syms',image],text=True)
            sections.extend((str(image),raw))
            if image.name.startswith('void_g4_aot_'):
                assert '(RPATH)' not in raw and '(RUNPATH)' not in raw and 'libvoid_g4_' not in raw
                symbols = subprocess.check_output(['nm',image],text=True).splitlines()
                assert not any(line.split()[-1:] == ['vd_c_primitive_dispatch_v3'] for line in symbols)
                os_links = subprocess.check_output(['ldd',image],text=True)
                assert str(Path.cwd()) not in os_links
                sections.extend(('Trusted host executable dependencies',os_links))
    (root/f'{profile}-native-linkage.txt').write_text('\n'.join(sections))
    compile_flags = {}
    for target in ('void_g4_main','void_g4_dep','void_g4_tests','void_runtime','void_llvm'):
        file = build/'CMakeFiles'/f'{target}.dir'/'flags.make'
        compile_flags[target] = [line for line in file.read_text().splitlines()
                                 if line.startswith(('C_FLAGS =','CXX_FLAGS ='))]
    if profile == 'strict':
        for name in ('void_g4_main','void_g4_dep'):
            assert '-fsanitize-trap=undefined' in ' '.join(compile_flags[name])
        for name in ('void_g4_tests','void_runtime','void_llvm'):
            assert '-fsanitize=address,undefined' in ' '.join(compile_flags[name])
    (root/f'{profile}-compile-flags.json').write_text(json.dumps(compile_flags,indent=2)+'\n')
    print(f'{profile}: {len(names)} qualified cases, {len(files)} verified artifact hashes')
