"""Exercise installed project tooling and independently compiled OS-loaded libraries."""
from contextlib import nullcontext
from hashlib import sha256
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8')
cmake, build, config, clang = sys.argv[1:5]
sanitized = sys.argv[5] == 'ON'
sources = Path(__file__).resolve().parent
windows = os.name == 'nt'
exe = '.exe' if windows else ''
environment = dict(os.environ)
environment.pop('VOID_STDLIB_ROOT', None)
if sanitized:
    environment.update(ASAN_OPTIONS='halt_on_error=1:abort_on_error=1:detect_leaks=1',
                       UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')


def run(args, cwd, status=0, contains=None):
    argv = list(map(str, args))
    print(f'cwd={cwd} argv={argv}', flush=True)
    proc = subprocess.run(argv, cwd=cwd, env=environment, capture_output=True, timeout=180)
    output = (proc.stdout + proc.stderr).decode('utf-8', errors='replace')
    print(f'exit={proc.returncode} {output}', flush=True)
    if status is None:
        assert proc.returncode != 0, (args, output)
    else:
        assert proc.returncode == status, (args, proc.returncode, output)
    if contains:
        assert contains in output, (contains, output)
    assert 'AddressSanitizer' not in output and 'UndefinedBehaviorSanitizer' not in output
    return output


def write(path, text):
    path.write_bytes(text.encode('utf-8'))


def digest(path):
    return sha256(path.read_bytes()).hexdigest()


def verify(root):
    cwd = root / 'unrelated cwd'
    cwd.mkdir()
    prefix = root / 'installed compiler'
    run([cmake, '--install', build, '--prefix', prefix, '--config', config, '--component', 'compiler'], cwd)
    compiler = prefix / 'bin' / ('voidc' + exe)
    project = root / 'sources outside checkout 雪'
    run([compiler, 'new', project, '--source', 'src/main.void', '--project-id', 'org.test.native-application',
         '--app-id', 'org.test.native-app'], cwd)
    native = project / 'native'
    native.mkdir()
    stem, suffix = ('', '.dll') if windows else ('lib', '.so')
    dependency, library, adapter = [native / (stem + 'void_g4_conventional_' + part + suffix)
                                    for part in ('dependency', 'provider', 'adapter')]
    descriptor = native / 'provider.descriptor'
    target = 'x86_64-pc-windows-msvc msvc-x64' if windows else 'x86_64-unknown-linux-gnu sysv-x64'
    long_type = 'i32' if windows else 'i64'
    values = {'TARGET': target, 'LONG': long_type, 'THUNK': digest(sources / 'conventional_rejection.c'),
              'FIXTURE': digest(sources / 'conventional_fixture.c')}
    contract = (sources / 'conventional.descriptor.in').read_text(encoding='utf-8')
    for key, value in values.items():
        contract = contract.replace(f'@{key}@', value)
    write(descriptor, contract)
    with (project / 'void.project.toml').open('a', encoding='utf-8') as manifest:
        platform = 'windows' if windows else 'linux'
        manifest.write(f'\n[native.{platform}]\ndescriptor = "{descriptor.name}"\n'
                       f'library = "{library.name}"\nadapter = "{adapter.name}"\n')
    long_arg, long_result = ('-200', '-193') if windows else ('4000000000 as i64', '4000000007 as i64')
    source = (sources / 'conventional.void.in').read_text(encoding='utf-8')
    write(project / 'src/main.void', source.replace('@LONG_ARG@', long_arg).replace('@LONG_RESULT@', long_result))

    selector = ['--project', project]
    marker = cwd / 'native-init.log'
    released = cwd / 'native-release.log'
    run([compiler, 'check', *selector], cwd)
    run([compiler, 'tidy', '--check', *selector], cwd)
    run([compiler, 'run', *selector, '--', 'ignored'], cwd, 2, 'Pass --allow-native')
    assert not marker.exists()
    run([compiler, 'run', *selector, '--allow-native', '--', 'ignored'], cwd, 1, 'C dynamic library')

    build_c = root / 'independent C sources'
    build_c.mkdir()
    for name in ('conventional_fixture.h', 'conventional_fixture.c',
                 'conventional_dependency.cpp', 'conventional_rejection.c'):
        shutil.copy2(sources / name, build_c / name)
    generated = build_c / 'adapter.c'
    run([compiler, 'native-bind', descriptor, 'conventional_fixture.h', '-o', generated], cwd)
    # Compile from independent C/C++ inputs using normal CRT and DLL/DSO imports.
    flags = ['-fuse-ld=lld', '-shared', '-O2', '-Wall', '-Wextra', '-Werror']
    if windows:
        flags += ['--target=x86_64-pc-windows-msvc', '-fms-runtime-lib=static', '-D_CRT_SECURE_NO_WARNINGS']
    else:
        flags += ['-fPIC', '-fvisibility=hidden', '-Wl,-rpath,$ORIGIN']
        if sanitized:
            flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all']
    def link(output, input_file, *imports, cxx=False, status=0, contains=None):
        options = ['--driver-mode=g++', '-std=c++17'] if cxx else ['-std=c11']
        if windows:
            options += ['-Wl,/implib:' + str(build_c / (output.stem + '.lib'))]
            imports = tuple(build_c / (item.stem + '.lib') for item in imports)
        else:
            options += ['-Wl,-soname,' + output.name]
        return run([clang, *options, *flags, input_file, *imports, '-o', output], build_c, status, contains)

    link(dependency, build_c / 'conventional_dependency.cpp', cxx=True)
    link(library, build_c / 'conventional_fixture.c', dependency)
    link(adapter, build_c / 'conventional_rejection.c', library)
    run([compiler, 'check', *selector], cwd)
    assert not marker.exists()
    denied_output = root / ('unauthorized' + exe)
    run([compiler, 'build', *selector, '-o', denied_output], cwd, 2, 'Pass --allow-native')
    assert not marker.exists() and not denied_output.exists()
    # Compile the unchanged generated adapter as well as the fault-injection wrapper.
    pristine = root / adapter.name
    link(pristine, generated, library)

    mismatch_dir = root / 'mismatched adapter'
    mismatch_dir.mkdir()
    bad_contract = mismatch_dir / descriptor.name
    write(bad_contract, contract.replace(values['FIXTURE'], 'f' * 64))
    mismatch_source = mismatch_dir / 'adapter.c'
    run([compiler, 'native-bind', bad_contract, 'conventional_fixture.h', '-o', mismatch_source], cwd)
    shutil.copy2(build_c / 'conventional_fixture.h', mismatch_dir / 'conventional_fixture.h')
    mismatch = mismatch_dir / adapter.name
    link(mismatch, mismatch_source, library)

    wrong_signature = root / 'wrong signature'
    wrong_signature.mkdir()
    wrong_header = (build_c / 'conventional_fixture.h').read_text().replace(
        'g4_conventional_scale(double, float)', 'g4_conventional_scale(double, double)')
    write(wrong_signature / 'conventional_fixture.h', wrong_header)
    shutil.copy2(generated, wrong_signature / generated.name)
    run([clang, '-std=c11', '-fsyntax-only', wrong_signature / generated.name], build_c, None,
        'C signature g4_conventional_scale')

    def invoke(args, case, status=0, contains=None, releases=(), initialized=True):
        marker.unlink(missing_ok=True)
        released.unlink(missing_ok=True)
        run(args, cwd, status, contains)
        assert marker.exists() == initialized
        init_log = marker.read_text().splitlines() if initialized else []
        release_log = released.read_text().splitlines() if released.exists() else []
        assert init_log == (['dependency initialized'] if initialized else [])
        assert release_log == list(releases), case
        print(f'{case}: initializers={init_log}, released={release_log}', flush=True)

    message = 'VOID native application payload 雪'
    shutil.copy2(adapter, build_c / 'adapter-with-rejection.bin')
    shutil.copy2(pristine, adapter)
    invoke([compiler, 'run', *selector, '--allow-native', '--', 'payload 雪'], 'pristine', contains=message,
           releases=('-2', '23'))
    shutil.copy2(build_c / 'adapter-with-rejection.bin', adapter)
    installed = {}
    for mode, options in (('normal', ['--no-opt']), ('optimized', [])):
        jit = [compiler, 'run', *selector, '--allow-native', '--no-cache', *options, '--']
        invoke([*jit, 'payload 雪'], mode, contains=message, releases=('-2', '23'))
        invoke([*jit, 'reject'], mode, 1, 'C resource operation rejected', ('31',))
        invoke([*jit, 'closed'], mode, 1, 'closed, null or invalid', ('42',))
        invoke([*jit, 'capacity'], mode, 1, 'C byte span outside admitted extent')
        output = root / mode / ('application' + exe)
        marker.unlink()
        run([compiler, 'build', *selector, '--allow-native', *options, '-o', output], cwd)
        assert not marker.exists()
        if mode == 'normal':
            run([compiler, 'build', *selector, '--allow-native', *options, '-o', output], cwd)
            assert not marker.exists()
        image_tool = Path(clang).with_name('llvm-readobj' + exe)
        imports = run([image_tool, '--coff-imports' if windows else '--dynamic-table', output], cwd)
        assert 'void_g4_conventional_' not in imports
        package = root / ('relocated ' + mode + ' 雪')
        output.parent.rename(package)
        packaged_native = package / 'native'
        packaged_native.mkdir()
        for image in (dependency, library, adapter):
            shutil.copy2(image, packaged_native / image.name)
        installed[mode] = package / output.name

    missing_export = 'g4_conventional_missing'
    write(descriptor, contract.replace('g4_conventional_scale', missing_export))
    invoke([compiler, 'run', *selector, '--allow-native', '--', 'payload 雪'], 'missing export',
           1, 'does not provide ' + missing_export)
    write(descriptor, contract)
    project.rename(root / 'archived project inputs')
    prefix.rename(root / 'archived compiler install')
    assert not project.exists() and not compiler.exists()
    environment['VOID_STDLIB_ROOT'] = str(root / 'absent stdlib')
    for mode, program in installed.items():
        invoke([program, 'payload 雪'], mode, contains=message, releases=('-2', '23'))
        invoke([program, 'reject'], mode, 1, 'C resource operation rejected', ('31',))
        invoke([program, 'closed'], mode, 1, 'closed, null or invalid', ('42',))
        invoke([program, 'capacity'], mode, 1, 'C byte span outside admitted extent')
        package = program.parent / 'native'
        binary = package / library.name
        hidden = binary.with_suffix('.saved')
        binary.rename(hidden)
        invoke([program, 'payload 雪'], mode, 1, 'C dynamic library', initialized=False)
        binary.write_bytes(b'invalid library image\n')
        invoke([program, 'payload 雪'], mode, 1, 'C dynamic load', initialized=False)
        binary.unlink()
        hidden.rename(binary)
        imported = package / dependency.name
        hidden = imported.with_suffix('.saved')
        imported.rename(hidden)
        # A candidate in the unrelated CWD must not satisfy a missing bundled dependency.
        shutil.copy2(hidden, cwd / imported.name)
        invoke([program, 'payload 雪'], mode, 1, 'C dynamic load', initialized=False)
        (cwd / imported.name).unlink()
        hidden.rename(imported)
        transport = package / adapter.name
        hidden = transport.with_suffix('.saved')
        transport.rename(hidden)
        shutil.copy2(mismatch, transport)
        invoke([program, 'payload 雪'], mode, 1, 'adapter rejected the exact descriptor')
        transport.unlink()
        hidden.rename(transport)
        invoke([program, 'payload 雪'], mode, contains=message, releases=('-2', '23'))
        for path in [program, *package.iterdir()]:
            print(f'{digest(path)}  {path}')
    print('PASS: conventional C FFI through installed compiler and relocated normal/O2 executables', flush=True)


retained = os.environ.get('VOID_G4_EVIDENCE_ROOT')
if retained:
    Path(retained).mkdir(parents=True, exist_ok=True)
    work = nullcontext(tempfile.mkdtemp(prefix='VOID G4 ', dir=retained))
else:
    work = tempfile.TemporaryDirectory(prefix='VOID G4 ')
with work as name:
    verify(Path(name))
