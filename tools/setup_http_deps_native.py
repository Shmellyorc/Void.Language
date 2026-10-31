#!/usr/bin/env python3
"""Pinned native Windows/macOS closure; invoked by setup_http_deps.py.

No upstream source edits, cross-ABI reuse or ambient transport/crypto libraries.
Windows uses Schannel/WinIDN and static MT archives. macOS uses the same pinned
unmodified AWS-LC source with curl SecTrust and Apple ICU IDN; SDK/architecture
and deployment selection are recorded in the prefix identity.
"""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tarfile
import time
import urllib.request

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile', choices=['Release', 'Debug', 'strict'], default='Release')
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    system = platform.system()
    if system not in ('Windows', 'Darwin'): raise SystemExit('Native Windows/macOS host required')
    architecture = platform.machine().lower()
    architecture = {'amd64': 'x86_64', 'aarch64': 'arm64'}.get(architecture, architecture)
    if architecture not in ('x86_64', 'arm64'): raise SystemExit('HTTP supports native x86_64/arm64 profiles')
    root = Path(__file__).resolve().parents[1]
    policy_path = root / 'third_party/http-versions.json'; policy = json.loads(policy_path.read_text())
    bundle = ('windows-schannel-' if system == 'Windows' else 'macos-aws-lc-5.12.0-') + architecture
    prefix = root / '.toolchains/http/prefix' / bundle / args.profile
    build_root = root / '.toolchains/http/build' / bundle / args.profile
    prefix.mkdir(parents=True, exist_ok=True); build_root.mkdir(parents=True, exist_ok=True)
    suffix = '.exe' if system == 'Windows' else ''
    cc = root / '.toolchains/llvm-23.1.4/bin' / ('clang-cl.exe' if system == 'Windows' else 'clang')
    cxx = cc if system == 'Windows' else cc.with_name('clang++')
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    receipt = root / 'docs/net/http/evidence' / f'dependencies-{bundle}-{args.profile}-{stamp}'; receipt.mkdir()
    selected = ['nghttp2', 'c-ares', 'zlib', 'libpsl'] + (['aws-lc'] if system == 'Darwin' else [])
    report = dict(host=platform.platform(), platform=system, architecture=architecture, bundle=bundle,
                  profile=args.profile, pins_sha256=hashlib.sha256(policy_path.read_bytes()).hexdigest(),
                  selected_dependencies=selected, commands=[])
    report['recipe_sha256'] = {name: hashlib.sha256((root / name).read_bytes()).hexdigest()
                              for name in ('tools/setup_http_deps_native.py', 'cmake/http-psl/CMakeLists.txt')}
    environment = os.environ.copy()
    if args.profile == 'strict':
        environment.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def run(argv, name, cwd=build_root):
        start = time.monotonic()
        with (receipt / (name + '.stdout')).open('wb') as out, (receipt / (name + '.stderr')).open('wb') as err:
            result = subprocess.run(list(map(str, argv)), cwd=cwd, env=environment, stdout=out, stderr=err)
        report['commands'].append(dict(argv=list(map(str, argv)), cwd=str(cwd), exit_code=result.returncode,
                                       seconds=time.monotonic()-start))
        (receipt / 'commands.json').write_text(json.dumps(report, indent=2) + '\n')
        if result.returncode: raise SystemExit(f'{name} failed: {receipt}')
    run([cc, '--version'], 'compiler')
    if system == 'Windows':
        target = os.environ.get('VSCMD_ARG_TGT_ARCH', '').lower()
        if target not in ({'x86_64': ('x64', 'amd64'), 'arm64': ('arm64',)}[architecture]):
            raise SystemExit('Use a matching native Visual Studio developer environment; no CRT/architecture fallback')
        if not shutil.which('ninja'): raise SystemExit('Ninja is required in the native developer environment')
        common = ['-G', 'Ninja', '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded', '-DCMAKE_POLICY_DEFAULT_CMP0091=NEW',
                  '-DCMAKE_C_FLAGS_DEBUG=/Od /Zi /D_DEBUG', '-DCMAKE_CXX_FLAGS_DEBUG=/Od /Zi /D_DEBUG /D_ITERATOR_DEBUG_LEVEL=0']
        flags = '/O2' if args.profile == 'Release' else '/Od /Zi'
        if args.profile == 'strict': flags += ' /clang:-fsanitize=address,undefined /clang:-fno-omit-frame-pointer'
        common += [f'-DCMAKE_C_FLAGS={flags}', f'-DCMAKE_CXX_FLAGS={flags}']
    else:
        sdk = environment.get('SDKROOT')
        if not sdk: sdk = subprocess.check_output(['xcrun', '--show-sdk-path'], text=True).strip()
        if not Path(sdk).is_absolute() or not Path(sdk).is_dir(): raise SystemExit('A physical native macOS SDK is required')
        minimum = environment.get('MACOSX_DEPLOYMENT_TARGET', '14.0')
        report.update(sdk=sdk, deployment_target=minimum)
        flags = '-O2 -fPIC -fvisibility=hidden' if args.profile == 'Release' else '-O0 -g -fPIC -fvisibility=hidden'
        if args.profile == 'strict': flags += ' -fsanitize=address,undefined -fno-omit-frame-pointer'
        common = ['-G', 'Unix Makefiles', f'-DCMAKE_OSX_SYSROOT={sdk}', f'-DCMAKE_OSX_ARCHITECTURES={architecture}',
                  f'-DCMAKE_OSX_DEPLOYMENT_TARGET={minimum}', f'-DCMAKE_C_FLAGS={flags}', f'-DCMAKE_CXX_FLAGS={flags}']
    sources = {}
    for pin in policy['dependencies']:
        if pin['name'] not in selected: continue
        if 'platforms' in pin and system not in pin['platforms']: raise SystemExit('TLS platform is absent from its pinned policy')
        archive = root / '.toolchains/http/downloads' / pin['archive']; archive.parent.mkdir(parents=True, exist_ok=True)
        if not archive.exists():
            temporary = archive.with_suffix(archive.suffix + '.part')
            with urllib.request.urlopen(pin['url'], timeout=120) as response, temporary.open('wb') as output:
                shutil.copyfileobj(response, output)
            temporary.replace(archive)
        if hashlib.sha256(archive.read_bytes()).hexdigest() != pin['sha256']: raise SystemExit(f'Archive checksum mismatch: {archive}')
        source = build_root / 'source' / pin.get('source_dir', pin['name'] + '-' + pin['version'])
        if source.exists(): shutil.rmtree(source)
        source.parent.mkdir(parents=True, exist_ok=True)
        with tarfile.open(archive) as bundle_file: bundle_file.extractall(source.parent, filter='data')
        sources[pin['name']] = source
    def cmake(name, options, source=None):
        build = build_root / name
        run(['cmake', '-S', source or sources[name], '-B', build, f'-DCMAKE_C_COMPILER={cc}', f'-DCMAKE_CXX_COMPILER={cxx}',
             f'-DCMAKE_INSTALL_PREFIX={prefix}', '-DCMAKE_INSTALL_LIBDIR=lib', '-DCMAKE_POSITION_INDEPENDENT_CODE=ON',
             '-DCMAKE_BUILD_TYPE=' + ('Release' if args.profile == 'Release' else 'Debug'), *common, *options], name + '-configure')
        run(['cmake', '--build', build, '--parallel', args.jobs], name + '-build')
        run(['cmake', '--install', build], name + '-install')
    cmake('nghttp2', ['-DENABLE_LIB_ONLY=ON', '-DBUILD_SHARED_LIBS=OFF', '-DBUILD_STATIC_LIBS=ON', '-DBUILD_TESTING=OFF', '-DENABLE_DOC=OFF'])
    cmake('c-ares', ['-DCARES_STATIC=ON', '-DCARES_SHARED=OFF', '-DCARES_BUILD_TESTS=OFF', '-DCARES_BUILD_TOOLS=OFF', '-DCARES_INSTALL=ON'])
    cmake('zlib', ['-DZLIB_BUILD_TESTING=OFF', '-DZLIB_BUILD_SHARED=OFF', '-DZLIB_BUILD_STATIC=ON'])
    if system == 'Darwin':
        cmake('aws-lc', ['-DBUILD_SHARED_LIBS=OFF', '-DBUILD_TESTING=OFF', '-DBUILD_LIBSSL=ON',
                         '-DDISABLE_GO=ON', '-DENABLE_SOURCE_MODIFICATION=OFF'])
    cmake('libpsl', [f'-DPSL_SOURCE={sources["libpsl"]}', f'-DPSL_DATA={root / "third_party/publicsuffix/public_suffix_list.dat"}'],
          root / 'cmake/http-psl')
    report['result'] = 'PASS (native dependency build; transport/platform acceptance separate)'
    report['files'] = {str(path.relative_to(prefix)).replace('\\', '/'): hashlib.sha256(path.read_bytes()).hexdigest()
                       for folder in ('lib', 'include', 'bin') for path in sorted((prefix / folder).rglob('*'))
                       if path.is_file() and not path.is_symlink()}
    (receipt / 'commands.json').write_text(json.dumps(report, indent=2) + '\n')
    (prefix / 'VOID-HTTP-DEPENDENCIES.json').write_text(json.dumps(report, indent=2) + '\n')
    print(prefix)

if __name__ == '__main__': main()
