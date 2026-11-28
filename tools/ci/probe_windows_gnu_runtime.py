"""Build two complete Windows Runtime closures with LLVM-MinGW on Linux.

This is a hosted engineering probe. Its artifacts are executable test programs
and PE inspection reports. It neither produces an official pack nor consumes
Microsoft developer SDK files.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tarfile
from urllib.request import urlretrieve


ARCHITECTURES = ('x86_64', 'aarch64')
MINGW_RELEASE = '20261117'
MINGW_ARCHIVE = f'llvm-mingw-{MINGW_RELEASE}-ucrt-ubuntu-22.04-x86_64.tar.xz'
MINGW_DIGEST = 'e7a15a49a2ea95e9f3d8a9d0cfabe5052a4b10fb4777cc54de5a8c27b64bb701'
RUNTIME_TARGETS = ('void_runtime', 'void_format_objects', 'void_zone_objects',
                   'void_application_objects', 'void_native_fixture')

EMITTER = r'''
#include "void/frontend.hpp"
#include "void/private_diagnostics.hpp"
#include <filesystem>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 4) return 2;
    auto input = vd::analyze_manifest_project(std::filesystem::path(argv[1]));
    if (!input.succeeded()) {
        for (const auto& diagnostic : input.diagnostics)
            std::cerr << vd::render_diagnostic(diagnostic) << '\n';
        return 1;
    }
    (void)vd::emit_publication_object(input.project->semantic_analysis()->lower(vd::ir::Emission::entry),
        std::filesystem::path(argv[2]), true, std::string(64, '0'), false, false, nullptr, false, argv[3]);
}
'''
NETWORK_HEADERS = '''
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef interface
#undef interface
#endif
'''

CURL_DEPENDENCIES = '''
macro(void_import_http_dependencies)
  set(CURL_TARGET_WINDOWS_VERSION 0x0A00)
  set(CURL_USE_CMAKECONFIG OFF)
  set(CURL_ENABLE_SSL ON)
  set(USE_NGHTTP2 ON)
  set(NGHTTP2_USE_STATIC_LIBS ON)
  set(ENABLE_ARES ON)
  set(CARES_USE_STATIC_LIBS ON)
  set(USE_LIBIDN2 OFF)
  set(CURL_USE_LIBPSL ON)
  set(CURL_USE_SCHANNEL ON)
  set(CURL_USE_OPENSSL OFF)
  set(ENABLE_UNICODE ON)
  set(USE_WIN32_IDN ON)
  set(NGHTTP2_INCLUDE_DIR "${CLOSURE}/include")
  set(NGHTTP2_LIBRARY "${CLOSURE}/lib/libnghttp2.a")
  set(CARES_INCLUDE_DIR "${CLOSURE}/include")
  set(CARES_LIBRARY "${CLOSURE}/lib/libcares.a")
  set(LIBPSL_INCLUDE_DIR "${CLOSURE}/include")
  set(LIBPSL_LIBRARY "${CLOSURE}/lib/libpsl.a")
  set(VOID_HTTP_ZLIB "${CLOSURE}/lib/libzs.a")
  set(VOID_HTTP_DEPS_ROOT "${CLOSURE}")
  set(VOID_HTTP_LINK_FILES "${LIBPSL_LIBRARY}" "${NGHTTP2_LIBRARY}" "${CARES_LIBRARY}" "${VOID_HTTP_ZLIB}")
  set(VOID_HTTP_SYSTEM_LINK_LIBRARIES ws2_32 crypt32 secur32 normaliz iphlpapi advapi32 bcrypt)
endmacro()
'''


def run(command, cwd, log):
    with log.open('ab') as output:
        result = subprocess.run(command, cwd=cwd, stdout=output, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(f'{command[0]} failed, see {log}')


def extract(source, destination, digest):
    with source.open('rb') as stream:
        if hashlib.file_digest(stream, 'sha256').hexdigest() != digest:
            raise ValueError(f'archive checksum mismatch: {source}')
    with tarfile.open(source) as bundle:
        bundle.extractall(destination, filter='data')
    children = list(destination.iterdir())
    if len(children) != 1 or not children[0].is_dir():
        raise ValueError(f'unexpected archive roots: {source}')
    return children[0]


def sources_for(root, work):
    sources = {}
    http = json.loads((root / 'third_party/http-versions.json').read_text())['dependencies']
    host = json.loads((root / 'third_party/host-versions.json').read_text())
    inputs = [(pin['name'], root / '.toolchains/http/downloads' / pin['archive'], pin['sha256'])
              for pin in http if pin['name'] in ('zlib', 'c-ares', 'nghttp2', 'libpsl')]
    inputs += [(name, root / '.toolchains/host/downloads' / pin['url'].rsplit('/', 1)[1], pin['sha256'])
               for name, pin in host.items() if name in ('libarchive', 'libsodium')]
    for name, archive, digest in inputs:
        sources[name] = extract(archive, work / name, digest)
        if name == 'libsodium':
            sources['sodium-archive'] = archive
    return sources


def curl_source(root, work):
    wrapper = work / 'curl-source'
    cmake = wrapper / 'cmake'
    cmake.mkdir(parents=True)
    for name in ('CurlProvider', 'CurlProviderObjects', 'CurlProviderFeatures', 'Sanitizers'):
        shutil.copyfile(root / 'cmake' / f'{name}.cmake', cmake / f'{name}.cmake')
    for name in ('third_party', 'tools', 'src'):
        (wrapper / name).symlink_to(root / name, target_is_directory=True)
    (cmake / 'HttpDependencies.cmake').write_text(CURL_DEPENDENCIES)
    (wrapper / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.24)
project(VoidGnuRuntimeCurlProbe C)
include(cmake/CurlProvider.cmake)
void_add_curl_provider(HTTP)
''')
    return wrapper


def build_providers(root, work, sources, curl, mingw, arch):
    from verify_host_deps import prepare_sodium

    prefix = work / 'prefix'
    tools = mingw / 'bin'
    driver = tools / f'{arch}-w64-mingw32'
    cc, cxx, rc = (str(driver) + '-' + tool for tool in ('clang', 'clang++', 'windres'))
    common = ['-G', 'Ninja', '-DCMAKE_SYSTEM_NAME=Windows', f'-DCMAKE_SYSTEM_PROCESSOR={arch}',
              f'-DCMAKE_C_COMPILER={cc}', f'-DCMAKE_CXX_COMPILER={cxx}', f'-DCMAKE_RC_COMPILER={rc}',
              '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_INSTALL_LIBDIR=lib', f'-DCMAKE_INSTALL_PREFIX={prefix}',
              '-DBUILD_SHARED_LIBS=OFF']

    def build(name, source, *options, target=None):
        directory = work / name
        log = work / f'{name}.log'
        run(['cmake', '-S', source, '-B', directory, *common, *options], root, log)
        command = ['cmake', '--build', directory, '--parallel', '3']
        if target:
            command += ['--target', target]
        run(command, root, log)
        if not target:
            run(['cmake', '--install', directory], root, log)

    build('zlib', sources['zlib'], '-DZLIB_BUILD_TESTING=OFF', '-DZLIB_BUILD_SHARED=OFF', '-DZLIB_BUILD_STATIC=ON')
    build('nghttp2', sources['nghttp2'], '-DENABLE_LIB_ONLY=ON', '-DBUILD_STATIC_LIBS=ON',
          '-DBUILD_TESTING=OFF', '-DENABLE_DOC=OFF')
    build('cares', sources['c-ares'], '-DCARES_STATIC=ON', '-DCARES_SHARED=OFF', '-DCARES_BUILD_TESTS=OFF',
          '-DCARES_BUILD_TOOLS=OFF', '-DCARES_INSTALL=ON')
    build('psl', root / 'cmake/http-psl', f'-DPSL_SOURCE={sources["libpsl"]}',
          f'-DPSL_DATA={root / "third_party/publicsuffix/public_suffix_list.dat"}')
    disabled = ('WERROR', 'MBEDTLS', 'NETTLE', 'OPENSSL', 'MD', 'LIBB2', 'LZ4', 'LZO', 'LZMA', 'ZSTD',
                'BZip2', 'LIBXML2', 'BSDXML', 'EXPAT', 'WIN32_XMLLITE', 'PCREPOSIX', 'PCRE2POSIX',
                'LIBGCC', 'CNG', 'TAR', 'CPIO', 'CAT', 'UNZIP', 'XATTR', 'ACL', 'ICONV', 'TEST')
    build('archive', sources['libarchive'], '-DENABLE_ZLIB=ON', '-DPOSIX_REGEX_LIB=NONE',
          f'-DZLIB_LIBRARY={prefix / "lib/libzs.a"}', f'-DZLIB_INCLUDE_DIR={prefix / "include"}',
          *('-DENABLE_' + feature + '=OFF' for feature in disabled))
    sodium_source = work / 'sodium-source'
    prepare_sodium(sources['libsodium'], sources['sodium-archive'], sodium_source)
    sodium = work / 'sodium'
    sodium.mkdir()
    log = work / 'sodium.log'
    run([sodium_source / 'configure', f'--host={arch}-w64-mingw32', f'--prefix={prefix}',
         '--disable-shared', '--enable-static', '--enable-minimal', '--disable-dependency-tracking',
         f'CC={cc}', f'AR={tools / "llvm-ar"}', f'RANLIB={tools / "llvm-ranlib"}'], sodium, log)
    run(['make', '-j3'], sodium, log)
    run(['make', 'install'], sodium, log)
    build('curl', curl, f'-DCLOSURE={prefix}', target='libcurl_static')
    print(f'{arch}: built the pinned GNU Runtime provider closure', flush=True)
    return prefix


def emit_driver(root, build, work):
    source = work / 'emit.cpp'
    obj = work / 'emit.o'
    binary = work / 'emit'
    source.write_text(EMITTER)
    log = work / 'emitter.log'
    clang = Path(os.environ['VOID_CLANG'])
    run([clang.with_name('clang++'), '-std=c++20', '-fno-rtti', '-Iinclude',
         '-c', source, '-o', obj], root, log)
    commands = subprocess.check_output(['ninja', '-C', build, '-t', 'commands', 'voidc'], text=True)
    link, = [shlex.split(line.split(' && ')[1]) for line in commands.splitlines()
             if ' -o voidc ' in line and ' CMakeFiles/voidc_objects.dir/src/main.cpp.o ' in line]
    link[link.index('CMakeFiles/voidc_objects.dir/src/main.cpp.o')] = str(obj)
    link[link.index('-o') + 1] = str(binary)
    run(link, build, log)
    return binary


def runtime_objects(root, build, work, prefix, mingw, arch):
    rows = json.loads((build / 'compile_commands.json').read_text())
    targets = set(RUNTIME_TARGETS)
    selected = [row for row in rows if row['file'].endswith(('.cpp', '.c'))
                and Path(row.get('output', '')).parts[:1] == ('CMakeFiles',)
                and Path(row['output']).parts[1].removesuffix('.dir') in targets]
    if len(selected) < 55:
        raise RuntimeError('Runtime compile inventory is incomplete')
    headers = work / 'network-prefix.hpp'
    headers.write_text(NETWORK_HEADERS)
    objdir = work / 'objects'
    objdir.mkdir()

    def compile_row(row):
        source = Path(row['file'])
        obj = objdir / (source.name + '.o')
        command = shlex.split(row['command'])
        native_flags = iter(command[1:command.index('-o')])
        flags = []
        for flag in native_flags:
            if flag == '-isystem':
                next(native_flags)
            elif not (flag.startswith('-I') and '.toolchains/host/prefix' in flag):
                flags.append(flag)
        cxx = source.suffix == '.cpp'
        if cxx:
            flags += ['-include', 'exception']
            if source.stem.startswith('network'):
                flags += ['-include', str(headers)]
        flags += [f'-I{prefix / "include"}', '-D_WIN32_WINNT=0x0A00', '-DLIBARCHIVE_STATIC',
                  '-DSODIUM_STATIC', '-DCARES_STATICLIB', '-D_CRT_SECURE_NO_WARNINGS', '-DNOMINMAX',
                  '-DWIN32_LEAN_AND_MEAN', '-ffunction-sections', '-fdata-sections']
        compiler = mingw / 'bin' / f'{arch}-w64-mingw32-{"clang++" if cxx else "clang"}'
        run([compiler, *flags, '-c', source, '-o', obj], root, obj.with_suffix('.log'))
        return obj

    with ThreadPoolExecutor(max_workers=3) as parallel:
        objects = list(parallel.map(compile_row, selected))
    archive = work / 'libvoid-runtime.a'
    run([mingw / 'bin/llvm-ar', 'rcs', archive, *objects], root, work / 'archive-runtime.log')
    print(f'{arch}: rebuilt {len(objects)} Runtime and fixture objects', flush=True)
    return archive


def link_app(root, work, prefix, mingw, arch, runtime, emit, project, output):
    tools = mingw / 'bin'
    obj = work / 'app.o'
    exe = output / f'{arch}.exe'
    log = work / 'link.log'
    run([emit, project, obj, f'{arch}-w64-windows-gnu'], root, log)
    archives = [runtime, work / 'curl/curl/lib/libcurl.a']
    archives += [prefix / 'lib' / name for name in
                 ('libcares.a', 'libnghttp2.a', 'libpsl.a', 'libarchive.a', 'libzs.a', 'libsodium.a')]
    system = ('ws2_32', 'crypt32', 'secur32', 'normaliz', 'iphlpapi', 'advapi32', 'bcrypt',
              'shell32', 'ole32', 'uuid', 'synchronization')
    isolation = ['sudo', 'unshare', '--net', '--', 'runuser', '-u', os.environ['USER'], '--']
    if os.environ.get('GITHUB_ACTIONS') != 'true':
        isolation = []
    run([*isolation, tools / f'{arch}-w64-mingw32-clang++', '-municode', '-static', '-Wl,--gc-sections',
         obj, '-Wl,--start-group', *archives, '-Wl,--end-group', *(f'-l{name}' for name in system),
         '-o', exe], root, log)
    report = subprocess.check_output([tools / 'llvm-readobj', '--file-headers', '--coff-imports', exe], text=True)
    machine = {'x86_64': 'AMD64', 'aarch64': 'ARM64'}[arch]
    if f'Machine: IMAGE_FILE_MACHINE_{machine} ' not in report or 'api-ms-win-crt-' not in report:
        raise ValueError(f'{arch}: PE machine or UCRT imports differ')
    if any(f'name: {name}' in report.lower() for name in
           ('msvcp', 'vcruntime', 'libc++', 'libwinpthread')):
        raise ValueError(f'{arch}: mixed or dynamic C++ closure')
    (output / f'{arch}.imports.txt').write_text(report)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('build', type=Path)
    parser.add_argument('work', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('source_sha')
    args = parser.parse_args()
    root, build, work, output = map(Path.resolve, (args.source, args.build, args.work, args.output))
    if subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip() != args.source_sha:
        raise ValueError('source revision differs from the requested probe input')
    sys.path.insert(0, str(root / 'tools'))
    work.mkdir()
    output.mkdir()
    archive = work / MINGW_ARCHIVE
    urlretrieve(f'https://github.com/mstorsjo/llvm-mingw/releases/download/{MINGW_RELEASE}/{MINGW_ARCHIVE}', archive)
    mingw = extract(archive, work / 'mingw', MINGW_DIGEST)
    sources = sources_for(root, work / 'sources')
    curl = curl_source(root, work)
    emit = emit_driver(root, build, work)
    project = work / 'project'
    run([build / 'voidc', 'new', 'app', project], root, work / 'project.log')
    (project / 'src/app.void').write_text('func main() -> i32:\n    print(37)\n    return 0\n')
    for arch in ARCHITECTURES:
        directory = work / arch
        directory.mkdir()
        prefix = build_providers(root, directory, sources, curl, mingw, arch)
        runtime = runtime_objects(root, build, directory, prefix, mingw, arch)
        link_app(root, directory, prefix, mingw, arch, runtime, emit, project, output)
    (output / 'source-sha.txt').write_text(args.source_sha + '\n')


if __name__ == '__main__':
    main()
