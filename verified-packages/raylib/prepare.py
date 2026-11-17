"""Generate the checked adapter and prepare the native directory from local raylib 6.0 inputs."""

import argparse
from hashlib import sha256
from io import BytesIO
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile
from urllib.request import urlopen
from zipfile import ZipFile


def digest(path):
    return sha256(path.read_bytes()).hexdigest()


def run(*args, cwd=None):
    command = list(map(str, args))
    print(command, flush=True)
    subprocess.run(command, check=True, cwd=cwd)


def write_catalog(directory, descriptor, provider, dependency, notice, target, import_library, revision='raylib-6.0-shim-v2-physical'):
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


def bundled_inputs(args, root, scratch, windows):
    # The shipped recipe is invoked directly, in isolated Python mode. Read
    # named files from the reviewed archive without extracting its directories.
    if sys.version_info < (3, 12):
        raise ValueError("Bundled raylib needs Python 3.12 or newer.")
    if windows:
        release = "raylib-6.0_win64_msvc16"
        asset = release + ".zip"
        size = 2597715
        expected = "c93c7dc74576e00e3ee57fa2bd5fd109fbfc5aca87e12046dd7ec2c2268b3f78"
        library = "raylib.dll"
    else:
        release = "raylib-6.0_linux_amd64"
        asset = release + ".tar.gz"
        size = 1793706
        expected = "b64ba618a19e7da9e9c0e09bb398ecfd477a77d2d7231901bafc8739d27c08d2"
        library = "libraylib.so.6.0.0"
    url = "https://github.com/raysan5/raylib/releases/download/6.0/" + asset
    print(f"Fetching pinned raylib 6.0 asset {url}", flush=True)
    with urlopen(url, timeout=30) as response:
        if not response.url.startswith("https://"):
            raise ValueError("Raylib asset requires HTTPS.")
        archive = response.read(size + 1)
    if len(archive) != size or sha256(archive).hexdigest() != expected:
        raise ValueError("Raylib release asset SHA-256/size mismatch.")
    print(f"Verified release asset SHA-256 {expected}", flush=True)
    args.library = scratch / library
    args.header = root / "upstream/raylib.h"
    args.license = root / "upstream/LICENSE"
    if windows:
        args.import_library = scratch / "raylibdll.lib"
        with ZipFile(BytesIO(archive)) as zipped:
            for item in (args.library, args.import_library):
                item.write_bytes(zipped.read(f"{release}/lib/{item.name}"))
    else:
        with tarfile.open(fileobj=BytesIO(archive), mode="r:gz") as packed:
            member = packed.getmember(f"{release}/lib/{library}")
            if not member.isfile():
                raise ValueError("Expected a regular library in the pinned raylib release.")
            with packed.extractfile(member) as source:
                args.library.write_bytes(source.read())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--voidc', default='voidc')
    parser.add_argument('--clang', default='clang')
    parser.add_argument('--header', type=Path)
    parser.add_argument('--library', type=Path)
    parser.add_argument('--license', type=Path)
    parser.add_argument('--import-library', type=Path)
    parser.add_argument('--managed-catalog', type=Path, help='write an approved local catalog to a new directory')
    parser.add_argument('--sanitize', action='store_true', help='instrument the shim when running a sanitized Linux Runtime')
    parser.add_argument('--api-package', type=Path, default=Path(__file__).resolve().parent, help='include generated bindings from verified-packages/raylib')
    parser.add_argument('--gallery-only', action='store_true', help='prepare the legacy gallery subset')
    parser.add_argument('--bundled', action='store_true', help='use the pinned official release for the shipped recipe')
    args = parser.parse_args()
    if args.bundled:
        if (not args.managed_catalog or any((args.header, args.library, args.license, args.import_library, args.gallery_only))
                or args.api_package != Path(__file__).resolve().parent):
            parser.error('--bundled requires --managed-catalog and the shipped package inputs')
        if sys.platform not in ('win32', 'linux'):
            parser.error('bundled raylib supports x86-64 GNU/Linux and Windows only')
        with tempfile.TemporaryDirectory(prefix='upstream-', dir=args.managed_catalog.parent) as scratch:
            bundled_inputs(args, Path(__file__).resolve().parent, Path(scratch), sys.platform == 'win32')
            prepare(args, parser)
    else:
        if not all((args.header, args.library, args.license)):
            parser.error('provide --header, --library and --license')
        if args.gallery_only:
            args.api_package = None
        prepare(args, parser)


def prepare(args, parser):
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
    if args.bundled:
        version = subprocess.check_output([args.clang, '--version'], text=True)
        if not version.startswith('clang version 23.1.4 '):
            parser.error('bundled raylib needs Clang 23.1.4')
        inputs += (digest(Path(args.clang)) + digest(Path(__file__)) +
                   subprocess.check_output([args.voidc, '--version'], text=True)).encode()
        if args.import_library is not None:
            inputs += digest(args.import_library).encode()
        inputs += b'sanitized' if args.sanitize else b'ordinary'
    thunk = sha256(inputs).hexdigest()
    fixture = sha256(args.header.read_bytes() + args.library.read_bytes()).hexdigest()
    plan = (native / 'raylib.descriptor.in').read_text(encoding='ascii')
    if api_native is None:
        plan = plan.replace('source-file /src/raylib_native.void', 'source-file /src/gallery_native.void')
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

    stage_parent = args.managed_catalog.parent if args.bundled else native
    with tempfile.TemporaryDirectory(prefix='.bind-', dir=stage_parent) as scratch:
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
            if args.bundled:
                flags += ['-Wl,/Brepro']
            imports = args.import_library.resolve()
        else:
            flags += ['-fPIC', '-fvisibility=hidden', '-Wl,-rpath,$ORIGIN']
            if args.sanitize:
                flags += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all']
            imports = stage / dependency
        sources = [source, *([api_native / 'raylib_api.c'] if api_native else []), stage / generated]
        if args.bundled and args.sanitize:
            # UBSan stores the primary source spelling in the shared object.
            # Relative names keep random scratch paths out of the locked bytes.
            for item in sources[:-1]:
                shutil.copyfile(item, stage / item.name)
            run(args.clang, *flags, *(item.name for item in sources), imports, '-o', stage / provider, cwd=stage)
        else:
            run(args.clang, *flags, *sources, imports, '-o', stage / provider)
        if args.managed_catalog is not None:
            write_catalog(args.managed_catalog, stage / descriptor, stage / provider, stage / dependency,
                          stage / 'RAYLIB-LICENSE.txt', target, args.import_library,
                          'raylib-6.0-api-v4-physical' if api_native else 'raylib-6.0-shim-v2-physical')
        else:
            for name in (provider, dependency, descriptor, generated, 'RAYLIB-LICENSE.txt'):
                (stage / name).replace(native / name)
                print(f'{digest(native / name)}  {native / name}')


if __name__ == '__main__':
    main()
