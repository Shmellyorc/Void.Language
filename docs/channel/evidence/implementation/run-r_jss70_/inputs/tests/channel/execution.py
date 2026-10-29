"""Normal/O2 JIT, independently linked AOT, launch/cache isolation and hard stacks."""
from pathlib import Path
import os
import sys
import tempfile
import support as h

compiler = Path(sys.argv[1]).resolve()
small = '--small-stack' in sys.argv
manifest = (compiler.parent / 'void-native.txt').read_text().splitlines()
clang, linker, runtime, fixture = manifest[1:5]
flags = ['--driver-mode=g++', '-pthread', '--ld-path=' + linker]
if sys.platform.startswith('linux'):
    flags += ['-ldl']
if 'san' in compiler.parent.name:
    flags += ['-fsanitize=address,undefined']
if os.name == 'nt':
    flags += ['-fms-runtime-lib=dll', '-llegacy_stdio_definitions']
checks = 0
with tempfile.TemporaryDirectory(prefix='VOID-channel-execution-') as directory:
    work = Path(directory)
    sources = [(h.ROOT / 'tests/channel' / (name + '.void'), b'7\n') for name in ('semantics', 'payloads', 'nested')]
    sources += [(h.ROOT / 'examples/channel/local.void', b'7\n7\nfalse\ntrue\nfalse\n8\nfalse\n'),
                (h.ROOT / 'examples/channel/global.void', b'Damage: 25\ntrue\n')]
    numeric = work / 'numeric.void'
    numeric.write_text('''use channel
func exercise<t>(const value: t):
    var bus = channel.router<t>()
    var calls = 0
    bus.subscribe("t", func(const payload: t):
        calls += 1
    )
    bus.emit("t", value)
    if calls != 1:
        var zero = 0
        var failed = 1 / zero
func main():
''' + ''.join('    exercise<' + kind + '>(' + value + ')\n' for kind, value in
              [(x, '7') for x in ('i8','u8','i16','u16','i32','u32','i64','u64','f32','f64')]
              + [('bool', 'true'), ('[i32, 2]', '[1, 2]')]) + '    print(7)\n')
    sources.append((numeric, b'7\n'))
    for index, (source, expected) in enumerate(sources):
        arguments = ['7'] if 'func entry(' in source.read_text() else []
        h.run([compiler, 'check', source], small=small)
        for optimize in (False, True):
            switches = [] if optimize else ['--no-opt']
            h.run([compiler, 'run', source, *arguments, '--no-cache', *switches], expected=expected, small=small)
            obj, exe = work / 'app.o', work / ('app.exe' if os.name == 'nt' else 'app')
            h.run([compiler, 'emit', source, '-o', obj, *switches], small=small)
            h.run([clang, obj, runtime, fixture, *flags, '-o', exe])
            h.run([exe, *arguments], expected=expected, small=small)
            checks += 4
    # Automatic .void caching must create code artifacts and fresh Runtime state.
    source = work / 'cached.void'
    original = (h.ROOT / 'tests/channel/reload.void').read_text()
    source.write_text(original.replace('func entry(command: i32) -> i32:', 'func exercise(command: i32) -> i32:') +
                      'func main():\n    exercise(0)\n    print(exercise(1))\n')
    cache = work / '.void/cache/v1'
    for optimize in (True, False):
        switches = [] if optimize else ['--no-opt']
        before = set(cache.iterdir()) if cache.exists() else set()
        h.run([compiler, 'run', source, *switches], expected=b'1\n', small=small)
        artifacts = set(cache.iterdir()) if cache.exists() else set()
        if os.name != 'nt':
            assert len(artifacts - before) == 1, 'cache did not miss/publish once'
        frozen = {p: (p.stat().st_mtime_ns, p.read_bytes()) for p in artifacts}
        h.run([compiler, 'run', source, *switches], expected=b'1\n', small=small)
        assert all((p.stat().st_mtime_ns, p.read_bytes()) == value for p, value in frozen.items()), 'cache hit republished code'
        h.run([compiler, 'run', source, '--no-cache', *switches], expected=b'1\n', small=small)
        checks += 3
    # Ordinary configured standard-source discovery also works outside this checkout.
    relocated = work / 'installed-stdlib'
    relocated.mkdir()
    (relocated / 'channel.void').write_bytes((h.ROOT / 'stdlib/channel.void').read_bytes())
    installed = work / 'installed.void'
    installed.write_text('use channel\nfunc main():\n    var bus = channel.router<i32>()\n    bus.subscribe("t", func(const amount: i32):\n        print(amount)\n    )\n    bus.emit("t", 7)\n')
    env = dict(os.environ); env['VOID_STDLIB_ROOT'] = str(relocated)
    h.run([compiler, 'check', installed], env=env, small=small)
    h.run([compiler, 'run', installed, '--no-cache'], env=env, expected=b'7\n', small=small)
    checks += 2
print(f'Channel execution: {checks} JIT/link/native/cache checks; hard stack={1048576 if small else "default"}')
