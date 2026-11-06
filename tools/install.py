#!/usr/bin/env python3
"""Build and install VOID from this checkout using its existing pinned recipes.

Prepare platform prerequisites first: docs/install/README.md. No sudo, system
package installation, shell profile changes or binary packaging is performed.
"""
import argparse
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]


def positive(value):
    value = int(value)
    if value < 1:
        raise argparse.ArgumentTypeError('must be at least 1')
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prefix', required=True, type=Path, help='Installation directory (use a user-owned prefix)')
    parser.add_argument('--configuration', choices=('Release', 'Debug'), default='Release')
    parser.add_argument('--build-dir', type=Path, help='Separate CMake build directory; default build/source-<configuration>')
    parser.add_argument('--jobs', type=positive, default=3, help='Parallel build jobs (default 3)')
    args = parser.parse_args()
    if sys.version_info < (3, 12):
        parser.error('Python 3.12+ is required')
    system = platform.system()
    arch = {'amd64': 'x86_64', 'aarch64': 'arm64'}.get(platform.machine().lower(), platform.machine().lower())
    if (system, arch) not in {('Linux', 'x86_64'), ('Windows', 'x86_64'), ('Darwin', 'arm64')}:
        parser.error('Recipes cover Linux x64, Windows x64 and macOS ARM64 only; see platform qualification limits')
    required = ['cmake']
    generator = 'Ninja' if shutil.which('ninja') else 'Unix Makefiles'
    if system == 'Windows':
        required.append('ninja')
    if system in ('Linux', 'Darwin'):
        required += ['make', 'perl']
    if system == 'Windows' and os.environ.get('VSCMD_ARG_TGT_ARCH', '').lower() not in ('x64', 'amd64'):
        parser.error('Use the Visual Studio 2022 x64 Native Tools environment with Windows and DIA SDKs')
    for name in required:
        if not shutil.which(name):
            parser.error(f'Missing prerequisite: {name}; see docs/install/{system.lower() if system != "Darwin" else "macos"}.md')
    prefix = args.prefix.expanduser().resolve()
    build = (args.build_dir or ROOT / 'build' / ('source-' + args.configuration.lower())).expanduser().resolve()
    if prefix == build or prefix.is_relative_to(build) or build.is_relative_to(prefix):
        parser.error('Build and installation directories must be separate')
    if prefix == ROOT or ROOT.is_relative_to(prefix):
        parser.error('The installation prefix cannot contain the source checkout')
    build.mkdir(parents=True, exist_ok=True)
    receipt = build / 'source-install.json'
    record = {'system': system, 'architecture': arch, 'python': sys.version,
              'configuration': args.configuration, 'prefix': str(prefix), 'commands': [], 'status': 'RUNNING'}

    def save():
        receipt.write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')

    environment = os.environ.copy()

    def run(label, argv):
        argv = list(map(str, argv))
        step = {'label': label, 'argv': argv, 'exit_code': None, 'log': str(build / (label + '.log'))}
        record['commands'].append(step)
        save()
        print('Running:', subprocess.list2cmdline(argv), flush=True)
        started = time.monotonic()
        with Path(step['log']).open('w', encoding='utf-8') as log:
            child = subprocess.Popen(argv, cwd=ROOT, env=environment, stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT, text=True, encoding='utf-8', errors='replace')
            for line in child.stdout:
                log.write(line)
                print(line, end='', flush=True)
            step['exit_code'] = child.wait()
        step['seconds'] = time.monotonic() - started
        save()
        if step['exit_code']:
            raise RuntimeError(f'{label} failed ({step["exit_code"]}); see {step["log"]}')

    save()
    try:
        # Use the same SDK verification, exported dependencies and host tools as CI.
        # Existing caches require the pinned identity; no unverified SDK fallback.
        os.chdir(ROOT)
        from ci import llvm_sdk
        sdk = ROOT / '.toolchains' / ('llvm-' + llvm_sdk.VERSION)
        environment.update(llvm_sdk.prepare(sdk))
        environment['PATH'] = str(sdk / 'bin') + os.pathsep + environment.get('PATH', '')
        record['sdk'] = json.loads((sdk / 'void-sdk-identity.json').read_text(encoding='utf-8'))
        bundle = {'Linux': 'linux-aws-lc-5.12.0', 'Windows': 'windows-schannel-x86_64',
                  'Darwin': 'macos-aws-lc-5.12.0-arm64'}[system]
        http = ROOT / '.toolchains/http/prefix' / bundle / args.configuration
        if not (http / 'VOID-HTTP-DEPENDENCIES.json').exists():
            command = [sys.executable, ROOT / 'tools/setup_http_deps.py', '--profile', args.configuration, '--jobs', args.jobs]
            if system == 'Darwin':
                command.append('--macos-install-only')
            run('http-setup', command)
        # Fail closed on a stale/modified cache. The existing setup script is the rebuild path.
        command = [sys.executable, ROOT / 'tools/verify_http_dependencies.py', http, args.configuration, system]
        if system != 'Linux':
            command.append(arch)
        run('http-verify', command)
        command = ['cmake', '-S', ROOT, '-B', build, '-G', generator,
                   '-DCMAKE_BUILD_TYPE=' + args.configuration, '-DBUILD_TESTING=OFF',
                   '-DVOID_SANITIZERS=OFF', '-DVOID_LLVM_RESEARCH=OFF',
                   '-DPython3_EXECUTABLE=' + sys.executable, '-DVOID_HTTP_DEPS_ROOT=' + str(http)]
        for name, key in (('CMAKE_C_COMPILER', 'CC'), ('CMAKE_CXX_COMPILER', 'CXX'),
                          ('LLVM_DIR', 'LLVM_DIR'), ('VOID_CLANG', 'VOID_CLANG'), ('VOID_LINKER', 'VOID_LINKER')):
            command.append('-D' + name + '=' + environment[key])
        if system == 'Linux':
            command.append('-DCMAKE_EXE_LINKER_FLAGS=--ld-path="' + environment['VOID_LINKER'] + '"')
        elif system == 'Darwin':
            command += ['-DCMAKE_OSX_ARCHITECTURES=arm64', '-DCMAKE_OSX_SYSROOT=' + environment['SDKROOT'],
                        '-DCMAKE_OSX_DEPLOYMENT_TARGET=' + environment.get('MACOSX_DEPLOYMENT_TARGET', '14.0')]
        run('configure', command)
        run('build', ['cmake', '--build', build, '--target', 'voidc', 'voidls', '--parallel', args.jobs])
        for component in ('compiler', 'tooling'):
            run('install-' + component, ['cmake', '--install', build, '--config', args.configuration,
                                        '--prefix', prefix, '--component', component])
        run('verify-install', [sys.executable, ROOT / 'tools/verify_install.py', prefix])
        record['status'] = 'PASS'
        print(f'Installed and verified VOID in {prefix}\nAdd {prefix / "bin"} to PATH.\nReceipt: {receipt}')
    except Exception as error:
        record.update(status='FAIL', error=str(error))
        raise
    finally:
        save()


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f'Source installation failed: {error}', file=sys.stderr)
        raise SystemExit(1)
