"""Opt-in managed raylib 6.0 qualification using approved local vendor files."""

import argparse
from hashlib import sha256
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import tomllib

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('compiler', type=Path)
parser.add_argument('clang', type=Path)
parser.add_argument('vendor', type=Path)
parser.add_argument('sanitizers', choices=('ON', 'OFF'))
parser.add_argument('--graphics', action='store_true')
parser.add_argument('--package', action='store_true')
parser.add_argument('--exclude-confirmed-mesa-lsan', action='store_true')
args = parser.parse_args()
assert sys.platform.startswith('linux'), 'This fixture qualifies Linux x86-64'
assert not args.exclude_confirmed_mesa_lsan or (args.graphics and args.sanitizers == 'ON')
compiler, clang, vendor = (path.resolve() for path in (args.compiler, args.clang, args.vendor))
root = Path(__file__).resolve().parents[2]
environment = dict(os.environ)
for variable in ('VOID_NATIVE_CATALOG', 'VOID_STDLIB_ROOT', 'VOID_TEST_STDLIB'):
    environment.pop(variable, None)
if args.sanitizers == 'ON':
    environment.update(ASAN_OPTIONS='halt_on_error=1:abort_on_error=1:detect_leaks=1',
                       UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')


def digest(path):
    return sha256(path.read_bytes()).hexdigest()


def run(*command, status=0, message=None, env=None):
    result = subprocess.run(list(map(str, command)), cwd=cwd, env=environment if env is None else env,
                            capture_output=True, timeout=180)
    text = (result.stdout + result.stderr).decode(errors='replace')
    assert result.returncode == status, (command, result.returncode, text)
    assert 'AddressSanitizer' not in text and 'UndefinedBehaviorSanitizer' not in text, text
    assert message is None or message in text, (command, text)
    return text


def cli(command, *arguments, **options):
    return run(compiler, command, '--project', project, *arguments, **options)


def headless(command, env):
    env = {name: value for name, value in env.items() if name not in ('DISPLAY', 'WAYLAND_DISPLAY')}
    text = run(*command, '--trace-only', env=env, message='RAYLIB trace invoked VOID')
    assert text.count('RAYLIB trace invoked VOID') == 1, text
    assert 'RAYLIB ready' not in text and 'RAYLIB owner released' not in text, text
    run(*command, '--reject-title', status=2, env=env, message='Raylib title rejected')


# Official Linux release inputs retained from S7 qualification.
for relative, expected in {
    'include/raylib.h': '047e7255f93f8c34039cab906ad76136706b5c7b4c5b5b065d84141963ee9b6b',
    'lib/libraylib.so.6.0.0': '1041653dd5c1cb8c67494fa398a296520d3ffec20cf1bf71b1eaa4b96ac61aee',
    'LICENSE': '882a5a819cf562aa3583aae3af3f2211dda15c63de9fc8cc4b399a2f9e78d799',
}.items():
    assert digest(vendor / relative) == expected, f'Unapproved raylib 6.0 input: {relative}'

with tempfile.TemporaryDirectory(prefix='VOID managed raylib 雪 ') as temporary:
    base = Path(temporary).resolve()
    cwd = base / 'unrelated working directory'
    cwd.mkdir()
    project = base / 'gallery source'
    shutil.copytree(root / 'examples/raylib', project)
    catalog = base / 'approved catalog'
    run(sys.executable, '-B', root / 'verified-packages/raylib/prepare.py', '--gallery-only', '--voidc', compiler, '--clang', clang,
        '--header', vendor / 'include/raylib.h', '--library', vendor / 'lib/libraylib.so.6.0.0',
        '--license', vendor / 'LICENSE', '--managed-catalog', catalog,
        *(['--sanitize'] if args.sanitizers == 'ON' else []))
    manifest = project / 'void.project.toml'
    initial = manifest.read_text().split('[native.linux]', 1)[0]
    manifest.write_text(initial + '[native-requirements.raylib]\nid="org.voidlang.raylib"\n'
                        'version="=6.0.0"\nabi=3\n', encoding='utf-8')
    environment['VOID_NATIVE_CATALOG'] = str(catalog)
    cli('resolve')
    lock = project / 'void.lock'
    locked, modified = lock.read_bytes(), lock.stat().st_mtime_ns
    plan = tomllib.loads(locked.decode())['native']
    assert plan['id'] == 'org.voidlang.raylib' and plan['version'] == '6.0.0' and plan['abi'] == 3
    assert plan['target'] == 'x86_64-unknown-linux-gnu'
    assert plan['runtime']['libraylib.so.600'].endswith(digest(vendor / 'lib/libraylib.so.6.0.0'))
    cli('resolve', '--locked')
    cli('check')
    assert (locked, modified) == (lock.read_bytes(), lock.stat().st_mtime_ns)
    cli('run', status=2, message='--allow-native')

    for name in ('libvoid_raylib.so', 'libraylib.so.600'):
        artifact = catalog / name
        original = artifact.read_bytes()
        artifact.write_bytes(original[:-1] + bytes([original[-1] ^ 1]))
        text = cli('run', '--allow-native', '--no-cache', '--', '--trace-only', status=1,
                   message='artifact identity mismatch')
        assert 'RAYLIB trace invoked VOID' not in text
        artifact.unlink()
        text = cli('run', '--allow-native', '--no-cache', '--', '--trace-only', status=1,
                   message='Missing managed native artifact')
        assert 'RAYLIB trace invoked VOID' not in text
        artifact.write_bytes(original)
    assert locked == lock.read_bytes()

    graphics_environment = dict(environment)
    if args.exclude_confirmed_mesa_lsan:
        graphics_environment['ASAN_OPTIONS'] = 'halt_on_error=1:abort_on_error=1:detect_leaks=0'
    if args.graphics:
        from raylib_graphics import qualify

    packages = []
    for profile in ('debug', 'release'):
        jit = [compiler, 'run', '--project', project, '--allow-native', '--no-cache', '--profile', profile, '--']
        headless(jit, environment)
        if args.graphics:
            print('Visible JIT', profile, flush=True)
            qualify([*jit, *(['--implicit-close'] if profile == 'debug' else [])], catalog, cwd,
                    graphics_environment, base / ('jit-' + profile))
        output = base / ('linked-' + profile) / 'gallery'
        if args.package:
            cli('package', '--allow-native', '--profile', profile, '-o', output.parent)
            output = output.parent / 'app'
            run(compiler, 'package', '--verify', output.parent)
        else:
            cli('build', '--allow-native', '--profile', profile, '-o', output)
        needed = run(clang.with_name('llvm-readelf'), '-d', output)
        assert not re.search(r'Shared library: \[(?:libvoid_raylib|libraylib)\.', needed), needed
        native = output.parent / 'native'
        assert {p.name for p in native.iterdir()} == {'libvoid_raylib.so', 'libraylib.so.600', 'RAYLIB-LICENSE.txt'}
        for file in native.iterdir():
            assert digest(file) == digest(catalog / file.name)
        relocated = base / ('relocated ' + profile + ' 雪')
        output.parent.rename(relocated)
        packages.append((profile, relocated, output.name))

    project.rename(base / 'parked source')
    catalog.rename(base / 'parked catalog')
    app_environment = dict(environment, PATH='/usr/bin:/bin', VOID_NATIVE_CATALOG=str(base / 'absent catalog'),
                           VOID_STDLIB_ROOT=str(base / 'absent stdlib'))
    bubblewrap = shutil.which('bwrap')
    assert bubblewrap, 'bwrap is required to verify AOT with the checkout and compiler unavailable'
    isolation = [bubblewrap, '--ro-bind', '/', '/', '--dev-bind', '/dev', '/dev', '--proc', '/proc',
                 '--tmpfs', root, '--tmpfs', compiler.parent, '--bind', base, base, '--chdir', cwd, '--']
    run(*isolation, '/usr/bin/test', '!', '-e', root / 'CMakeLists.txt', env=app_environment)
    run(*isolation, '/usr/bin/test', '!', '-e', compiler, env=app_environment)
    for profile, package, executable in packages:
        command = [*isolation, package / executable]
        headless(command, app_environment)
        if args.graphics:
            print('Visible relocated AOT', profile, flush=True)
            env = dict(app_environment)
            if args.exclude_confirmed_mesa_lsan:
                env['ASAN_OPTIONS'] = graphics_environment['ASAN_OPTIONS']
            qualify([*command, *(['--implicit-close'] if profile == 'release' else [])],
                    package / 'native', cwd, env, base / ('aot-' + profile))
        for name in ('libvoid_raylib.so', 'libraylib.so.600'):
            artifact = package / 'native' / name
            original = artifact.read_bytes()
            artifact.write_bytes(original[:-1] + bytes([original[-1] ^ 1]))
            text = run(*command, '--trace-only', status=1, env=app_environment,
                       message='managed native payload identity mismatch')
            assert 'RAYLIB trace invoked VOID' not in text
            artifact.unlink()
            text = run(*command, '--trace-only', status=1, env=app_environment,
                       message='managed native payload missing or changed')
            assert 'RAYLIB trace invoked VOID' not in text
            artifact.write_bytes(original)

print('Managed raylib headless JIT/AOT, isolated relocation and artifact checks passed', flush=True)
if args.graphics:
    print('Managed raylib visible drawing, input, callbacks and explicit/implicit cleanup passed' +
          (' with confirmed Mesa leak detection excluded for graphics' if args.exclude_confirmed_mesa_lsan else ''), flush=True)
