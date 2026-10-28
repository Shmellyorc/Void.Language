"""Real source checking, JIT/O2 and independently linked AOT on hard stacks."""
from pathlib import Path
import os
import sys
import tempfile
import support as h
compiler = Path(sys.argv[1]).resolve()
small = '--small-stack' in sys.argv
selected = [x for x in sys.argv[2:] if not x.startswith('--')]
fixtures = selected or ['authority', 'semantics', 'mutation', 'types', 'numeric', 'large']
manifest = (compiler.parent / 'void-native.txt').read_text().splitlines()
clang, linker, runtime, native_fixture = manifest[1:5]
flags = ['--driver-mode=g++', '-pthread', '--ld-path=' + linker]
if sys.platform.startswith('linux'):
    flags += ['-ldl']
if 'san' in compiler.parent.name:
    flags += ['-fsanitize=address,undefined']
if os.name == 'nt':
    flags += ['-fms-runtime-lib=dll', '-llegacy_stdio_definitions']
checks = 0
with tempfile.TemporaryDirectory(prefix='VOID-collections-execution-') as directory:
    work = Path(directory)
    for name in fixtures:
        source = Path(__file__).with_name(name + '.void')
        arguments = ['7'] if 'func entry(' in source.read_text() else []
        h.run([compiler, 'check', source], small=small)
        for optimize in (False, True):
            switches = [] if optimize else ['--no-opt']
            context = {'mode': 'o2' if optimize else 'jit', 'fixture': name}
            h.run([compiler, 'run', source, *arguments, '--no-cache', *switches], expected=b'7\n', small=small, context=context)
            obj = work / (name + '.o')
            exe = work / (name + ('.exe' if os.name == 'nt' else '.app'))
            h.run([compiler, 'emit', source, '-o', obj, *switches], small=small, context={'mode': 'aot-o2' if optimize else 'aot'})
            h.run([clang, obj, runtime, native_fixture, *flags, '-o', exe], sources=[source])
            h.run([exe, *arguments], expected=b'7\n', small=small, sources=[source], context={'mode': 'aot-o2' if optimize else 'aot'})
            checks += 4
print(f'Collections: {checks} JIT/emit/link/native checks; hard stack={1048576 if small else "default"}')
