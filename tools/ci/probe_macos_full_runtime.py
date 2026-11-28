"""Link a real VOID object offline and test it with a native prelinked Runtime.

The Linux artifact contains executable code, an export list and diagnostic
metadata. The native runner uses its local SDK to link VOID's own code and
pinned open providers. No Apple SDK files are copied by this probe.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import platform
import shlex
import shutil
import subprocess
import sys
from urllib.request import urlretrieve

from probe_windows_gnu_runtime import emit_driver, extract, run, runtime_rows


ARCHITECTURES = ('arm64', 'x86_64')
SYSTEM = ('dyld_stub_binder', '___stderrp', '_fputc', '_fputs', '_fwrite',
          '_memcpy', '_memset', '_memmove')
FRAMEWORKS = ('Foundation', 'Security', 'CoreFoundation', 'CoreServices')
CURL_DEPENDENCIES = '''
macro(void_import_http_dependencies)
  set(CURL_USE_CMAKECONFIG OFF)
  set(CURL_ENABLE_SSL ON)
  set(USE_NGHTTP2 ON)
  set(NGHTTP2_USE_STATIC_LIBS ON)
  set(ENABLE_ARES ON)
  set(CARES_USE_STATIC_LIBS ON)
  set(USE_LIBIDN2 OFF)
  set(CURL_USE_LIBPSL ON)
  set(CURL_USE_OPENSSL ON)
  set(USE_APPLE_SECTRUST ON)
  set(USE_APPLE_IDN ON)
  set(CURL_DISABLE_OPENSSL_AUTO_LOAD_CONFIG ON)
  set(OPENSSL_USE_STATIC_LIBS ON)
  set(OPENSSL_INCLUDE_DIR "${CLOSURE}/include")
  set(OPENSSL_SSL_LIBRARY "${CLOSURE}/lib/libssl.a")
  set(OPENSSL_CRYPTO_LIBRARY "${CLOSURE}/lib/libcrypto.a")
  set(NGHTTP2_INCLUDE_DIR "${CLOSURE}/include")
  set(NGHTTP2_LIBRARY "${CLOSURE}/lib/libnghttp2.a")
  set(CARES_INCLUDE_DIR "${CLOSURE}/include")
  set(CARES_LIBRARY "${CLOSURE}/lib/libcares.a")
  set(LIBPSL_INCLUDE_DIR "${CLOSURE}/include")
  set(LIBPSL_LIBRARY "${CLOSURE}/lib/libpsl.a")
  set(VOID_HTTP_ZLIB "${CLOSURE}/lib/libz.a")
  set(VOID_HTTP_DEPS_ROOT "${CLOSURE}")
  set(CURL_CA_BUNDLE none CACHE STRING "Use native system trust" FORCE)
  set(CURL_CA_PATH none CACHE STRING "Use native system trust" FORCE)
  set(VOID_HTTP_LINK_FILES "${LIBPSL_LIBRARY}" "${NGHTTP2_LIBRARY}" "${OPENSSL_SSL_LIBRARY}"
      "${OPENSSL_CRYPTO_LIBRARY}" "${CARES_LIBRARY}" "${VOID_HTTP_ZLIB}")
  set(VOID_HTTP_SYSTEM_LINK_LIBRARIES "-framework Security" "-framework CoreFoundation"
      "-framework CoreServices" icucore iconv)
endmacro()
'''


def tbd(path, arch, install_name, symbols):
    path.write_text(f"""--- !tapi-tbd
tbd-version: 4
targets: [ {arch}-macos ]
install-name: '{install_name}'
exports:
  - targets: [ {arch}-macos ]
    symbols: [ {', '.join(symbols)} ]
...
""")


def emit(root, build, work, output, source_sha):
    work.mkdir()
    output.mkdir()
    compiler = Path(os.environ['VOID_CLANG'])
    linker = compiler.with_name('ld64.lld')
    nm = compiler.with_name('llvm-nm')
    dump = compiler.with_name('llvm-objdump')
    driver = emit_driver(root, build, work)
    project = work / 'project'
    run([build / 'voidc', 'new', 'app', project], root, work / 'project.log')
    (project / 'src/app.void').write_text('func main() -> i32:\n    print(43)\n    return 0\n')
    sources = {}
    for row in runtime_rows(build):
        file = Path(row['file']).relative_to(root).as_posix()
        sources[file] = [arg for arg in shlex.split(row['command']) if arg.startswith('-D')]
    sources['src/runtime/os_apple.mm'] = []
    (output / 'sources.json').write_text(json.dumps(sources, indent=2) + '\n')
    (output / 'source-sha.txt').write_text(source_sha + '\n')
    for arch in ARCHITECTURES:
        slice_work = work / arch
        slice_output = output / arch
        slice_work.mkdir()
        slice_output.mkdir()
        obj = slice_work / 'program.o'
        run([driver, project, obj, f'{arch}-apple-macosx14.0'], root, slice_work / 'emit.log')
        imports = subprocess.check_output([nm, '--undefined-only', obj], text=True).split()
        exports = sorted(name for name in imports if name.startswith('_vd_'))
        if not exports or any(name not in SYSTEM and name not in exports for name in imports):
            raise ValueError(f'{arch}: unexpected application imports: {imports}')
        (slice_output / 'exports.txt').write_text('\n'.join(exports) + '\n')
        runtime_stub = slice_work / 'libVoidRuntime.tbd'
        system_stub = slice_work / 'libSystem.tbd'
        tbd(runtime_stub, arch, '@rpath/libVoidRuntime.dylib', exports)
        tbd(system_stub, arch, '/usr/lib/libSystem.B.dylib', SYSTEM)
        isolation = ['sudo', 'unshare', '--net', '--', 'runuser', '-u', os.environ['USER'], '--']
        if os.environ.get('GITHUB_ACTIONS') != 'true':
            isolation = []
        app = slice_output / 'app'
        run([*isolation, linker, '-arch', arch, '-platform_version', 'macos', '14.0', '14.0',
             '-adhoc_codesign', '-rpath', '@executable_path', '-o', app, obj, runtime_stub, system_stub],
            root, slice_work / 'link.log')
        report = subprocess.check_output([dump, '--macho', '--dylibs-used', app], text=True)
        if '@rpath/libVoidRuntime.dylib' not in report or '/usr/lib/libSystem.B.dylib' not in report:
            raise ValueError(f'{arch}: Linux-linked Mach-O lacks the intended loader edges')
        (slice_output / 'dependencies.txt').write_text(report)


def native_sources(root, local):
    from http_archive import acquire

    sources = {}
    policy = json.loads((root / 'third_party/http-versions.json').read_text())['dependencies']
    names = ('zlib', 'c-ares', 'nghttp2', 'libpsl', 'aws-lc')
    for pin in policy:
        if pin['name'] in names:
            archive = local / pin['archive']
            acquire(pin, archive, local)
            sources[pin['name']] = extract(archive, local / pin['name'], pin['sha256'])
    host = json.loads((root / 'third_party/host-versions.json').read_text())
    for name in ('libarchive', 'libsodium'):
        pin = host[name]
        archive = local / pin['url'].rsplit('/', 1)[1]
        urlretrieve(pin['url'], archive)
        sources[name] = extract(archive, local / name, pin['sha256'])
        if name == 'libsodium':
            sources['sodium-archive'] = archive
    return sources


def native_closure(root, work, arch, sdk, cc, cxx):
    from verify_host_deps import prepare_sodium

    sources_dir = work / 'sources'
    sources_dir.mkdir()
    sources = native_sources(root, sources_dir)
    prefix = work / 'prefix'
    common = ['-G', 'Ninja', f'-DCMAKE_OSX_SYSROOT={sdk}', f'-DCMAKE_OSX_ARCHITECTURES={arch}',
              '-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0', f'-DCMAKE_C_COMPILER={cc}', f'-DCMAKE_CXX_COMPILER={cxx}',
              '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_INSTALL_LIBDIR=lib', f'-DCMAKE_INSTALL_PREFIX={prefix}',
              '-DCMAKE_POSITION_INDEPENDENT_CODE=ON', '-DBUILD_SHARED_LIBS=OFF']

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
    build('aws-lc', sources['aws-lc'], '-DBUILD_TESTING=OFF', '-DBUILD_LIBSSL=ON',
          '-DBUILD_TOOL=OFF', '-DDISABLE_GO=ON', '-DENABLE_SOURCE_MODIFICATION=OFF')
    disabled = ('WERROR', 'MBEDTLS', 'NETTLE', 'OPENSSL', 'MD', 'LIBB2', 'LZ4', 'LZO', 'LZMA', 'ZSTD',
                'BZip2', 'LIBXML2', 'BSDXML', 'EXPAT', 'WIN32_XMLLITE', 'PCREPOSIX', 'PCRE2POSIX',
                'LIBGCC', 'CNG', 'TAR', 'CPIO', 'CAT', 'UNZIP', 'XATTR', 'ACL', 'ICONV', 'TEST')
    build('archive', sources['libarchive'], '-DENABLE_ZLIB=ON', '-DPOSIX_REGEX_LIB=NONE',
          f'-DZLIB_LIBRARY={prefix / "lib/libz.a"}', f'-DZLIB_INCLUDE_DIR={prefix / "include"}',
          *('-DENABLE_' + feature + '=OFF' for feature in disabled))
    sodium_source = work / 'sodium-source'
    prepare_sodium(sources['libsodium'], sources['sodium-archive'], sodium_source)
    sodium = work / 'sodium'
    sodium.mkdir()
    log = work / 'sodium.log'
    run([sodium_source / 'configure', f'--prefix={prefix}', '--disable-shared', '--enable-static',
         '--enable-minimal', '--disable-dependency-tracking', f'CC={cc}',
         f'CFLAGS=-O2 -fPIC -arch {arch} -isysroot {sdk} -mmacosx-version-min=14.0'], sodium, log)
    run(['make', '-j3'], sodium, log)
    run(['make', 'install'], sodium, log)
    wrapped = work / 'curl-source'
    cmake = wrapped / 'cmake'
    cmake.mkdir(parents=True)
    for name in ('CurlProvider', 'CurlProviderObjects', 'CurlProviderFeatures', 'Sanitizers'):
        shutil.copyfile(root / 'cmake' / f'{name}.cmake', cmake / f'{name}.cmake')
    for name in ('third_party', 'tools', 'src'):
        (wrapped / name).symlink_to(root / name, target_is_directory=True)
    (cmake / 'HttpDependencies.cmake').write_text(CURL_DEPENDENCIES)
    (wrapped / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.24)
project(VoidMacRuntimeCurlProbe C)
include(cmake/CurlProvider.cmake)
void_add_curl_provider(HTTP)
''')
    build('curl', wrapped, f'-DCLOSURE={prefix}', target='libcurl_static')
    return prefix


def prove_native(root, work, artifact, arch, source_sha):
    work.mkdir()
    if platform.machine() != arch:
        raise ValueError('unexpected native runner architecture')
    if (artifact / 'source-sha.txt').read_text().strip() != source_sha:
        raise ValueError('unexpected Linux-built application source')
    sdk = subprocess.check_output(['xcrun', '--show-sdk-path'], text=True).strip()
    cc, cxx = (subprocess.check_output(['xcrun', '--find', name], text=True).strip()
               for name in ('clang', 'clang++'))
    prefix = native_closure(root, work, arch, sdk, cc, cxx)
    slices = artifact / arch
    app = slices / 'app'
    app.chmod(0o755)
    exports = slices / 'exports.txt'
    minimum = ['-arch', arch, '-isysroot', sdk, '-mmacosx-version-min=14.0']
    includes = [root / 'include', root / 'third_party/curl/include', root / 'third_party/date/include',
                root / 'third_party/ryu', prefix / 'include']
    sources = json.loads((artifact / 'sources.json').read_text())

    def compile_unit(item):
        name, definitions = item
        file = root / name
        if not file.resolve().is_relative_to(root) or file.suffix not in ('.c', '.cpp', '.mm'):
            raise ValueError(f'invalid native Runtime source: {name}')
        obj = work / (file.name + '.o')
        cpp = file.suffix != '.c'
        language = ['-std=c++20', '-fno-rtti', '-include', 'exception'] if cpp else []
        run([cxx if cpp else cc, '-O2', '-fPIC', *minimum, *language,
             *(f'-I{path}' for path in includes), '-DCURL_STATICLIB', '-DCARES_STATICLIB',
             *definitions, '-c', file, '-o', obj], root, obj.with_suffix('.log'))
        return obj

    with ThreadPoolExecutor(max_workers=3) as pool:
        objects = list(pool.map(compile_unit, sources.items()))
    archives = [work / 'curl/curl/lib/libcurl.a']
    archives += [prefix / 'lib' / f'lib{name}.a' for name in
                 ('archive', 'sodium', 'psl', 'nghttp2', 'ssl', 'crypto', 'cares', 'z')]
    adapter = slices / 'libVoidRuntime.dylib'
    frameworks = [item for framework in FRAMEWORKS for item in ('-framework', framework)]
    run([cxx, *minimum, '-dynamiclib', '-Wl,-install_name,@rpath/libVoidRuntime.dylib',
         f'-Wl,-exported_symbols_list,{exports}', *objects, *archives, *frameworks,
         '-lobjc', '-licucore', '-liconv', '-o', adapter], root, work / 'link-runtime.log')
    run(['codesign', '--force', '--sign', '-', adapter], root, work / 'sign.log')
    for image in (app, adapter):
        run(['codesign', '--verify', '--strict', image], root, work / 'sign.log')
        if subprocess.check_output(['lipo', '-archs', image], text=True).strip() != arch:
            raise ValueError(f'unexpected Mach-O slice: {image}')
    dependencies = subprocess.check_output(['otool', '-L', adapter], text=True)
    expected = ('/usr/lib/', '/System/Library/Frameworks/', '@rpath/libVoidRuntime.dylib ')
    for line in dependencies.splitlines()[1:]:
        if not line.lstrip().startswith(expected):
            raise ValueError(f'non-system dependency in the prelinked Runtime: {line}')
    (slices / 'runtime-dependencies.txt').write_text(dependencies)
    output = subprocess.check_output([app], cwd='/', text=True).strip()
    if output != '43':
        raise ValueError(f'{arch}: application failed its native runtime check: {output}')
    print(f'{arch}: Linux-linked VOID executable ran against the full native prelinked Runtime', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=('emit', 'native'))
    parser.add_argument('source', type=Path)
    parser.add_argument('build_or_artifact', type=Path)
    parser.add_argument('work', type=Path)
    parser.add_argument('output_or_arch')
    parser.add_argument('source_sha')
    args = parser.parse_args()
    root, work, build = map(Path.resolve, (args.source, args.work, args.build_or_artifact))
    if subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip() != args.source_sha:
        raise ValueError('source revision differs from requested probe input')
    sys.path.insert(0, str(root / 'tools'))
    if args.mode == 'emit':
        emit(root, build, work, Path(args.output_or_arch).resolve(), args.source_sha)
    else:
        prove_native(root, work, build, args.output_or_arch, args.source_sha)


if __name__ == '__main__':
    main()
