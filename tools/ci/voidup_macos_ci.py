"""Focused hosted macOS VOIDUP stages; no complete compiler/test matrix."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

import http_bootstrap as http
from install_smoke import Qualification, ROOT, SYSTEM, digest, require


class MacQualification(Qualification):
    def identity(self):
        super().identity()
        path = self.evidence / 'identity.json'
        record = json.loads(path.read_text())
        record['source_sha256'].update({name: digest(ROOT / name) for name in (
            '.github/workflows/voidup-release.yml', 'tools/ci/voidup_macos_ci.py',
            'tools/ci/voidup_isolation.py', 'tests/voidup/first_run.py',
            'tools/package_voidup_fixtures.py', 'tools/voidup_macos.py', 'CMakeLists.txt',
            'src/runtime/host_directory.cpp', 'src/runtime/host_directory_diagnostic.hpp',
            'tests/voidup/native_directory_diagnostic.py')})
        self.save('identity.json', record)

    def http_plan(self, complete=False):
        plan = http.make_plan('Release')
        if complete:
            return plan
        # Installation omits AWS-LC's auxiliary CLI, so it must never masquerade
        # as the complete HTTP test profile. Source archive keys remain shared.
        plan['identity']['voidup_install_only'] = digest(Path(__file__))
        # A complete key must not prefix an installation-only cache key:
        # Actions also searches primary-key prefixes without restore-keys.
        plan['deps-key'] = 'voidup-install-v1-' + plan['deps-key'] + '-' + http.key(plan['identity'])
        return plan

    def selected_http_plan(self):
        return self.http_plan(complete=os.environ.get('HTTP_FULL_DEPS_CACHE_HIT') == 'true')

    def sdk_install(self):
        # llvm_sdk.py validates the pinned receipt, tools, headers and libraries
        # on both exact cache hits and downloads. Do not weaken that verifier.
        self.run('sdk-bootstrap', [sys.executable, 'tools/ci/llvm_sdk.py'], timeout=1800)
        shutil.copy2(self.sdk / 'void-sdk-identity.json', self.evidence / 'llvm-sdk-identity.json')

    def dependency_identity(self):
        plan = self.http_plan()
        http.run(self.evidence, 'verify-curl-source', [sys.executable, ROOT / 'tools/verify_curl_source.py'])
        self.save('http-cache-plan.json', plan)
        with open(os.environ['GITHUB_OUTPUT'], 'a', encoding='utf-8') as stream:
            for name in ('prefix', 'sources-key', 'deps-key'):
                stream.write(f'{name}={plan[name]}\n')
            stream.write(f'full-deps-key={self.http_plan(complete=True)["deps-key"]}\n')

    def dependencies(self):
        plan = self.selected_http_plan()
        evidence = self.evidence / 'http'
        evidence.mkdir(parents=True, exist_ok=True)
        cached = any(os.environ.get(name) == 'true' for name in
                     ('HTTP_DEPS_CACHE_HIT', 'HTTP_FULL_DEPS_CACHE_HIT'))
        if not cached:
            require(not self.http.exists(), 'HTTP prefix exists on a cache miss')
            self.run('http-bootstrap', [sys.executable, 'tools/setup_http_deps.py',
                     '--profile', 'Release', '--jobs', '3', '--macos-install-only'], timeout=1800)
        manifest = http.verify_prefix(plan, evidence, cached=cached)
        if not cached:
            http.save(self.http, 'VOID-HTTP-CI-IDENTITY.json',
                      {'identity': plan['identity'], 'manifest_sha256': digest(manifest)})
        shutil.copy2(manifest, self.evidence / 'http-dependencies.json')
        require(not self.host_prefix.exists(), 'Host prefix must start fresh')
        self.run('host-bootstrap', [sys.executable, 'tools/setup_host_deps.py',
                 '--profile', 'Release', '--jobs', '3'], timeout=1200)
        self.verify_dependencies()

    def verify_dependencies(self):
        http.verify_prefix(self.selected_http_plan(), self.evidence / 'http', cached=True)
        self.run('host-verify', [sys.executable, 'tools/verify_host_deps.py', '--prefix',
                 self.host_prefix, '--profile', 'Release', '--platform', 'Darwin',
                 '--architecture', 'arm64'], timeout=120)
        shutil.copy2(self.host_prefix / 'VOID-HOST-DEPENDENCIES.json', self.evidence / 'host-dependencies.json')

    def configure(self):
        # Missing HTTP manifests caused the foundation configure failure. Require
        # real bootstrap/verification before CMake; never synthesize a receipt.
        self.verify_dependencies()
        super().configure()

    def run(self, label, argv, **kwargs):
        if label == 'configure':
            argv = [*argv, '-DVOIDUP_DIRECTORY_DIAGNOSTIC=ON']
            cache = self.work / 'thinlto-cache'
            cache.mkdir(exist_ok=True)
            # Reuse ci-link-experiment's Apple ld cache for expensive SDK bitcode
            # links, including the ORC configure probe. No forced -flto rebuild.
            argv = [*argv, '-DCMAKE_EXE_LINKER_FLAGS:STRING=-Wl,-cache_path_lto,"' + str(cache) + '"']
            kwargs.setdefault('timeout', 900)
        return super().run(label, argv, **kwargs)

    def qualify(self):
        self.run('voidup-first-run', [sys.executable, '-B', ROOT / 'tests/voidup/first_run.py',
                 '--build', self.build, '--sdk', self.sdk, '--licenses', self.prefix / 'share/void/licenses',
                 '--evidence', self.evidence / 'voidup-first-run', '--isolate-producer'], timeout=1800)
        for name, args in [('policy', [self.voidc]), ('release', []), ('production', []),
                           ('developer_environment', [self.voidc])]:
            self.run('voidup-' + name, [sys.executable, '-B', ROOT / 'tests/voidup' / (name + '.py'), *args], timeout=180)
        self.save('voidup-qualification.json', {'status': 'PASS', 'system': SYSTEM,
                  'scope': 'Native ARM64 engineering bootstrap, relocated payload, install/compiler/LSP, update/rollback/recovery; producer paths hidden during consumer checks',
                  'production_distribution_qualified': False})

    def report(self):
        # Reuse the installation report and its failure evidence collection. Add
        # the VOIDUP result before it seals all receipts; missing is never PASS.
        result = self.evidence / 'voidup-qualification.json'
        qualified = result.is_file() and json.loads(result.read_text())['status'] == 'PASS'
        if not qualified:
            os.environ['INSTALL_PRIOR_JOB_STATUS'] = 'failure'
        cache = self.work / 'thinlto-cache'
        entries = [p for p in cache.rglob('*') if p.is_file()]
        self.save('thinlto-cache.json', {'directory': str(cache), 'files': len(entries),
                  'bytes': sum(p.stat().st_size for p in entries),
                  'scope': 'Job-local Apple linker SDK bitcode cache; no forced -flto or warm-relink campaign'})
        for name in ('.ninja_log', 'build.ninja'):
            if (self.build / name).is_file():
                shutil.copy2(self.build / name, self.evidence / name.lstrip('.'))
        for path in (ROOT / 'docs/voidup/evidence').glob('dependencies-macos-*'):
            if path.is_dir() and not subprocess.check_output(['git', 'ls-files', '--', str(path)], cwd=ROOT):
                shutil.copytree(path, self.evidence / path.name, dirs_exist_ok=True)
        super().report()


def main():
    require(SYSTEM == 'Darwin', 'Focused driver requires native macOS')
    phase = sys.argv[1]
    q = MacQualification()
    if phase == 'identity':
        q.work.mkdir(parents=True, exist_ok=False)
    q.evidence.mkdir(parents=True, exist_ok=True)
    if phase not in ('identity', 'report'):
        gate = q.evidence / 'check-identity.json'
        require(gate.is_file() and json.loads(gate.read_text())['status'] == 'PASS', 'Passing hosted identity required')
    actions = {'identity': q.identity, 'host': q.host, 'sdk': q.sdk_install,
               'dependency-identity': q.dependency_identity, 'dependencies': q.dependencies,
               'configure': q.configure, 'build': lambda: q.run('build', ['cmake', '--build', q.build,
                   '--target', 'voidc', 'voidls', '--parallel', '3'], timeout=2700),
               'install': q.install, 'smoke': q.smoke, 'qualify': q.qualify, 'report': q.report}
    if phase == 'report':
        actions[phase]()
    else:
        q.check(phase, actions[phase])


if __name__ == '__main__':
    main()
