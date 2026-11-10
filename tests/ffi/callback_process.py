"""Exercise G5 with a separate C provider and generated checked adapter."""
from contextlib import nullcontext
from hashlib import sha256
from pathlib import Path
import os
import re
import shutil
import subprocess
import sys
import tempfile

sys.stdout.reconfigure(encoding='utf-8')
compiler, clang = map(Path, sys.argv[1:3])
windows = os.name == 'nt'
sanitized = sys.argv[3] == 'ON'
managed = sys.argv[-1] == '--managed'
sources = Path(__file__).resolve().parent
environment = dict(os.environ)
environment.pop('VOID_STDLIB_ROOT', None)
environment.pop('VOID_NATIVE_CATALOG', None)
if sanitized:
    environment.update(ASAN_OPTIONS='halt_on_error=1:abort_on_error=1:detect_leaks=1',
                       UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
suffix = '.dll' if windows else '.so'
exe = '.exe' if windows else ''
requirement = '\n[native-requirements.g5]\nid = "org.voidlang.g5"\nversion = "=1.0.0"\nabi = 3\n'


def digest(path):
    return sha256(path.read_bytes()).hexdigest()


def approve(catalog, descriptor, provider, adapter):
    catalog.mkdir()
    for path in (descriptor, provider, adapter):
        shutil.copyfile(path, catalog / path.name)
    notice = catalog / 'LICENSE.txt'
    shutil.copyfile(sources.parents[1] / 'LICENSE', notice)

    def refresh_catalog():
        def identity(path):
            return f'{path.stat().st_size}:{digest(path)}'

        descriptor_copy, provider_copy, adapter_copy = (catalog / path.name for path in (descriptor, provider, adapter))
        (catalog / 'void.native.toml').write_text(
            'format = 1\n[native]\nid = "org.voidlang.g5"\nversion = "1.0.0"\nabi = 3\n'
            'target = "x86_64-unknown-linux-gnu"\nrevision = "g5-fixture"\n'
            f'descriptor = "{descriptor.name}"\ndescriptor-identity = "{identity(descriptor_copy)}"\n'
            f'library = "{provider.name}"\nadapter = "{adapter.name}"\n[native.runtime]\n'
            f'"{provider.name}" = "{identity(provider_copy)}"\n"{adapter.name}" = "{identity(adapter_copy)}"\n'
            f'[native.link]\n[native.notices]\n"{notice.name}" = "{identity(notice)}"\n', encoding='utf-8')

    refresh_catalog()
    return refresh_catalog


def run(cwd, *args, status=0, expected=None, env=None):
    command = list(map(str, args))
    print('cwd=', cwd, 'argv=', command, flush=True)
    completed = subprocess.run(command, cwd=cwd, env=env or environment, capture_output=True, timeout=90)
    output = (completed.stdout + completed.stderr).decode('utf-8', errors='replace')
    print(f'exit={completed.returncode}\n{output}', flush=True)
    assert completed.returncode != 0 if status is None else completed.returncode == status, output
    if expected:
        assert expected in output, output
    assert 'AddressSanitizer' not in output and 'UndefinedBehaviorSanitizer' not in output, output
    return output


def qualify(root):
    global compiler
    cwd = root / 'independent cwd'
    cwd.mkdir()
    prefix = root / 'compiler install'
    if len(sys.argv) > 5:
        cmake, build, config = sys.argv[5:8]
        run(cwd, cmake, '--install', build, '--prefix', prefix, '--config', config, '--component', 'compiler')
        compiler = prefix / 'bin' / ('voidc' + exe)
    project = root / 'native callbacks project'
    run(cwd, compiler, 'new', project, '--source', 'src/main.void', '--project-id', 'org.voidlang.callback-qual')
    native = root / 'compiled native inputs' if managed else project / 'native'
    native.mkdir()
    provider = native / ('g5_fixture' + suffix)
    adapter = native / ('g5_adapter' + suffix)
    descriptor = native / 'callback.descriptor'
    manifest = project / 'void.project.toml'
    with manifest.open('a', encoding='utf-8') as stream:
        stream.write(requirement if managed else
                     f'\n[native.{"windows" if windows else "linux"}]\ndescriptor = "{descriptor.name}"\n'
                     f'library = "{provider.name}"\nadapter = "{adapter.name}"\n')
    text = (sources / 'callback.descriptor.in').read_text()
    for key, value in {
        'TARGET': 'x86_64-pc-windows-msvc msvc-x64' if windows else 'x86_64-unknown-linux-gnu sysv-x64',
        'LONG': 'i32' if windows else 'i64', 'THUNK': digest(sources / 'callback_fixture.h'),
        'FIXTURE': digest(sources / 'callback_fixture.c'),
    }.items():
        text = text.replace(f'@{key}@', value)
    descriptor.write_bytes(text.encode('ascii'))
    source = project / 'src/main.void'
    shutil.copyfile(sources / 'callback.void', source)
    generated = root / 'callback_adapter.c'
    run(cwd, compiler, 'native-bind', descriptor, 'callback_fixture.h', '-o', generated)
    flags = ['-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-shared', '-fuse-ld=lld']
    if windows:
        flags += ['--target=x86_64-pc-windows-msvc', '-fms-runtime-lib=static']
        import_library = root / 'g5_fixture.lib'
        provider_flags = ['-Wl,/implib:' + str(import_library)]
    else:
        flags += ['-fPIC', '-fvisibility=hidden', '-pthread', '-Wl,-rpath,$ORIGIN']
        if sanitized:
            flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all']
        import_library = provider
        provider_flags = ['-Wl,-soname,' + provider.name]
    run(cwd, clang, *flags, *provider_flags, '-DG5_FIXTURE_BUILD', sources / 'callback_fixture.c', '-o', provider)
    run(cwd, clang, *flags, '-I' + str(sources), generated, import_library, '-o', adapter)
    catalog = root / 'approved catalog'
    if managed:
        approve(catalog, descriptor, provider, adapter)
        environment['VOID_NATIVE_CATALOG'] = str(catalog)
        run(cwd, compiler, 'resolve', '--project', project)
        run(cwd, compiler, 'resolve', '--project', project, '--locked')

    bad_header = root / 'callback_fixture.h'
    bad_header.write_text((sources / bad_header.name).read_text().replace(
        '(*G5Float)(void*, double, float)', '(*G5Float)(void*, double, double)'))
    run(cwd, clang, '-std=c11', '-fsyntax-only', generated, status=None, expected='C signature g5_mix')
    bad_header.unlink()
    bad_descriptor = root / 'unsupported.descriptor'
    bad_descriptor.write_bytes(text.replace('context-first', 'no-context', 1).encode('ascii'))
    run(cwd, compiler, 'native-bind', bad_descriptor, 'callback_fixture.h', '-o', root/'bad.c',
        status=None, expected='context-first')
    select = ['--project', project]
    run(cwd, compiler, 'check', *select)
    run(cwd, compiler, 'run', *select, status=2, expected='Pass --allow-native')

    original = source.read_bytes()
    source.write_bytes(original.replace(b'g5.twice(-2, local.step)', b'g5.twice(-2, mix_value)'))
    run(cwd, compiler, 'check', *select, status=None, expected='Type mismatch')
    source.write_bytes(original.replace(b'    if g5.twice(-2, local.step)', b'    var indirect = local.step\n    if g5.twice(-2, indirect)'))
    run(cwd, compiler, 'check', *select, status=None, expected='from this generation')
    source.write_bytes(original)

    if len(sys.argv) > 4:
        reload = root / 'reload.void'
        shutil.copyfile(sources / 'callback_reload.void', reload)
        refusal_source = root / 'callback_refusal_adapter.c'
        refusal_adapter = native / ('g5_refusal_adapter' + suffix)
        release = '        int32_t value = g5_unregister(arg0);'
        adapter_text = generated.read_text()
        assert adapter_text.count(release) == 1
        # The reload fixture holds one owner and retries each refusal once.
        # A refusal precedes the provider call and leaves the result word alone.
        refusal_source.write_text('#include <stdio.h>\n' + adapter_text.replace(release, '''
        static int refused;
        if (!refused) {
            refused = 1;
            puts("G5 transport refused before native unregister");
            fflush(stdout);
            return 2;
        }
        refused = 0;
''' + release))
        run(cwd, clang, *flags, '-I' + str(sources), refusal_source, import_library, '-o', refusal_adapter)
        reload_env = environment
        reload_args = [reload, native, provider.name, refusal_adapter.name, descriptor]
        if managed:
            reload_project = root / 'managed reload project'
            run(cwd, compiler, 'new', reload_project, '--source', 'src/main.void', '--project-id', 'org.voidlang.g5-reload')
            with (reload_project / 'void.project.toml').open('a', encoding='utf-8') as stream:
                stream.write(requirement)
            shutil.copyfile(reload, reload_project / 'src/main.void')
            reload_catalog = root / 'approved reload catalog'
            refresh_catalog = approve(reload_catalog, descriptor, provider, refusal_adapter)
            reload_env = environment | {'VOID_NATIVE_CATALOG': str(reload_catalog)}
            run(cwd, compiler, 'resolve', '--project', reload_project, env=reload_env)
            notice = reload_catalog / 'LICENSE.txt'
            files = [notice, reload_catalog / 'void.native.toml', reload_project / 'void.lock']
            originals = [path.read_bytes() for path in files]
            alternate = root / 'alternate approved metadata'
            alternate.mkdir()
            notice.write_bytes(originals[0] + b'\nG5 alternate notice identity.\n')
            refresh_catalog()
            run(cwd, compiler, 'resolve', '--project', reload_project, '--refresh', env=reload_env)
            for path, original in zip(files, originals):
                shutil.copyfile(path, alternate / path.name)
                path.write_bytes(original)
            reload_args = ['--managed', reload_project, reload_catalog, alternate]
        output = run(cwd, sys.argv[4], *reload_args, env=reload_env,
                     expected='compatible/incompatible reload, refusal/retry, retirement and self-drain passed')
        assert output.count('G5 transport refused before native unregister') == 4
        assert output.count('G5 native unregistered and drained') == 4

    def cases(command, env):
        for mode in ('normal', 'implicit', 'retired', 'failure'):
            argv = [*command, mode]
            if mode == 'failure':
                output = run(cwd, *argv, status=1, expected='Division by zero', env=env)
                assert 'G5 native unregistered' not in output
            else:
                retired = mode == 'retired'
                output = run(cwd, *argv, status=1 if retired else 0, expected='retained foreign callbacks passed', env=env)
                assert output.count('G5 native unregistered and drained') == 1
                barrier = 'G5 barrier mode=1 result=0' if mode == 'implicit' else 'G5 barrier mode=2 result=140'
                assert barrier in output
                if retired:
                    assert 'closed, null or invalid' in output

    packages = []
    for name, options in (('normal', ['--no-opt']), ('optimized', [])):
        cases([compiler, 'run', *select, '--allow-native', '--no-cache', *options, '--'], environment)
        output = root / ('linked-' + name) / ('g5' + exe)
        run(cwd, compiler, 'build', *select, '--allow-native', *options, '-o', output)
        package = root / ('relocated ' + name + ' 雪')
        output.parent.rename(package)
        packaged_native = package / 'native'
        if managed:
            assert {path.name: digest(path) for path in packaged_native.iterdir()} == {
                name: digest(catalog / name) for name in (provider.name, adapter.name, 'LICENSE.txt')
            }
        else:
            packaged_native.mkdir()
            for binary in (provider, adapter):
                shutil.copy2(binary, packaged_native / binary.name)
        packages.append(package / output.name)
    project.rename(root / 'retained original project')
    if prefix.exists():
        prefix.rename(root / 'retained compiler install')
    if managed:
        catalog.rename(root / 'retained original catalog')
    runtime_path = str(Path(os.environ['SystemRoot']) / 'System32') if windows else '/usr/bin:/bin'
    relocated_env = environment | {'PATH': runtime_path, 'VOID_STDLIB_ROOT': str(root / 'absent stdlib')}
    for binary in packages:
        cases([binary], relocated_env)
    print('G5 passed all four compiled and relocated execution modes with independent C')
    print('artifact identities', [(str(p), digest(p)) for p in (generated, *packages)])


retained = os.environ.get('VOID_G5_EVIDENCE_ROOT')
assert not managed or sys.platform.startswith('linux'), 'Managed G5 qualification requires Linux'
if retained:
    root = Path(retained)
    root.mkdir(parents=True, exist_ok=True)
    scratch = nullcontext(tempfile.mkdtemp(prefix='g5-', dir=root))
else:
    scratch = tempfile.TemporaryDirectory(prefix='VOID G5 ')
with scratch as directory:
    qualify(Path(directory))
