"""Generate the checked adapter and prepare the native directory from local raylib 6.0 inputs."""

import argparse
from hashlib import sha256
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def digest(path):
    return sha256(path.read_bytes()).hexdigest()


def run(*args):
    command = list(map(str, args))
    print(command, flush=True)
    subprocess.run(command, check=True)


def write_catalog(directory, descriptor, provider, dependency, notice, target, import_library, revision='raylib-6.0-shim-v1'):
    directory.mkdir()
    files = [descriptor, provider, dependency, notice]
    if import_library is not None:
        files.append(import_library)
    for path in files:
        shutil.copyfile(path, directory / path.name)

    def identity(path):
        copied = directory / path.name
        return f'{copied.stat().st_size}:{digest(copied)}'

    def entry(path):
        return f'"{path.name}" = "{identity(path)}"\n'

    plan = ('format = 1\n[native]\nid = "org.voidlang.raylib"\nversion = "6.0.0"\nabi = 3\n'
            f'target = "{target.split()[0]}"\nrevision = "{revision}"\n'
            f'descriptor = "{descriptor.name}"\ndescriptor-identity = "{identity(descriptor)}"\n'
            f'library = "{provider.name}"\nadapter = "{provider.name}"\n'
            '\n[native.runtime]\n' + entry(provider) + entry(dependency) + '\n[native.link]\n')
    if import_library is not None:
        plan += entry(import_library)
    plan += '\n[native.notices]\n' + entry(notice)
    (directory / 'void.native.toml').write_text(plan, encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--voidc', default='voidc')
    parser.add_argument('--clang', default='clang')
    parser.add_argument('--header', required=True, type=Path)
    parser.add_argument('--library', required=True, type=Path)
    parser.add_argument('--license', required=True, type=Path)
    parser.add_argument('--import-library', type=Path)
    parser.add_argument('--managed-catalog', type=Path, help='write an approved local catalog to a new directory')
    parser.add_argument('--sanitize', action='store_true', help='instrument the shim when running a sanitized Linux Runtime')
    parser.add_argument('--api-package', type=Path, help='include generated bindings from verified-packages/raylib')
    args = parser.parse_args()

    windows = sys.platform == 'win32'
    if windows != (args.import_library is not None) or (windows and args.sanitize):
        parser.error('provide a COFF import library on Windows and use --sanitize only on Linux')
    for path in (args.header, args.library, args.license, args.import_library):
        if path is not None:
            print(f'{digest(path)}  {path}', flush=True)
    if args.header.name != 'raylib.h':
        parser.error('--header must name raylib.h')
    if args.managed_catalog is not None and args.managed_catalog.exists():
        parser.error('--managed-catalog must name a new directory')

    native = Path(__file__).resolve().parent / 'native'
    platform = 'windows' if windows else 'linux'
    provider = 'void_raylib.dll' if windows else 'libvoid_raylib.so'
    dependency = 'raylib.dll' if windows else 'libraylib.so.600'
    target = 'x86_64-pc-windows-msvc msvc-x64' if windows else 'x86_64-unknown-linux-gnu sysv-x64'
    descriptor = f'raylib-{platform}.descriptor'
    generated = 'raylib_checked.c'
    header = native / 'raylib_provider.h'
    source = native / 'raylib_provider.c'
    inputs = header.read_bytes() + source.read_bytes()
    api_native = args.api_package.resolve() / 'native' if args.api_package else None
    if api_native is not None:
        if args.header.read_bytes() != (api_native.parent / 'upstream/raylib.h').read_bytes():
            parser.error('--header must match the pinned API package raylib.h exactly')
        # Generation is an explicit development operation. Preparation consumes
        # reviewed source; dependency fetch never compiles or executes it.
        for name in ('raylib_api.h', 'raylib_api.c', 'raylib_parity.h', 'raylib_api.descriptor.inc','raylib_callbacks.inc'):
            inputs += (api_native / name).read_bytes()
    thunk = sha256(inputs).hexdigest()
    fixture = sha256(args.header.read_bytes() + args.library.read_bytes()).hexdigest()
    plan = (native / 'raylib.descriptor.in').read_text(encoding='ascii')
    if api_native is not None:
        extra = (api_native / 'raylib_api.descriptor.inc').read_text(encoding='ascii')
        resource_lines = ''.join(line + '\n' for line in extra.splitlines() if line.startswith('resource '))
        extra = ''.join(line + '\n' for line in extra.splitlines() if not line.startswith('resource '))
        position = plan.index('callback ')
        plan = plan[:position] + resource_lines + plan[position:]
        plan = plan.replace('void-c-primitive 3 3', 'void-c-primitive 4 3').replace('c_bool:bool\n', 'c_bool:bool c_char:i8\n')
        records, separator, bindings = extra.partition('bind ')
        existing, first, rest = plan.partition('bind ')
        plan = existing + records + first + rest + separator + bindings
    for field, value in dict(TARGET=target, THUNK=thunk, FIXTURE=fixture, LONG='i32' if windows else 'i64').items():
        plan = plan.replace(f'@{field}@', value)

    with tempfile.TemporaryDirectory(prefix='.bind-', dir=native) as scratch:
        stage = Path(scratch)
        (stage / descriptor).write_bytes(plan.encode('ascii'))
        shutil.copyfile(args.library, stage / dependency)
        shutil.copyfile(args.license, stage / 'RAYLIB-LICENSE.txt')
        run(args.voidc, 'native-bind', stage / descriptor, 'raylib_api.h' if api_native else header.name, '-o', stage / generated)
        flags = ['-std=c11', '-shared', '-fuse-ld=lld', '-O2', '-Wall', '-Wextra', '-Werror',
                 '-I' + str(native), '-I' + str(args.header.resolve().parent)]
        if api_native is not None:
            flags += ['-I' + str(api_native),'-DVOID_RAYLIB_API_CALLBACKS=1']
        if windows:
            flags += ['--target=x86_64-pc-windows-msvc', '-DUSE_LIBTYPE_SHARED', '-fms-runtime-lib=static']
            imports = args.import_library.resolve()
        else:
            flags += ['-fPIC', '-fvisibility=hidden', '-Wl,-rpath,$ORIGIN']
            if args.sanitize:
                flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all']
            imports = stage / dependency
        run(args.clang, *flags, source, *([api_native / 'raylib_api.c'] if api_native else []),
            stage / generated, imports, '-o', stage / provider)
        if args.managed_catalog is not None:
            write_catalog(args.managed_catalog, stage / descriptor, stage / provider, stage / dependency,
                          stage / 'RAYLIB-LICENSE.txt', target, args.import_library,
                          'raylib-6.0-api-v3' if api_native else 'raylib-6.0-shim-v1')
        else:
            for name in (provider, dependency, descriptor, generated, 'RAYLIB-LICENSE.txt'):
                (stage / name).replace(native / name)
                print(f'{digest(native / name)}  {native / name}')


if __name__ == '__main__':
    main()
