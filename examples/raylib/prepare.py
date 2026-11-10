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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--voidc', default='voidc')
    parser.add_argument('--clang', default='clang')
    parser.add_argument('--header', required=True, type=Path)
    parser.add_argument('--library', required=True, type=Path)
    parser.add_argument('--license', required=True, type=Path)
    parser.add_argument('--import-library', type=Path)
    parser.add_argument('--sanitize', action='store_true', help='instrument the shim when running a sanitized Linux Runtime')
    args = parser.parse_args()

    windows = sys.platform == 'win32'
    if windows != (args.import_library is not None) or (windows and args.sanitize):
        parser.error('provide a COFF import library on Windows and use --sanitize only on Linux')
    for path in (args.header, args.library, args.license, args.import_library):
        if path is not None:
            print(f'{digest(path)}  {path}', flush=True)
    if args.header.name != 'raylib.h':
        parser.error('--header must name raylib.h')

    native = Path(__file__).resolve().parent / 'native'
    platform = 'windows' if windows else 'linux'
    provider = 'void_raylib.dll' if windows else 'libvoid_raylib.so'
    dependency = 'raylib.dll' if windows else 'libraylib.so.600'
    target = 'x86_64-pc-windows-msvc msvc-x64' if windows else 'x86_64-unknown-linux-gnu sysv-x64'
    descriptor = f'raylib-{platform}.descriptor'
    generated = 'raylib_checked.c'
    header = native / 'raylib_provider.h'
    source = native / 'raylib_provider.c'
    thunk = sha256(header.read_bytes() + source.read_bytes()).hexdigest()
    fixture = sha256(args.header.read_bytes() + args.library.read_bytes()).hexdigest()
    plan = (native / 'raylib.descriptor.in').read_text(encoding='ascii')
    for field, value in dict(TARGET=target, THUNK=thunk, FIXTURE=fixture, LONG='i32' if windows else 'i64').items():
        plan = plan.replace(f'@{field}@', value)

    with tempfile.TemporaryDirectory(prefix='.bind-', dir=native) as scratch:
        stage = Path(scratch)
        (stage / descriptor).write_bytes(plan.encode('ascii'))
        shutil.copyfile(args.library, stage / dependency)
        shutil.copyfile(args.license, stage / 'RAYLIB-LICENSE.txt')
        run(args.voidc, 'native-bind', stage / descriptor, header.name, '-o', stage / generated)
        flags = ['-std=c11', '-shared', '-fuse-ld=lld', '-O2', '-Wall', '-Wextra', '-Werror',
                 '-I' + str(native), '-I' + str(args.header.resolve().parent)]
        if windows:
            flags += ['--target=x86_64-pc-windows-msvc', '-DUSE_LIBTYPE_SHARED', '-fms-runtime-lib=static']
            imports = args.import_library.resolve()
        else:
            flags += ['-fPIC', '-fvisibility=hidden', '-Wl,-rpath,$ORIGIN']
            if args.sanitize:
                flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all']
            imports = stage / dependency
        run(args.clang, *flags, source, stage / generated, imports, '-o', stage / provider)
        for name in (provider, dependency, descriptor, generated, 'RAYLIB-LICENSE.txt'):
            (stage / name).replace(native / name)
            print(f'{digest(native / name)}  {native / name}')


if __name__ == '__main__':
    main()
