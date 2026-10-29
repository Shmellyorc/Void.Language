from pathlib import Path
import sys
import support as h

exe, root, work = map(lambda x: Path(x).resolve(), sys.argv[1:])
work.mkdir(parents=True, exist_ok=True)
result = h.run([exe, root, work], cwd=work, timeout=600)
print(result.stdout.decode(), end='')
for mode in ('o0', 'o2'):
    native = work / ('async-' + mode + ('.exe' if sys.platform == 'win32' else ''))
    h.link(exe, work / ('async-' + mode + '.o'), native)
    h.run([native, '7'], expected=b'7\n', small=sys.platform != 'win32')
print('Channel owner-async independently linked O0/O2 AOT passed')
