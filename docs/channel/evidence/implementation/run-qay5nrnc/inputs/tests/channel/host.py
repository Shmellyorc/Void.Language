from pathlib import Path
import sys
import support as h

exe, root, work = map(lambda x: Path(x).resolve(), sys.argv[1:])
work.mkdir(parents=True, exist_ok=True)
result = h.run([exe, root, work], cwd=work, timeout=600)
print(result.stdout.decode(), end='')
manifest = (exe.parent / 'void-native.txt').read_text().splitlines()
clang, linker, runtime, fixture = manifest[1:5]
flags = ['--driver-mode=g++', '-pthread', '--ld-path=' + linker]
if sys.platform.startswith('linux'):
    flags += ['-ldl']
if 'san' in exe.parent.name:
    flags += ['-fsanitize=address,undefined']
if sys.platform == 'win32':
    flags += ['-fms-runtime-lib=dll', '-llegacy_stdio_definitions']
for mode in ('o0', 'o2'):
    native = work / ('async-' + mode + ('.exe' if sys.platform == 'win32' else ''))
    h.run([clang, work / ('async-' + mode + '.o'), runtime, fixture, *flags, '-o', native])
    h.run([native, '7'], expected=b'7\n', small=sys.platform != 'win32')
print('Channel owner-async independently linked O0/O2 AOT passed')
