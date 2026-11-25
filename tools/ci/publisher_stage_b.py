"""Focused Stage B qualification orchestration; no production publishing authority.

Run only on the reviewed native macOS ARM64 CI checkout. Reuses existing pinned
SDK/provider recipes and validators. All new build/output/evidence is disposable.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import signal
import shlex
import shutil
import struct
import subprocess
import sys
import time
import traceback
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import http_bootstrap as http
import verify_host_deps as host

BASELINE = 'df33af0a7e8ae75b97d2ab27395a683b69d3994f'
PATCH_SHA256 = '2644e3a94758a843bb2ebc3b2ab03c22accc2c83aea5fc41ed1baaa80bdf1f5d'
POLICY = ROOT / 'tools/ci/publisher-stage-b'
TARGETS = ('voidc', 'voidls', 'void_publish_foundation', 'void_project_header',
           'void_project_manifest', 'void_project_selection', 'void_private_diagnostics',
           'void_runtime_tests', 'void_runtime_generated', 'void_runtime_callbacks', 'void_runtime_failures',
           'void_runtime_aot_normal', 'void_runtime_aot_optimized', 'void_runtime_invariants',
           'void_project_creation', 'void_macos_package_image')
TESTS = ('publish_contracts', 'publish_binary_fixtures', 'project_manifest_header',
         'project_manifest', 'project_manifest_legacy', 'project_selection',
         'project_discovery_cli', 'project_profiles', 'project_protocol', 'cli_contract',
         'publish_private_diagnostics', 'cli_presentation',
         'runtime_coordination', 'runtime_generated', 'runtime_callbacks', 'runtime_failures',
         'runtime_aot_normal', 'runtime_aot_optimized', 'runtime_invariants',
         'project_creation', 'project_application_package', 'project_macos_package_image')
CI_PATHS = {'.github/workflows/ci-link-experiment.yml',
            '.github/workflows/ci-publisher-stage-b-macos.yml',
            'tools/ci/publisher_stage_b.py',
            'tools/ci/publisher-stage-b/native-targets.cmake',
            'tools/ci/publisher-stage-b/identity.json',
            'tools/ci/publisher-stage-b/candidate.patch',
            'tools/ci/publisher-stage-b/test_qualification.py',
            'tools/ci/publisher-stage-b/protocol_repro.py',
            'tools/ci/publisher-stage-b/private_native.py'}
# Preserve both historical reports on case-insensitive Mac checkouts.
REPORT_RENAMES = {
    'docs/json/evidence/implementation/readonly-Debug.json':
        'docs/json/evidence/implementation/readonly-final-Debug.json',
    'docs/json/evidence/implementation/readonly-Release.json':
        'docs/json/evidence/implementation/readonly-final-Release.json',
}
BYTE_LIMIT = 64 * 1024 * 1024


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def save(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


def snapshot(root):
    return {p.relative_to(root).as_posix(): {'link': os.readlink(p)} if p.is_symlink()
            else {'sha256': digest(p)} if p.is_file() else {'directory': True}
            for p in sorted(root.rglob('*'))}


def macho_dependencies(needed_output, id_output, image):
    # Pinned llvm-readobj includes LC_ID_DYLIB (LLVM issue #39505).
    # llvm-objdump --dylib-id selects only that command. Subtract each ID
    # once, preserving any LC_LOAD_DYLIB with the same name and duplicates.
    names = re.search(rb'NeededLibraries \[(.*?)\]', needed_output, re.S)
    lines = id_output.decode().splitlines()
    if names is None or not lines or lines[0] != str(image) + ':':
        raise RuntimeError('Malformed LLVM Mach-O dependency oracle')
    dependencies = [line.strip().decode() for line in names[1].splitlines() if line.strip()]
    for name in lines[1:]:
        if name not in dependencies:
            raise RuntimeError('LLVM Mach-O ID missing from needed-libs oracle')
        dependencies.remove(name)
    return sorted(dependencies)


def verify_candidate(root=ROOT):
    identity = json.loads((root / 'tools/ci/publisher-stage-b/identity.json').read_text())
    if identity['baseline'] != BASELINE or identity['patch_sha256'] != PATCH_SHA256:
        raise RuntimeError('Stage B reviewed identity mismatch')
    if len(identity['source_sha256']) != 30 or any(p.startswith('editors/') for p in identity['source_sha256']):
        raise RuntimeError('Stage B inventory must contain exactly 30 paths and no editor changes')
    if digest(root / 'tools/ci/publisher-stage-b/candidate.patch') != PATCH_SHA256:
        raise RuntimeError('Stage B patch checksum mismatch')
    for name, expected in identity['source_sha256'].items():
        if digest(root / name) != expected:
            raise RuntimeError('Stage B source checksum mismatch: ' + name)
    return identity


def validate_registration(document, *, built=True):
    tests = document['tests']
    if sorted(t['name'] for t in tests) != sorted(TESTS):
        raise RuntimeError('Exact focused CTest selection mismatch')
    for test in tests:
        props = {p['name']: p['value'] for p in test.get('properties', [])}
        if (built and not test.get('command')) or props.get('DISABLED') or any(
                key in props for key in ('SKIP_RETURN_CODE', 'SKIP_REGULAR_EXPRESSION')):
            raise RuntimeError('Missing command, disabled test or skip condition: ' + test['name'])
    return tests


def validate_junit(path):
    suite = ET.parse(path).getroot()
    cases = suite.findall('.//testcase')
    if sorted(c.get('name') for c in cases) != sorted(TESTS):
        raise RuntimeError('JUnit test selection mismatch')
    if suite.get('tests') != str(len(TESTS)) or any(int(suite.get(k, '0')) for k in ('failures', 'errors', 'disabled', 'skipped')):
        raise RuntimeError('CTest failures or nonexecuted tests')
    if any(c.find('failure') is not None or c.find('error') is not None or c.find('skipped') is not None
           or c.get('status') in ('notrun', 'disabled', 'fail') for c in cases):
        raise RuntimeError('CTest case did not pass')
    return {c.get('name'): c.get('time') for c in cases}


class Qualification:
    def __init__(self, profile, work):
        self.profile = profile
        self.work = work.resolve()
        self.build = self.work / 'build'
        self.evidence = self.work / 'evidence'
        self.sdk = ROOT / '.toolchains/llvm-23.1.4'
        self.evidence.mkdir(parents=True, exist_ok=True)

    def run(self, label, argv, *, expected=0, timeout=600, env=None, cwd=None):
        argv = list(map(str, argv))
        start = time.monotonic()
        record = dict(argv=argv, cwd=str(cwd or ROOT))
        deadline = self.work / 'deadline.json'
        if deadline.exists():
            remaining = json.loads(deadline.read_text())['execution_deadline'] - time.time()
            if remaining <= 0:
                raise RuntimeError('40-minute execution budget exhausted; reserve final five minutes for evidence')
            timeout = min(timeout, remaining)
        print('Running ' + label, flush=True)
        try:
            with (self.evidence / (label + '.stdout')).open('wb') as out, (self.evidence / (label + '.stderr')).open('wb') as err:
                process = subprocess.Popen(argv, cwd=cwd or ROOT, env=env, stdout=out, stderr=err, start_new_session=True)
                try:
                    status = process.wait(timeout=timeout)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL)
                    process.wait()
                    raise
            result = subprocess.CompletedProcess(argv, status,
                (self.evidence / (label + '.stdout')).read_bytes(), (self.evidence / (label + '.stderr')).read_bytes())
            record['exit'] = result.returncode
            if result.returncode != expected:
                raise RuntimeError(f'{label}: expected {expected}, got {result.returncode}; see evidence')
            return result
        except subprocess.TimeoutExpired as error:
            record['timeout'] = timeout
            raise
        finally:
            record['seconds'] = time.monotonic() - start
            save(self.evidence / (label + '.command.json'), record)
            print(label, record.get('exit', 'incomplete'), flush=True)

    def source(self):
        save(self.work / 'deadline.json', dict(execution_deadline=time.time() + 40 * 60))
        if (platform.system(), platform.machine()) != ('Darwin', 'arm64'):
            raise RuntimeError('Native Apple Silicon execution required; no cross-host substitute')
        if os.environ.get('RUNNER_ENVIRONMENT') != 'github-hosted' or os.environ.get('RUNNER_ARCH') != 'ARM64':
            raise RuntimeError('Expected a native GitHub-hosted ARM64 runner')
        if os.environ.get('GITHUB_EVENT_NAME') != 'workflow_dispatch':
            raise RuntimeError('Only reviewed manual dispatch is authorized')
        identity = verify_candidate()
        head = self.run('source-head', ['git', 'rev-parse', 'HEAD']).stdout.decode().strip()
        reviewed = os.environ.get('PUBLISH_REVIEWED_SHA', '')
        if not re.fullmatch('[0-9a-f]{40}', reviewed) or reviewed != head or head != os.environ.get('GITHUB_SHA'):
            raise RuntimeError('Reviewed SHA, event SHA and checkout must be identical')
        if os.environ.get('GITHUB_REF') != 'refs/heads/codex/publisher-stage-b-macos-ci':
            raise RuntimeError('Use only the isolated qualification branch')
        self.run('source-baseline', ['git', 'merge-base', '--is-ancestor', BASELINE, head])
        names = set(self.run('source-inventory', ['git', 'diff', '--no-renames', '--name-only', BASELINE, head]).stdout.decode().splitlines())
        rename_paths = set(REPORT_RENAMES) | set(REPORT_RENAMES.values())
        if names != set(identity['source_sha256']) | CI_PATHS | rename_paths:
            raise RuntimeError('Commit inventory includes missing or unauthorized changes: ' + repr(names))
        tracked = self.run('source-paths', ['git', 'ls-files', '-z']).stdout.decode().split('\0')
        for old, new in REPORT_RENAMES.items():
            original = self.run('source-report-' + Path(old).stem,
                                ['git', 'show', BASELINE + ':' + old]).stdout
            if old in tracked or (ROOT / new).read_bytes() != original:
                raise RuntimeError('Historical report rename changed content: ' + new)
        folded = {}
        for name in filter(None, tracked):
            if name.casefold() in folded:
                raise RuntimeError('Case-conflicting tracked paths: ' + folded[name.casefold()] + ', ' + name)
            folded[name.casefold()] = name
        if self.run('source-clean', ['git', 'status', '--porcelain']).stdout:
            raise RuntimeError('Qualification checkout must be clean')
        self.run('source-reverse-patch', ['git', 'apply', '--check', '--reverse', POLICY / 'candidate.patch'])
        # Select an installed vendor SDK with C++20 stop_token support. Never install Xcode.
        selected = None
        candidates = [Path(os.environ.get('DEVELOPER_DIR', '/Applications/Xcode.app/Contents/Developer'))]
        candidates += sorted(Path('/Applications').glob('Xcode_*.app/Contents/Developer'), reverse=True)
        for index, developer in enumerate(dict.fromkeys(candidates)):
            if not developer.is_dir():
                continue
            env = dict(os.environ, DEVELOPER_DIR=str(developer))
            result = self.run(f'xcode-{index}', ['xcodebuild', '-version'], env=env)
            if re.match(rb'Xcode (2[6-9]|[3-9][0-9])\.', result.stdout):
                sdk = self.run('apple-sdk', ['xcrun', '--sdk', 'macosx', '--show-sdk-path'], env=env).stdout.decode().strip()
                selected = dict(DEVELOPER_DIR=str(developer), SDKROOT=str(Path(sdk).resolve()))
                break
        if not selected:
            raise RuntimeError('An existing Xcode 26+ SDK is required by the vendor libc++ contract')
        with open(os.environ['GITHUB_ENV'], 'a', encoding='utf-8') as output:
            output.write(f'PUBLISH_WORK={self.work}\n')
            for key, value in selected.items():
                output.write(f'{key}={value}\n')
            scratch = self.work / 'tmp'
            scratch.mkdir(exist_ok=True)
            output.write(f'TMPDIR={scratch}/\n')
        self.run('host-os', ['sw_vers'])
        self.run('host-hardware', ['sysctl', '-n', 'hw.memsize'])
        self.run('host-cmake', ['cmake', '--version'])
        self.run('host-ninja', ['ninja', '--version'])
        save(self.evidence / 'source-identity.json', dict(commit=head, candidate=identity, apple=selected,
             profile=self.profile, disk=shutil.disk_usage(ROOT)._asdict(), targets=TARGETS, tests=TESTS))
        receipts = [p.relative_to(ROOT).as_posix() for base in ('docs/net/http/evidence', 'docs/voidup/evidence')
                    for p in (ROOT / base).glob('dependencies-*')]
        save(self.work / 'existing-receipts.json', receipts)

    def http_identity(self):
        plan = http.make_plan(self.profile)
        self.run('curl-source', [sys.executable, ROOT / 'tools/verify_curl_source.py'])
        save(self.work / 'http-plan.json', plan)
        with open(os.environ['GITHUB_OUTPUT'], 'a', encoding='utf-8') as output:
            for key in ('prefix', 'deps-key', 'sources-key'):
                output.write(f'{key}={plan[key]}\n')

    def http(self):
        plan = json.loads((self.work / 'http-plan.json').read_text())
        prefix = ROOT / plan['prefix']
        cached = os.environ.get('HTTP_DEPS_CACHE_HIT') == 'true'
        if cached:
            selected = {'nghttp2', 'c-ares', 'zlib', 'libpsl', 'aws-lc'}
            pins = json.loads((ROOT / 'third_party/http-versions.json').read_text())['dependencies']
            self.acquire_sources([pin for pin in pins if pin['name'] in selected], ROOT / '.toolchains/http/downloads')
        if not cached:
            # Even the primary key can prefix-match without restore-keys.
            # A non-exact restore is disposable, never a reusable prefix.
            if prefix.exists():
                shutil.rmtree(prefix)
            self.run('http-build', [sys.executable, ROOT / 'tools/setup_http_deps.py', '--profile', self.profile,
                                   '--jobs', '2', '--macos-install-only'], timeout=1800)
        manifest = http.verify_prefix(plan, self.evidence, cached=cached)
        if not cached:
            http.save(prefix, 'VOID-HTTP-CI-IDENTITY.json', dict(identity=plan['identity'], manifest_sha256=digest(manifest)))
        save(self.evidence / 'http-result.json', dict(cache_hit=cached, plan=plan, verified=True))

    def acquire_sources(self, pins, directory):
        # Source-cache eviction must not force rebuilding a valid library prefix.
        # Reuse the existing hash-checking downloader; never accept ambient sources.
        code = ('import json,sys; sys.path.insert(0,"tools"); '
                'from http_archive import acquire; '
                'acquire(json.loads(sys.argv[1]),sys.argv[2],sys.argv[3])')
        for index, pin in enumerate(pins):
            archive = directory / pin.get('archive', pin['url'].rsplit('/', 1)[-1])
            self.run('source-' + directory.parent.name + '-' + str(index),
                     [sys.executable, '-B', '-c', code, json.dumps(pin), archive, self.evidence / 'source-acquisition'], timeout=300)

    def host_identity(self):
        plan = http.make_plan(self.profile)
        prefix = f'.toolchains/host/prefix/macos-arm64/{self.profile}'
        zlib = ROOT / plan['prefix'] / 'lib/libz.a'
        identity = dict(http=plan['identity'], zlib_sha256=digest(zlib), recipes=host.recipe_hashes(),
                        pins=digest(ROOT / 'third_party/host-versions.json'))
        plan = dict(prefix=prefix, identity=identity,
                    sources_key='host-sources-v1-' + identity['pins'],
                    deps_key=f'publisher-host-v1-Darwin-arm64-{self.profile}-' + http.key(identity))
        save(self.work / 'host-plan.json', plan)
        with open(os.environ['GITHUB_OUTPUT'], 'a', encoding='utf-8') as output:
            for key, source in [('prefix', 'prefix'), ('sources-key', 'sources_key'), ('deps-key', 'deps_key')]:
                output.write(f'{key}={plan[source]}\n')

    def host(self):
        plan = json.loads((self.work / 'host-plan.json').read_text())
        prefix = ROOT / plan['prefix']
        cached = os.environ.get('HOST_DEPS_CACHE_HIT') == 'true'
        if cached:
            pins = json.loads((ROOT / 'third_party/host-versions.json').read_text())
            self.acquire_sources([pins[name] for name in ('libsodium', 'libarchive')], ROOT / '.toolchains/host/downloads')
        seal = prefix / 'VOID-PUBLISHER-CI-IDENTITY.json'
        if not cached:
            if prefix.exists():
                shutil.rmtree(prefix)
            self.run('host-build', [sys.executable, ROOT / 'tools/setup_host_deps.py', '--profile', self.profile,
                                   '--jobs', '2'], timeout=1200)
        receipt = host.verify_prefix(prefix, self.profile, 'Darwin', 'arm64')
        http_plan = http.make_plan(self.profile)
        zlib = ROOT / http_plan['prefix'] / 'lib/libz.a'
        if Path(receipt['zlib']).resolve() != zlib.resolve() or receipt['zlib_sha256'] != digest(zlib):
            raise RuntimeError('Host and HTTP zlib closure mismatch')
        h = http_plan['identity']['host']
        if Path(receipt['sdk']).resolve() != Path(h['sdk']).resolve() or receipt['deployment_target'] != h['deployment_target']:
            raise RuntimeError('Host Apple SDK/deployment mismatch')
        expected = dict(identity=plan['identity'], manifest_sha256=digest(prefix / 'VOID-HOST-DEPENDENCIES.json'))
        if cached and json.loads(seal.read_text()) != expected:
            raise RuntimeError('Host cache identity/manifest mismatch')
        if not cached:
            save(seal, expected)
        save(self.evidence / 'host-result.json', dict(cache_hit=cached, plan=plan, receipt=receipt, verified=True))

    def configure(self):
        lld = self.sdk / 'bin/ld64.lld'
        for name in ('clang', 'clang++', 'ld64.lld', 'llvm-ar', 'llvm-ranlib'):
            result = self.run('version-' + name.replace('+', 'x'), [self.sdk / 'bin' / name, '--version'])
            if not re.search(rb'\b23\.1\.4(?:\D|$)', result.stdout + result.stderr):
                raise RuntimeError('Pinned LLVM tool version mismatch: ' + name)
        thin = False
        apple_ld = ''
        decision = dict(requested=self.profile == 'Release', enabled=False,
                        intended_targets='compiler and qualification harness only; Runtime/native resources excluded')
        if self.profile == 'Release':
            apple_ld = self.run('apple-linker', ['xcrun', '--find', 'ld']).stdout.decode().strip()
            if not Path(apple_ld).is_absolute() or not Path(apple_ld).is_file():
                raise RuntimeError('Selected Xcode linker unavailable')
            decision['linker'] = apple_ld
            decision['libLTO'] = str(self.sdk / 'lib/libLTO.dylib')
            if not Path(decision['libLTO']).is_file():
                raise RuntimeError('Pinned LLVM libLTO unavailable')
            probe = self.work / 'thinlto-probe'
            probe.mkdir()
            (probe / 'value.cpp').write_text('#include <stdexcept>\n__attribute__((noinline)) int answer(int n) { if(n!=23) throw std::invalid_argument("stale version"); return n; }\n')
            (probe / 'main.cpp').write_text('#include <stdexcept>\nextern int answer(int); int main(int argc,char**) { try { answer(argc); } catch(const std::invalid_argument&) { return answer(23)==23 ? 0 : 1; } return 2; }\n')
            try:
                for stem in ('value', 'main'):
                    self.run('thin-compile-' + stem, [os.environ['CXX'], '-O3', '-fno-rtti', '-flto=thin', '-stdlib=libc++',
                        '-nostdinc++', '-isystem', str(Path(os.environ['SDKROOT']) / 'usr/include/c++/v1'), '-isysroot', os.environ['SDKROOT'],
                        '-mmacosx-version-min=14.0', '-c', probe / (stem + '.cpp'), '-o', probe / (stem + '.o')])
                self.run('thin-link', [os.environ['CXX'], '-fuse-ld=ld', '--ld-path=' + apple_ld, '-flto=thin', '-Wl,-mllvm,-threads=2',
                    '-Wl,-lto_library,' + decision['libLTO'],
                    '-Wl,-cache_path_lto,' + str(probe / 'cache'), '-isysroot', os.environ['SDKROOT'],
                    '-mmacosx-version-min=14.0', probe / 'main.o', probe / 'value.o', '-o', probe / 'probe'])
                self.run('thin-native-execution', [probe / 'probe'])
                thin = True
            except (RuntimeError, subprocess.TimeoutExpired) as error:
                decision['failure'] = str(error)
                save(self.evidence / 'thinlto-decision.json', decision)
                raise  # This repair must qualify ThinLTO, never a non-LTO fallback.
        decision['enabled'] = thin
        save(self.evidence / 'thinlto-decision.json', decision)
        cache = self.work / 'thinlto-cache'
        cache.mkdir(exist_ok=True)
        command = ['cmake', '-S', ROOT, '-B', self.build, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=' + self.profile,
            '-DBUILD_TESTING=ON', '-DVOID_SANITIZERS=OFF', '-DVOID_LLVM_RESEARCH=OFF',
            '-DCMAKE_C_COMPILER=' + os.environ['CC'], '-DCMAKE_CXX_COMPILER=' + os.environ['CXX'],
            '-DLLVM_DIR=' + os.environ['LLVM_DIR'], '-DVOID_CLANG=' + os.environ['VOID_CLANG'], '-DVOID_LINKER=',
            '-DCMAKE_OSX_ARCHITECTURES=arm64', '-DCMAKE_OSX_SYSROOT=' + os.environ['SDKROOT'],
            '-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0', '-DCMAKE_AR=' + str(self.sdk / 'bin/llvm-ar'),
            '-DCMAKE_RANLIB=' + str(self.sdk / 'bin/llvm-ranlib'),
            '-DCMAKE_PROJECT_VoidLanguage_INCLUDE=' + str(POLICY / 'native-targets.cmake'),
            '-DPUBLISH_STAGE_A_THINLTO=' + ('ON' if thin else 'OFF'), '-DPUBLISH_STAGE_A_LLD=' + str(lld),
            '-DPUBLISH_STAGE_A_APPLE_LD=' + apple_ld,
            '-DPUBLISH_STAGE_A_LIBLTO=' + str(self.sdk / 'lib/libLTO.dylib'),
            '-DPUBLISH_STAGE_A_LTO_CACHE=' + str(cache)]
        self.run('configure', command, timeout=600)
        selector = '^(' + '|'.join(TESTS) + ')$'
        result = self.run('registration', ['ctest', '--test-dir', self.build, '-R', selector, '--show-only=json-v1'])
        validate_registration(json.loads(result.stdout), built=False)
        graph = self.run('ninja-targets', ['ninja', '-C', self.build, '-t', 'targets', 'all']).stdout.decode()
        if any(not re.search(r'^' + re.escape(name) + r':', graph, re.M) for name in TARGETS):
            raise RuntimeError('A required focused build target is missing')
        commands = json.loads((self.build / 'compile_commands.json').read_text())
        selected = [row for row in commands if '/void_publish.dir/' in row['command'] or '/voidc_objects.dir/' in row['command']]
        if not selected or any(('-flto=thin' in row['command']) != thin for row in selected):
            raise RuntimeError('Compiler/auditor ThinLTO compile flags inconsistent')
        resources = [row for row in commands if '/void_runtime.dir/' in row['command'] or '/void_native_fixture.dir/' in row['command']]
        if not resources or any('-flto' in row['command'] for row in resources):
            raise RuntimeError('Runtime/native resources must retain ordinary machine-code compilation')
        save(self.evidence / 'configure-contract.json', dict(tests=TESTS, targets=TARGETS, thinlto=thin,
             runtime_native_resources_non_lto=True, automatic_signing=False, production_publishing=False))

    def build_targets(self):
        self.run('build', ['cmake', '--build', self.build, '--parallel', '2', '--target', *TARGETS], timeout=1800)
        self.run('build-commands', ['ninja', '-C', self.build, '-t', 'commands', *TARGETS])
        self.run('compiler-version', [self.build / 'voidc', '--version'])
        outputs = [self.build / name for name in TARGETS]
        outputs += [p for p in self.build.glob('*.a') if p.is_file()]
        save(self.evidence / 'build-identities.json', {p.name: dict(bytes=p.stat().st_size, sha256=digest(p)) for p in outputs})
        self.run('publisher-members', [self.sdk / 'bin/llvm-ar', 't', self.build / 'libvoid_publish.a'])
        for name in ('cache-inputs-' + self.profile + '.txt', 'cache-identity-' + self.profile + '.cpp',
                     'void-native.txt', 'void-native-resources.txt'):
            shutil.copyfile(self.build / name, self.evidence / name)

    def protocol(self):
        self.run('protocol-independent', [sys.executable, '-B', POLICY / 'protocol_repro.py',
                 ROOT, self.build / 'voidls', self.evidence / 'protocol-independent'], timeout=180,
                 env=dict(os.environ, VOID_CRASH_DIRECTORY=str(self.evidence / 'protocol-crashes')))

    def tests(self):
        selector = '^(' + '|'.join(TESTS) + ')$'
        result = self.run('active-registration', ['ctest', '--test-dir', self.build, '-R', selector, '--show-only=json-v1'])
        validate_registration(json.loads(result.stdout))
        env = dict(os.environ, VOID_TEST_DIAGNOSTICS_DIR=str(self.work / 'ctest-evidence'))
        self.run('ctest', ['ctest', '--test-dir', self.build, '-R', selector, '--no-tests=error', '-j', '2',
                          '--verbose', '--output-on-failure', '--output-junit', self.evidence / 'ctest.xml'], timeout=900, env=env)
        save(self.evidence / 'tests-passed.json', validate_junit(self.evidence / 'ctest.xml'))

    def private(self):
        self.run('private-native', [sys.executable, '-B', POLICY / 'private_native.py',
                 self.build, self.sdk, self.work / 'private-native'], timeout=900)
        results=json.loads((self.work / 'private-native/results.json').read_text())
        if len(results)!=6:
            raise RuntimeError('All six Stage B native specimens must complete')
        save(self.evidence / 'private-native-passed.json', results)

    def native(self):
        root = self.work / 'native'
        root.mkdir()
        compiler = self.build / 'voidc'
        (root / 'main.void').write_text('func main() -> i32:\n    return 17\n')
        self.run('smoke-check', [compiler, 'check', root / 'main.void'])
        self.run('smoke-jit', [compiler, 'run', root / 'main.void', '--no-cache'], expected=17)
        self.run('smoke-emit', [compiler, 'emit', root / 'main.void', '-o', root / 'void.o'])
        self.run('smoke-build', [compiler, 'build', root / 'main.void', '-o', root / 'void-app'])
        self.run('smoke-native-execution', [root / 'void-app'], expected=17)
        package_source = root / 'package-source'
        self.run('package-new', [compiler, 'new', package_source])
        manifest = package_source / 'void.project.toml'
        manifest.write_text(manifest.read_text().replace('[application]\n',
            '[application]\nresources=["assets/message.txt"]\n'))
        (package_source / 'assets').mkdir()
        (package_source / 'assets/message.txt').write_text('macOS packaged resource')
        (package_source / 'src/app.void').write_text('use os\nuse io\nfunc main() -> i32:\n'
            '    if os.resource_dir() is Ok(var directory):\n'
            '        if io.read_text(directory + "/assets/message.txt") is Ok(var text):\n'
            '            if text == "macOS packaged resource":\n'
            '                return 17\n'
            '    return 23\n')
        packaged = root / 'packaged'
        self.run('package-build', [compiler, 'package', '--project', package_source,
                 '--profile', self.profile.lower(), '-o', packaged])
        self.run('package-verify', [compiler, 'package', '--verify', packaged])
        package_source.rename(root / 'parked-package-source')
        moved = root / 'relocated-package'
        packaged.rename(moved)
        self.run('package-relocated-verify', [compiler, 'package', '--verify', moved])
        self.run('package-image-policy', [self.build / 'void_macos_package_image', moved / 'app'])
        self.run('package-relocated-native-execution', [moved / 'app'], expected=17)
        (root / 'producer.c').write_text('#include <stdio.h>\nconst char *stage_b_canary(void) { return "stage-b-native-canary"; }\nint main(void) { puts(stage_b_canary()); return 0; }\n')
        flags = ['-arch', 'arm64', '-isysroot', os.environ['SDKROOT'], '-mmacosx-version-min=14.0', '-g', '-O0']
        self.run('macho-producer-executable', [os.environ['CC'], *flags, root / 'producer.c', '-o', root / 'producer'])
        self.run('macho-producer-library', [os.environ['CC'], *flags, '-dynamiclib', root / 'producer.c',
            '-Wl,-install_name,@rpath/libstage-b.dylib', '-Wl,-rpath,@loader_path', '-o', root / 'libstage-b.dylib'])
        reports = {}
        auditor = self.build / 'void_publish_foundation'
        images = [self.build / 'voidc', self.build / 'voidls', root / 'void-app', root / 'producer', root / 'libstage-b.dylib']
        for image in images:
            label = image.name
            oracle = self.run(label + '-oracle', [self.sdk / 'bin/llvm-readobj', '--file-headers', '--needed-libs', '--sections', image])
            arch = self.run(label + '-arch', ['xcrun', 'lipo', '-archs', image]).stdout.decode().strip()
            if arch != 'arm64' or b'Mach-O' not in oracle.stdout:
                raise RuntimeError('Actual native Mach-O ARM64 producer expected: ' + label)
            expected = 2 if image.stat().st_size > BYTE_LIMIT else 0
            result = self.run(label + '-audit', [auditor, 'audit', 'macos-arm64', image, 'stage-b-native-canary'], expected=expected)
            report = json.loads(result.stdout) if expected == 0 else None
            reports[label] = dict(bytes=image.stat().st_size, sha256=digest(image), report=report, harness_over_byte_limit=expected == 2)
            if expected == 2:
                if result.stdout:
                    raise RuntimeError('Over-budget image must not produce a clean report')
                continue
            if report['sha256'] != digest(image):
                raise RuntimeError('Audited image identity mismatch')
            if image.parent == self.build and not report['inspected'] and report['findings'] and all(
                    finding['code'] == 'limit' for finding in report['findings']):
                # Large developer images may exceed fixed metadata budgets.
                # Preserve the incomplete report; small native producers must complete.
                reports[label]['incomplete_policy_limit'] = True
                continue
            if not report['inspected'] or report['architecture'] != 'aarch64' or report['format'] != 'Mach-O':
                raise RuntimeError('Native Mach-O inspection incomplete: ' + label)
            dylib_id = self.run(label + '-dylib-id',
                [self.sdk / 'bin/llvm-objdump', '--macho', '--dylib-id', image])
            dependencies = macho_dependencies(oracle.stdout, dylib_id.stdout, image)
            if dependencies != sorted(report['dependencies']):
                raise RuntimeError('Native dependency declarations differ from pinned LLVM oracle: ' + label)
            if image.parent == root and image.name != 'void-app':
                if not any(f['code'] == 'forbidden-string' for f in report['findings']):
                    raise RuntimeError('Native producer canary not found')
                raw = image.read_bytes()
                if not any(f['code'] == 'forbidden-string' and f['offset'] == raw.index(b'stage-b-native-canary')
                           for f in report['findings']):
                    raise RuntimeError('Native producer canary offset mismatch')
                # Actual symbol table and strings must be inspected, not merely a synthetic header.
                if not report['symbols'] or not report['dependencies']:
                    raise RuntimeError('Native producer symbol/dependency metadata missing')
                damaged = bytearray(raw)
                struct.pack_into('<I', damaged, 20, len(raw))  # oversized Mach-O load-command area
                path = root / (label + '.bad-commands')
                path.write_bytes(damaged)
                rejected = json.loads(self.run(label + '-bad-commands', [auditor, 'audit', 'macos-arm64', path]).stdout)
                if rejected['inspected'] or not any(f['code'] == 'malformed-image' for f in rejected['findings']):
                    raise RuntimeError('Malformed native Mach-O accepted')
        if not reports['libstage-b.dylib']['report']['loader_paths']:
            raise RuntimeError('Native dylib loader-path declarations missing')
        save(self.evidence / 'native-images.json', reports)

    def rejections(self):
        root = self.work / 'publish-rejections'
        root.mkdir()
        compiler = self.build / 'voidc'
        capability = json.loads(self.run('capabilities', [compiler, '--capabilities']).stdout)
        if capability.get('publishEmission') is not False:
            raise RuntimeError('Stage B emission capability changed')
        approval = (Path.home() / 'Library/Application Support/VOID/native-approvals' if platform.system() == 'Darwin'
                    else Path(os.environ.get('XDG_DATA_HOME', str(Path.home() / '.local/share'))) / 'void/native-approvals')
        # Record the actual user state or absence; never create or modify it.
        before_approval = snapshot(approval) if approval.exists() else None
        base = "schema=1\n[project]\nid='org.test.sources'\nname='Fixture'\nversion='1.0.0'\nkind='application'\n[application]\nid='org.test.app'\n"
        variants = ('', "[dependencies.helper]\nid='org.test.helper'\npath='missing-dependency'\nversion='=1.0.0'\n",
                    "[native-requirements.raylib]\nid='org.void.raylib'\nversion='=1.0.0'\nabi=3\n")
        rows = []
        for index, suffix in enumerate(variants):
            project = root / ('project-' + str(index))
            project.mkdir()
            (project / 'void.project.toml').write_text(base + suffix)
            (project / 'main.void').write_text('intentionally invalid source; publish must not compile\n')
            existing = project / 'existing-output'
            existing.mkdir()
            (existing / 'sentinel').write_bytes(b'preserve Stage B destination')
            before = snapshot(project)
            for target in ('linux-x64', 'windows-x64', 'macos-arm64'):
                for profile in ('release', 'performance', 'protected'):
                    for destination in (project / 'fresh-output', existing):
                        result = self.run(f'reject-{index}-{target}-{profile}-{destination.name}',
                            [compiler, 'publish', '--project', project, '--target', target, '--profile', profile, '-o', destination], expected=1)
                        message = b'fresh directory' if destination == existing else b'Publish capability unavailable'
                        if message not in result.stderr or snapshot(project) != before:
                            raise RuntimeError('Publish rejection contract or state preservation failed')
                        rows.append(dict(variant=index, target=target, profile=profile, existing=destination == existing, exit=1))
            self.run('reject-default-' + str(index), [compiler, 'publish', '--project', project], expected=1)
            if snapshot(project) != before or (project / 'dist').exists():
                raise RuntimeError('Default publishing created state/output')
        self.run('publish-help', [compiler, 'publish', '--help'])
        after_approval = snapshot(approval) if approval.exists() else None
        if before_approval != after_approval:
            raise RuntimeError('Native approvals changed during rejection controls')
        save(self.evidence / 'rejections-passed.json', dict(cases=rows, explicit_invocations=len(rows),
             default_invocations=3, all_targets_profiles_unavailable=True, no_distributable_output=True, approvals_unchanged=True))

    def collect(self):
        receipt_checkpoint = self.work / 'existing-receipts.json'
        before = set(json.loads(receipt_checkpoint.read_text())) if receipt_checkpoint.exists() else set()
        roots = [self.build, self.work / 'native', self.work / 'ctest-evidence', self.work / 'thinlto-probe', self.work / 'private-native']
        roots += [p for base in ('docs/net/http/evidence', 'docs/voidup/evidence')
                  for p in (ROOT / base).glob('dependencies-*')
                  if receipt_checkpoint.exists() and p.relative_to(ROOT).as_posix() not in before]
        for path in self.work.glob('*-plan.json'):
            shutil.copyfile(path, self.evidence / path.name)
        for path in self.work.glob('*.log'):
            shutil.copyfile(path, self.evidence / path.name)
        for index, root in enumerate(roots):
            for path in root.rglob('*'):
                if not path.is_file() or path.is_symlink():
                    continue
                wanted = root in (self.work / 'thinlto-probe', self.work / 'private-native', self.work / 'native') or path.suffix in ('.json', '.log', '.yaml', '.stdout', '.stderr', '.trs') or path.name in (
                    'CMakeCache.txt', 'build.ninja', 'publisher-native-targets.txt', 'producer', 'void-app',
                    'libstage-b.dylib', 'void.o', 'main.void', 'producer.c') or path.name.endswith('.bad-commands')
                if not wanted:
                    continue
                destination = self.evidence / 'collected' / str(index) / path.relative_to(root)
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(path, destination)
        # Retain actual filenames and patches even when verification fails early.
        self.run('collect-status', ['git', 'status', '--porcelain=v1'])
        changed = self.run('collect-tracked-paths', ['git', 'diff', 'HEAD', '--name-only']).stdout
        self.run('collect-tracked-diff', ['git', 'diff', 'HEAD', '--no-ext-diff', '--binary'])
        identity = verify_candidate()
        if changed:
            raise RuntimeError('Tracked checkout differs from HEAD; see collect-tracked-paths.stdout')
        binaries = json.loads((self.evidence / 'build-identities.json').read_text()) if (self.evidence / 'build-identities.json').exists() else {}
        for name, row in binaries.items():
            if digest(self.build / name) != row['sha256']:
                raise RuntimeError('Qualification binary changed after testing: ' + name)
        phases = {name: (self.evidence / name).exists() for name in ('tests-passed.json', 'native-images.json', 'rejections-passed.json', 'private-native-passed.json')}
        save(self.evidence / 'qualification-summary.json', dict(candidate=identity, profile=self.profile,
             completed_phases=phases, native_qualification_passed=all(phases.values()), binaries_unchanged=bool(binaries),
             tracked_source_unchanged=True, production_emission=False, automatic_signing=False))
        save(self.evidence / 'SHA256.json', {p.relative_to(self.evidence).as_posix(): digest(p)
             for p in sorted(self.evidence.rglob('*')) if p.is_file() and p.name != 'SHA256.json'})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('phase', choices=('source', 'http-identity', 'http', 'host-identity', 'host', 'configure', 'build', 'protocol', 'tests', 'native', 'private', 'rejections', 'collect'))
    parser.add_argument('--configuration', choices=('Debug', 'Release'), default=os.environ.get('PUBLISH_CONFIGURATION', 'Release'))
    parser.add_argument('--work', type=Path, default=Path(os.environ.get('PUBLISH_WORK') or
                        str(Path(os.environ.get('RUNNER_TEMP', 'build')) / 'publisher-stage-b')))
    args = parser.parse_args()
    task = Qualification(args.configuration, args.work)
    try:
        getattr(task, args.phase.replace('-', '_') if args.phase != 'build' else 'build_targets')()
    except BaseException:
        (task.evidence / (args.phase + '-failure.txt')).write_text(traceback.format_exc())
        raise


if __name__ == '__main__':
    main()
