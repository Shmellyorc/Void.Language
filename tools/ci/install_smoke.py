#!/usr/bin/env python3
"""Manual hosted installation qualification; no regression suites or binary reuse.

Each phase is a separate Actions step so llvm_sdk.py's GITHUB_ENV takes effect.
Only the disposable hosted checkout's stdlib is hidden, and restored in finally,
to prove installed discovery without a source fallback. Never run on accepted
local builds. Required failures propagate; receipts are written before assertions.
"""
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import time
import traceback

ROOT = Path(__file__).resolve().parents[2]
SYSTEM = platform.system()
SUFFIX = '.exe' if SYSTEM == 'Windows' else ''
REQUIRED = ('identity', 'host', 'sdk', 'dependencies', 'configure', 'build', 'install', 'smoke',
            'layout', 'minimal-check', 'minimal-run', 'console-check', 'console-run',
            'native-build', 'native-run', 'discovery', 'lsp', 'runtime')
MINIMAL = 'func main() -> i32:\n    return 0\n'
CONSOLE = 'use console\n\nfunc main() -> i32:\n    print("VOID installed stdlib OK")\n    return 0\n'
OUTPUT = b'VOID installed stdlib OK\n'


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def decode(data):
    return data.decode('utf-8', errors='replace')


def frames(messages):
    result = b''
    for message in messages:
        body = json.dumps(message, separators=(',', ':')).encode('utf-8')
        result += f'Content-Length: {len(body)}\r\n\r\n'.encode('ascii') + body
    return result


def unframe(data):
    messages = []
    while data:
        header, separator, rest = data.partition(b'\r\n\r\n')
        require(separator, 'LSP response lacks a complete header')
        lengths = re.findall(rb'(?im)^Content-Length:\s*([0-9]+)\s*$', header)
        require(len(lengths) == 1, 'LSP response must contain one Content-Length')
        length = int(lengths[0])
        require(0 < length <= len(rest), 'LSP response body is truncated or empty')
        messages.append(json.loads(rest[:length]))
        data = rest[length:]
    return messages


class Qualification:
    def __init__(self):
        self.work = (Path(os.environ['RUNNER_TEMP']) / os.environ['INSTALL_QUALIFICATION_NAME']).resolve()
        self.evidence = self.work / 'evidence'
        self.build = self.work / 'build'
        self.prefix = self.work / 'prefix'
        self.project = self.work / 'project'
        self.sdk = ROOT / '.toolchains/llvm-23.1.4'
        bundle = {'Linux': 'linux-aws-lc-5.12.0', 'Windows': 'windows-schannel-x86_64',
                  'Darwin': 'macos-aws-lc-5.12.0-arm64'}[SYSTEM]
        self.http = ROOT / '.toolchains/http/prefix' / bundle / 'Release'
        self.bin = self.prefix / 'bin'
        self.voidc = self.bin / ('voidc' + SUFFIX)
        self.voidls = self.bin / ('voidls' + SUFFIX)
        self.native = self.project / ('qualified' + SUFFIX)

    def save(self, name, value):
        self.evidence.mkdir(parents=True, exist_ok=True)
        (self.evidence / name).write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')

    def check(self, name, action):
        record = {'check': name, 'status': 'UNKNOWN'}
        self.save('check-' + name + '.json', record)
        try:
            action()
            record['status'] = 'PASS'
        except BaseException:
            record.update(status='FAIL', exception=traceback.format_exc())
            raise
        finally:
            self.save('check-' + name + '.json', record)

    def run(self, label, argv, *, cwd=ROOT, env=None, stdin=None, expected=None, timeout=5400):
        argv = list(map(str, argv))
        base = f'command-{len(list(self.evidence.glob("command-*.json"))):03d}-{label}'
        record = {'argv': argv, 'cwd': str(cwd), 'exit_code': None,
                  'stdout': base + '.stdout', 'stderr': base + '.stderr', 'status': 'UNKNOWN'}
        selected_env = os.environ if env is None else env
        record['environment'] = {key: selected_env.get(key) for key in
            ('PATH', 'CC', 'CXX', 'LLVM_DIR', 'VOID_CLANG', 'VOID_LINKER', 'CMAKE_PREFIX_PATH',
             'SDKROOT', 'MACOSX_DEPLOYMENT_TARGET', 'VOID_STDLIB_ROOT', 'LD_LIBRARY_PATH',
             'DYLD_LIBRARY_PATH', 'VCToolsVersion', 'WindowsSDKVersion', 'VSCMD_ARG_TGT_ARCH',
             'EXPERIMENT_CONFIGURATION', 'EXPERIMENT_DIR', 'THINLTO_CACHE_DIR')}
        if stdin is not None:
            (self.evidence / (base + '.stdin')).write_bytes(stdin)
            record['stdin'] = base + '.stdin'
        self.save(base + '.json', record)
        started = time.monotonic()
        print(json.dumps(argv), flush=True)
        try:
            with (self.evidence / record['stdout']).open('wb') as out, (self.evidence / record['stderr']).open('wb') as err:
                result = subprocess.run(argv, cwd=cwd, env=env, input=stdin, stdout=out, stderr=err, timeout=timeout)
            record['exit_code'] = result.returncode
            result.stdout = (self.evidence / record['stdout']).read_bytes()
            result.stderr = (self.evidence / record['stderr']).read_bytes()
            require(result.returncode == 0, f'{label} exited {result.returncode}; see {base} logs')
            if expected is not None:
                record['expected_stdout_hex'] = expected.hex()
                require(result.stdout == expected, f'{label} unexpected stdout: {result.stdout!r}')
                require(result.stderr == b'', f'{label} unexpected stderr: {result.stderr!r}')
            record['status'] = 'PASS'
            return result
        except BaseException:
            record.update(status='FAIL', exception=traceback.format_exc())
            for kind in ('stdout', 'stderr'):
                path = self.evidence / record[kind]
                if path.exists():
                    print(decode(path.read_bytes()[-8000:]), flush=True)
            raise
        finally:
            record['seconds'] = time.monotonic() - started
            self.save(base + '.json', record)

    def identity(self):
        require(os.environ.get('GITHUB_ACTIONS') == 'true' and
                os.environ.get('RUNNER_ENVIRONMENT') == 'github-hosted', 'Disposable GitHub-hosted runner required')
        require(not self.work.is_relative_to(ROOT) and not ROOT.is_relative_to(self.work),
                'Qualification scratch must be outside the checkout')
        require(self.work.is_relative_to(Path(os.environ['RUNNER_TEMP']).resolve()), 'Scratch must remain in runner temp')
        require(not (ROOT / '.toolchains').exists(), 'Fresh checkout required; refusing existing toolchain/dependency artifacts')
        arch = {'amd64': 'X64', 'x86_64': 'X64', 'aarch64': 'ARM64', 'arm64': 'ARM64'}.get(platform.machine().lower())
        commit = decode(self.run('commit', ['git', 'rev-parse', 'HEAD']).stdout).strip()
        self.save('identity.json', {'commit': commit, 'dispatch_sha': os.environ.get('GITHUB_SHA'),
            'configuration': 'Release', 'system': SYSTEM, 'platform': platform.platform(),
            'machine': platform.machine(), 'python': sys.version, 'runner': {key: os.environ.get(key) for key in
                ('RUNNER_NAME', 'RUNNER_OS', 'RUNNER_ARCH', 'RUNNER_ENVIRONMENT', 'ImageOS', 'ImageVersion',
                 'INSTALL_RUNNER_LABEL', 'GITHUB_RUN_ID', 'GITHUB_RUN_ATTEMPT')},
            'source_sha256': {name: digest(ROOT / name) for name in
                ('.github/workflows/ci-install-qualification.yml', 'tools/ci/install_smoke.py',
                 'tools/ci/llvm_sdk.py', 'tools/setup_http_deps.py', 'tools/setup_http_deps_native.py',
                 'third_party/http-versions.json')}})
        require(commit == os.environ['GITHUB_SHA'], 'Checkout differs from dispatch SHA')
        require(SYSTEM == os.environ['INSTALL_EXPECTED_SYSTEM'] and arch == os.environ['INSTALL_EXPECTED_ARCH']
                and arch == os.environ['RUNNER_ARCH'], 'Actual host/runner architecture differs from required platform')

    def host(self):
        if SYSTEM == 'Linux':
            env = os.environ | {'EXPERIMENT_CONFIGURATION': 'Release', 'EXPERIMENT_DIR': str(self.evidence),
                                'THINLTO_CACHE_DIR': str(self.work / 'unused-thinlto')}
            self.run('disk-prepare', [sys.executable, 'tools/ci/linux_disk.py', 'prepare'], env=env)
            self.run('apt-update', ['sudo', 'apt-get', 'update'])
            self.run('apt-install', ['sudo', 'apt-get', 'install', '-y', '--no-install-recommends',
                'build-essential', 'cmake', 'ninja-build', 'libffi-dev', 'libedit-dev', 'libxml2-dev',
                'libzstd-dev', 'zlib1g-dev', 'libtinfo-dev', 'ca-certificates', 'xz-utils', 'curl', 'git', 'perl'])
            self.run('packages', ['dpkg-query', '-W', 'build-essential', 'cmake', 'ninja-build', 'libffi-dev',
                'libedit-dev', 'libxml2-dev', 'libzstd-dev', 'zlib1g-dev', 'libtinfo-dev', 'ca-certificates', 'perl'])
        elif SYSTEM == 'Windows':
            require(os.environ.get('VSCMD_ARG_TGT_ARCH', '').lower() in ('x64', 'amd64'), 'Native x64 MSVC environment required')
            self.save('microsoft-sdk.json', {key: os.environ.get(key) for key in
                ('VSINSTALLDIR', 'VCToolsVersion', 'WindowsSDKVersion', 'VSCMD_ARG_TGT_ARCH')})
            self.run('msvc-version', ['cl.exe', '/Bv', '/?'])
        else:
            # macos-15 defaults to Xcode 16.4; VOID's vendor libc++ needs 26+.
            # This version is installed on the documented ARM64 runner image.
            self.run('select-xcode', ['sudo', 'xcode-select', '--switch', '/Applications/Xcode_26.3.app/Contents/Developer'])
            self.run('xcode-version', ['xcodebuild', '-version'])
            self.run('xcode-path', ['xcode-select', '-p'])
            self.run('apple-sdk-version', ['xcrun', '--show-sdk-version'])
            self.run('apple-linker', ['xcrun', '--find', 'ld'])
        self.run('cmake-version', ['cmake', '--version'])
        self.run('ninja-version', ['ninja', '--version'])

    def sdk_install(self):
        require(not self.sdk.exists(), 'SDK must be freshly downloaded and hash verified')
        self.run('sdk-bootstrap', [sys.executable, 'tools/ci/llvm_sdk.py'])
        shutil.copy2(self.sdk / 'void-sdk-identity.json', self.evidence / 'llvm-sdk-identity.json')
        for tool in ('clang', 'clang++', {'Linux': 'ld.lld', 'Windows': 'lld-link', 'Darwin': 'ld64.lld'}[SYSTEM]):
            result = self.run('version-' + tool, [self.sdk / 'bin' / (tool + SUFFIX), '--version'])
            require(re.search(rb'\b23\.1\.4\b', result.stdout), f'Wrong {tool} version')

    def dependencies(self):
        require(not self.http.exists(), 'HTTP Release prefix must be built afresh')
        self.run('http-bootstrap', [sys.executable, 'tools/setup_http_deps.py', '--profile', 'Release', '--jobs', '3'])
        args = [sys.executable, 'tools/verify_http_dependencies.py', self.http, 'Release', SYSTEM]
        if SYSTEM != 'Linux':
            args.append('x86_64' if SYSTEM == 'Windows' else 'arm64')
        self.run('http-verify', args)
        shutil.copy2(self.http / 'VOID-HTTP-DEPENDENCIES.json', self.evidence / 'http-dependencies.json')

    def configure(self):
        require(not self.build.exists() and not self.prefix.exists(), 'Build and install must start fresh')
        command = ['cmake', '-S', ROOT, '-B', self.build, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
            '-DBUILD_TESTING=OFF', '-DVOID_SANITIZERS=OFF', '-DVOID_LLVM_RESEARCH=OFF',
            '-DLLVM_DIR=' + os.environ['LLVM_DIR'], '-DVOID_CLANG=' + os.environ['VOID_CLANG'],
            '-DVOID_LINKER=' + os.environ.get('VOID_LINKER', ''), '-DVOID_HTTP_DEPS_ROOT=' + str(self.http),
            '-DCMAKE_C_COMPILER=' + os.environ['CC'], '-DCMAKE_CXX_COMPILER=' + os.environ['CXX']]
        if SYSTEM == 'Linux':
            command.append('-DCMAKE_EXE_LINKER_FLAGS=--ld-path="' + os.environ['VOID_LINKER'] + '"')
        elif SYSTEM == 'Darwin':
            command += ['-DCMAKE_OSX_ARCHITECTURES=arm64', '-DCMAKE_OSX_SYSROOT=' + os.environ['SDKROOT'],
                        '-DCMAKE_OSX_DEPLOYMENT_TARGET=' + os.environ['MACOSX_DEPLOYMENT_TARGET']]
        self.run('configure', command)

    def install(self):
        require(not self.prefix.exists(), 'Refusing to overwrite an installed prefix')
        for component in ('compiler', 'tooling'):
            self.run('install-' + component, ['cmake', '--install', self.build, '--config', 'Release',
                                            '--component', component, '--prefix', self.prefix])

    def layout(self):
        installed = self.prefix / 'share/void/stdlib'
        sources = {str(p.relative_to(ROOT / 'stdlib')): digest(p) for p in (ROOT / 'stdlib').rglob('*.void')}
        copies = {str(p.relative_to(installed)): digest(p) for p in installed.rglob('*.void')}
        self.save('stdlib.json', {'source': sources, 'installed': copies})
        require(sources == copies and 'console.void' in copies, 'Installed standard sources differ or are missing')
        for executable in (self.voidc, self.voidls):
            require(executable.is_file() and executable.resolve().is_relative_to(self.prefix), 'Missing/private-prefix executable mismatch')
        lines = (self.bin / 'void-native.txt').read_text(encoding='utf-8').splitlines()
        require(lines[0] == 'VOID native 23.1.4 v2', 'Invalid installed native manifest version')
        require(Path(lines[1]).resolve() == (self.sdk / 'bin' / ('clang' + SUFFIX)).resolve(), 'Native driver differs from pinned SDK')
        require(lines[2] == (os.environ['VOID_LINKER'] if SYSTEM == 'Linux' else ''), 'Wrong native linker override')
        resource_lines = (self.bin / 'void-native-resources.txt').read_text(encoding='utf-8').splitlines()
        require(resource_lines[0] == 'VOID native resources 1', 'Invalid resource inventory version')
        resources = {}
        for line in resource_lines[1:]:
            expected, relative = line.split('\t')
            require(relative.startswith('@VOID_NATIVE_DIR@/../lib/void/'), 'Resource is not installation relative')
            path = Path(relative.replace('@VOID_NATIVE_DIR@', str(self.bin))).resolve()
            require(path.is_relative_to(self.prefix / 'lib/void') and path.is_file(), 'Resource escapes prefix or is missing')
            require(digest(path) == expected, f'Native resource hash mismatch: {path}')
            resources[str(path)] = expected
        require(len(resources) >= 7, 'Native Runtime/provider inventory is incomplete')
        linked_resources = set()
        option_operand = False
        for resource in lines[3:]:
            expanded = resource.replace('@VOID_NATIVE_DIR@', str(self.bin))
            require(str(self.build) not in expanded and str(ROOT / '.toolchains/http') not in expanded,
                    'Installed native manifest retains build/dependency path')
            # NativeBuild writes one argv token per line. Option operands include
            # SDK paths, framework names, rpaths and .a-looking exclusion lists;
            # every other bare argument is a file resource, including .so aliases.
            if option_operand:
                option_operand = False
            elif resource in ('-Xlinker', '-framework', '-isystem', '-isysroot', '-arch'):
                option_operand = True
            elif not resource.startswith('-'):
                linked_resources.add(str((self.bin / expanded).resolve()))
        require(not option_operand, 'Native linker option lacks an operand')
        require(linked_resources == set(resources), 'Native link manifest and hashed resource inventory differ')
        if SYSTEM == 'Windows':
            require('-fms-runtime-lib=static' in lines, 'Native Windows static CRT contract missing')
        self.save('native-resources.json', {'driver': lines[1], 'linker_override': lines[2], 'resources': resources})

    def lsp(self, env):
        requests = [
            {'jsonrpc': '2.0', 'id': 1, 'method': 'initialize',
             'params': {'processId': None, 'rootUri': self.project.as_uri(), 'capabilities': {}}},
            {'jsonrpc': '2.0', 'method': 'initialized', 'params': {}},
            {'jsonrpc': '2.0', 'id': 2, 'method': 'shutdown'},
            {'jsonrpc': '2.0', 'method': 'exit'}]
        result = self.run('lsp', [self.voidls, '--entry', self.project / 'main.void'], cwd=self.project,
                          env=env, stdin=frames(requests), timeout=60)
        responses = unframe(result.stdout)
        self.save('lsp.json', {'requests': requests, 'responses': responses, 'exit_code': result.returncode,
                               'transport': 'stdio Content-Length frames; ordered requests on one connection'})
        require(len(responses) == 2 and all(r.get('jsonrpc') == '2.0' and 'error' not in r for r in responses), 'Unexpected LSP responses')
        require(responses[0].get('id') == 1 and responses[0]['result']['capabilities']['experimental']['voidToolingProtocol'] == 1,
                'LSP initialize/protocol mismatch')
        require(responses[1].get('id') == 2 and 'result' in responses[1] and responses[1]['result'] is None, 'LSP shutdown mismatch')
        require(not result.stderr, 'Unexpected LSP stderr')

    def runtime(self, env):
        for binary in (self.voidc, self.voidls, self.native):
            label = binary.stem
            if SYSTEM == 'Linux':
                loaded = decode(self.run('ldd-' + label, ['ldd', binary], cwd=self.project, env=env).stdout)
                self.run('elf-' + label, ['readelf', '-h', '-d', binary], cwd=self.project, env=env)
                require('not found' not in loaded, f'Unresolved ELF dependency: {binary}')
                require(str(self.build) not in loaded and str(ROOT / '.toolchains/http') not in loaded, 'ELF loader uses build dependencies')
                if binary != self.voidls:
                    for family in ('libidn2.so.', 'libunistring.so.'):
                        rows = [line for line in loaded.splitlines() if line.strip().startswith(family)]
                        require(len(rows) == 1 and ' => ' in rows[0], f'Missing installed {family}')
                        path = Path(rows[0].split(' => ', 1)[1].rsplit(' (', 1)[0])
                        require(path.resolve().is_relative_to(self.prefix / 'lib/void'), f'{family} loaded outside prefix')
            elif SYSTEM == 'Darwin':
                arch = self.run('macho-arch-' + label, ['lipo', '-archs', binary], cwd=self.project, env=env)
                require(arch.stdout.strip() == b'arm64', 'Installed/native Mach-O must be ARM64')
                linked = self.run('macho-imports-' + label, ['otool', '-L', binary], cwd=self.project, env=env)
                self.run('macho-load-' + label, ['otool', '-l', binary], cwd=self.project, env=env)
                for line in decode(linked.stdout).splitlines()[1:]:
                    dependency = line.strip().split(' (', 1)[0]
                    require(dependency.startswith(('/usr/lib/', '/System/Library/')), f'Unexpected non-system Mach-O dependency: {dependency}')
            else:
                pe = self.run('pe-' + label, [self.sdk / 'bin/llvm-readobj.exe', '--file-headers', '--coff-imports', binary], cwd=self.project, env=env)
                text = decode(pe.stdout)
                require('IMAGE_FILE_MACHINE_AMD64' in text, 'Installed/native PE must be x64')
                imports = re.findall(r'^\s*Name: (\S+\.dll)\s*$', text, re.I | re.M)
                require(imports, 'No PE import inventory found')
                system32 = Path(os.environ['SystemRoot']) / 'System32'
                resolved = {}
                for dll in imports:
                    if dll.lower().startswith(('api-ms-win-', 'ext-ms-')):
                        resolved[dll] = 'Windows API-set contract (loader verified by execution)'
                    else:
                        path = system32 / dll
                        require(path.is_file(), f'PE import is not an available Windows system DLL: {dll}')
                        resolved[dll] = {'path': str(path), 'sha256': digest(path)}
                self.save('pe-imports-' + label + '.json', resolved)

    def smoke(self):
        self.check('layout', self.layout)
        self.project.mkdir()
        env = os.environ.copy()
        for key in ('VOID_STDLIB_ROOT', 'LD_LIBRARY_PATH', 'LD_PRELOAD', 'DYLD_LIBRARY_PATH', 'DYLD_FALLBACK_LIBRARY_PATH', 'DYLD_INSERT_LIBRARIES'):
            env.pop(key, None)
        env['PATH'] = str(self.bin) + os.pathsep + env['PATH']
        for executable in (self.voidc, self.voidls):
            require(Path(shutil.which(executable.name, path=env['PATH'])).resolve() == executable.resolve(), 'Installed PATH discovery mismatch')
        source = self.project / 'main.void'
        held = ROOT / '.install-qualification-stdlib-held'  # Same filesystem for atomic rename.
        require(not held.exists(), 'Refusing to overwrite held source stdlib')
        (ROOT / 'stdlib').rename(held)
        try:
            self.save('discovery-isolation.json', {'source_fallback_absent': not (ROOT / 'stdlib').exists(),
                      'project': str(self.project), 'prefix': str(self.prefix), 'stdlib_override': env.get('VOID_STDLIB_ROOT')})
            source.write_text(MINIMAL, encoding='utf-8', newline='\n')
            self.check('minimal-check', lambda: self.run('minimal-check', [self.voidc, 'check', source], cwd=self.project, env=env))
            self.check('minimal-run', lambda: self.run('minimal-run', [self.voidc, 'run', source], cwd=self.project, env=env, expected=b''))
            source.write_text(CONSOLE, encoding='utf-8', newline='\n')
            self.check('console-check', lambda: self.run('console-check', [self.voidc, 'check', source], cwd=self.project, env=env))
            self.check('console-run', lambda: self.run('console-run', [self.voidc, 'run', source], cwd=self.project, env=env, expected=OUTPUT))
            self.check('native-build', lambda: self.run('native-build', [self.voidc, 'build', source, '-o', self.native], cwd=self.project, env=env))
            self.check('native-run', lambda: self.run('native-run', [self.native], cwd=self.project, env=env, expected=OUTPUT))
            def discover():
                result = self.run('discovery', [self.voidls, '--discover', self.project, source, ''], cwd=self.project, env=env)
                selected = json.loads(result.stdout)
                self.save('discovery.json', selected)
                require(Path(selected['entry']).resolve() == source.resolve() and Path(selected['root']).resolve() == self.project, 'Installed server selected another project')
            self.check('discovery', discover)
            self.check('lsp', lambda: self.lsp(env))
            self.check('runtime', lambda: self.runtime(env))
        finally:
            held.rename(ROOT / 'stdlib')

    def report(self):
        # Run even after dependency/configure/acceptance failure. Never infer PASS
        # from absent records or a skipped Actions step.
        for pattern in ('CMakeCache.txt', 'CMakeFiles/CMakeConfigureLog.yaml', 'CMakeFiles/CMakeError.log', 'CMakeFiles/CMakeOutput.log', 'install_manifest*.txt'):
            for path in self.build.glob(pattern):
                shutil.copy2(path, self.evidence / path.name)
        for folder in (ROOT / '.toolchains/llvm-23.1.4-deps-build', ROOT / '.toolchains/http/build'):
            for path in folder.glob('**/CMakeFiles/CMakeConfigureLog.yaml'):
                target = self.evidence / 'dependency-configure' / path.relative_to(ROOT / '.toolchains')
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, target)
        identity = self.sdk / 'void-sdk-identity.json'
        if identity.is_file():
            shutil.copy2(identity, self.evidence / 'llvm-sdk-identity.json')
        for path in (ROOT / 'docs/net/http/evidence').glob('dependencies-*'):
            if path.is_dir() and not subprocess.check_output(['git', 'ls-files', '--', str(path)], cwd=ROOT):
                shutil.copytree(path, self.evidence / path.name, dirs_exist_ok=True)
        if self.prefix.exists():
            self.save('installed-files.json', {p.relative_to(self.prefix).as_posix(): {'sha256': digest(p), 'bytes': p.stat().st_size}
                for p in sorted(self.prefix.rglob('*')) if p.is_file()})
        if self.native.is_file():
            self.save('native-program.json', {'path': str(self.native), 'sha256': digest(self.native), 'bytes': self.native.stat().st_size})
        results = {}
        for name in REQUIRED:
            path = self.evidence / ('check-' + name + '.json')
            results[name] = json.loads(path.read_text(encoding='utf-8'))['status'] if path.exists() else 'SKIP'
        prior = os.environ.get('INSTALL_PRIOR_JOB_STATUS', 'unknown')
        status = ('FAIL' if 'FAIL' in results.values() or prior == 'failure' else
                  'PASS' if all(s == 'PASS' for s in results.values()) and prior == 'success' else 'UNKNOWN')
        self.save('summary.json', {'platform': SYSTEM, 'runner_label': os.environ.get('INSTALL_RUNNER_LABEL'),
            'status': status, 'checks': results, 'prior_actions_job_status': prior,
            'scope': 'Hosted Release source build/install/use on this runner; no independent physical-machine portability',
            'excluded': ['CTest', 'sanitizers', 'benchmarks', 'HTTP/HTTPS protocol campaigns', 'historical LLVM crash reproductions']})
        self.save('evidence-sha256.json', {p.relative_to(self.evidence).as_posix(): digest(p)
            for p in sorted(self.evidence.rglob('*')) if p.is_file() and p.name != 'evidence-sha256.json'})
        summary = os.environ.get('GITHUB_STEP_SUMMARY')
        if summary:
            with open(summary, 'a', encoding='utf-8') as stream:
                stream.write(f'### {SYSTEM} / Release installation: {status}\n\n| Check | Status |\n| --- | --- |\n')
                stream.writelines(f'| {name} | {value} |\n' for name, value in results.items())
                stream.write('\nHosted-runner evidence only; independent physical-machine portability remains UNKNOWN.\n')
        require(status == 'PASS', f'{SYSTEM} installation status: {status}')


def main():
    phase = sys.argv[1]
    q = Qualification()
    if phase == 'identity':
        q.work.mkdir(parents=True, exist_ok=False)
    q.evidence.mkdir(parents=True, exist_ok=True)
    if phase not in ('identity', 'report'):
        gate = q.evidence / 'check-identity.json'
        require(gate.is_file() and json.loads(gate.read_text(encoding='utf-8'))['status'] == 'PASS',
                'A passing disposable-host identity gate is required before any work')
    actions = {'identity': q.identity, 'host': q.host, 'sdk': q.sdk_install, 'dependencies': q.dependencies,
               'configure': q.configure, 'build': lambda: q.run('build', ['cmake', '--build', q.build, '--target', 'voidc', 'voidls', '--parallel', '3']),
               'install': q.install, 'smoke': q.smoke, 'report': q.report}
    if phase == 'report':
        actions[phase]()  # The evidence seal must be the last write in the artifact.
    else:
        q.check(phase, actions[phase])


if __name__ == '__main__':
    main()
