"""HTTP preparation for the existing seven-job CI; no compiler/build/test replacement.

Exact cache keys have no fallback. A restored native prefix requires both its
setup manifest and a seal binding it to the current toolchain/recipe identity.
The read-only verify phase also accepts existing local setup manifests.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import ssl
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
RECIPES = ('tools/ci/http_bootstrap.py', 'tools/ci/llvm_sdk.py',
           'tools/setup_http_deps.py', 'tools/setup_http_deps_native.py', 'tools/http_archive.py',
           'tools/verify_http_dependencies.py', 'cmake/HttpDependencies.cmake',
           'cmake/HttpNativeDependencies.cmake', 'cmake/CurlProvider.cmake', 'cmake/CurlProviderFeatures.cmake', 'cmake/CurlProviderObjects.cmake',
           'cmake/http-psl/CMakeLists.txt', 'cmake/NativeBuild.cmake', 'third_party/http-versions.json',
           'third_party/publicsuffix/UPSTREAM.json',
           'third_party/publicsuffix/public_suffix_list.dat',
           'third_party/curl/VOID-PROVENANCE.json', 'tools/verify_curl_source.py')
FIXTURES = ('tools/setup_http_tests.py', 'tests/net/http/python-versions.json')
REQUIRED_TESTS = {'http_native', 'http_reload', 'http_timeouts', 'http_wire', 'http_dns',
                  'url_native', 'http_examples', 'http_install', 'http_ci_policy', 'http_bulk', 'http_ir',
                  'http_source', 'http_source_negative', 'http_source_faults', 'url_ir',
                  'url_source', 'url_lsp', 'http_lsp'}


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def key(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()


def host_identity():
    system = platform.system()
    arch = {'amd64': 'x86_64', 'aarch64': 'arm64'}.get(platform.machine().lower(), platform.machine().lower())
    if (system, arch) not in {('Linux', 'x86_64'), ('Windows', 'x86_64'), ('Darwin', 'arm64')}:
        raise RuntimeError(f'HTTP seven-job CI native host mismatch: {system}/{arch}')
    host = {'system': system, 'architecture': arch, 'image_os': os.environ.get('ImageOS', ''),
            'cmake': subprocess.check_output(['cmake', '--version'], text=True).splitlines()[0],
            'sdk_sha256': digest(ROOT / '.toolchains/llvm-23.1.4/void-sdk-identity.json')}
    if system == 'Windows':
        for name in ('VCToolsVersion', 'WindowsSDKVersion', 'VSCMD_ARG_TGT_ARCH'):
            host[name] = os.environ[name]
        if host['VSCMD_ARG_TGT_ARCH'].lower() not in ('x64', 'amd64'):
            raise RuntimeError('HTTP CI requires the native x64 MSVC developer environment')
    elif system == 'Darwin':
        sdk = os.environ.get('SDKROOT') or subprocess.check_output(['xcrun', '--show-sdk-path'], text=True).strip()
        if not Path(sdk).is_absolute() or not Path(sdk).is_dir():
            raise RuntimeError('HTTP CI requires the physical selected macOS SDK')
        host.update(sdk=str(Path(sdk).resolve()),
                    sdk_version=subprocess.check_output(['xcrun', '--show-sdk-version'], text=True).strip(),
                    xcode=subprocess.check_output(['xcodebuild', '-version'], text=True).strip(),
                    deployment_target=os.environ.get('MACOSX_DEPLOYMENT_TARGET', '14.0'))
    else:
        host['libc'] = list(platform.libc_ver())
    return host


def make_plan(configuration):
    profile = {'Debug': 'Debug', 'Release': 'Release', 'Sanitizers': 'strict'}[configuration]
    host = host_identity()
    if profile == 'strict' and host['system'] != 'Linux':
        raise RuntimeError('The seven-job matrix has one strict Linux job')
    policy = json.loads((ROOT / 'third_party/http-versions.json').read_text())
    bundle = {'Linux': policy['bundle'], 'Windows': 'windows-schannel-x86_64',
              'Darwin': 'macos-aws-lc-5.12.0-arm64'}[host['system']]
    identity = {'schema': 1, 'llvm': '23.1.4', 'host': host, 'profile': profile,
                'files': {name: digest(ROOT / name) for name in RECIPES}}
    fixture_identity = {name: digest(ROOT / name) for name in FIXTURES}
    fixture_identity['python'] = list(sys.version_info[:2])
    if host['system'] == 'Windows':
        fixture_identity['certificate_host'] = host  # Release-built tool shared by both configurations.
        fixture_identity['pins'] = digest(ROOT / 'third_party/http-versions.json')
    tag = host['system'] + '-' + host['architecture']
    return {'configuration': configuration, 'profile': profile, 'identity': identity,
            'prefix': f'.toolchains/http/prefix/{bundle}/{profile}',
            'deps-key': f'http-deps-v1-llvm23.1.4-{tag}-{profile}-{key(identity)}',
            'sources-key': f'http-sources-v1-{host["system"]}-{digest(ROOT / "third_party/http-versions.json")}',
            'fixtures-key': f'http-fixtures-v1-{tag}-{key(fixture_identity)}'}


def save(evidence, name, value):
    evidence.mkdir(parents=True, exist_ok=True)
    (evidence / name).write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


def run(evidence, label, argv):
    start = time.perf_counter()
    record = {'argv': list(map(str, argv))}
    try:
        with (evidence / f'{label}.log').open('w', encoding='utf-8') as log:
            process = subprocess.Popen(record['argv'], cwd=ROOT, stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT, text=True, errors='replace')
            for line in process.stdout:
                log.write(line); log.flush(); print(line, end='', flush=True)
            record['exit_code'] = process.wait()
    finally:
        record['wall_seconds'] = time.perf_counter() - start
        save(evidence, label + '.json', record)
    if record['exit_code']:
        raise RuntimeError(f'HTTP {label} failed ({record["exit_code"]}); see {evidence}')


def verify_prefix(plan, evidence, cached=False):
    prefix = ROOT / plan['prefix']
    manifest = prefix / 'VOID-HTTP-DEPENDENCIES.json'
    if manifest.exists():
        shutil.copy2(manifest, evidence / 'dependency-manifest.json')
    if cached:
        seal_path = prefix / 'VOID-HTTP-CI-IDENTITY.json'
        if seal_path.exists():
            shutil.copy2(seal_path, evidence / 'dependency-cache-seal.json')
        seal = json.loads(seal_path.read_text())
        if seal != {'identity': plan['identity'], 'manifest_sha256': digest(manifest)}:
            raise RuntimeError('Restored HTTP cache identity/manifest mismatch; no fallback')
    report = json.loads(manifest.read_text())
    host = plan['identity']['host']
    if report.get('platform', 'Linux') != host['system']:
        raise RuntimeError('HTTP manifest platform mismatch')
    # Linux's original accepted manifest predates explicit architecture metadata.
    if report.get('architecture', host['architecture']) != host['architecture']:
        raise RuntimeError('HTTP manifest architecture mismatch')
    if host['system'] == 'Darwin' and (
        str(Path(report['sdk']).resolve()) != host['sdk'] or
        report['deployment_target'] != host['deployment_target']
    ):
        raise RuntimeError('HTTP manifest macOS SDK/deployment mismatch')
    if not report.get('files'):
        raise RuntimeError('Empty HTTP dependency inventory')
    args = [sys.executable, ROOT / 'tools/verify_http_dependencies.py', prefix, plan['profile']]
    if host['system'] != 'Linux':
        args += [host['system'], host['architecture']]
    run(evidence, 'verify-dependencies', args)
    return manifest


def fixture_preflight(plan, evidence):
    if not ssl.HAS_ALPN:
        raise RuntimeError('Python fixture TLS lacks ALPN; genuine HTTP/2 cannot be tested')
    site = ROOT / '.toolchains/http/test-python/site-packages'
    sys.path.insert(0, str(site))
    import importlib.metadata
    versions = {}
    for pin in json.loads((ROOT / 'tests/net/http/python-versions.json').read_text()):
        package = __import__(pin['name'])
        if not Path(package.__file__).resolve().is_relative_to(site.resolve()):
            raise RuntimeError(f'Unpinned fixture module: {pin["name"]}')
        actual = importlib.metadata.version(pin['name'])
        if actual != pin['version']:
            raise RuntimeError(f'Fixture version mismatch: {pin["name"]}')
        versions[pin['name']] = actual
    sys.path.insert(0, str(ROOT / 'tests/net/http'))
    import fixture
    tool = fixture.certificate_tool(ROOT / 'build' / plan['configuration'] / ('voidc.exe' if os.name == 'nt' else 'voidc'),
                                    explicit=None) if plan['identity']['host']['system'] == 'Windows' else ROOT / plan['prefix'] / 'bin/openssl'
    if not tool.is_file():
        raise RuntimeError(f'Pinned certificate fixture executable missing: {tool}')
    run(evidence, 'certificate-version', [tool, 'version'])
    save(evidence, 'fixtures.json', {'versions': versions, 'certificate_tool': str(tool),
                                    'python_ssl': ssl.OPENSSL_VERSION, 'alpn': ssl.HAS_ALPN})


def test_plan(plan, evidence, build):
    tests = json.loads(subprocess.check_output(['ctest', '--test-dir', str(build), '--show-only=json-v1'], text=True))['tests']
    http = {t['name']: t for t in tests if t['name'].startswith(('http_', 'url_'))}
    required = REQUIRED_TESTS | ({'http_tls_memory'} if plan['identity']['host']['system'] != 'Windows' else set())
    missing = sorted(required - http.keys())
    if missing:
        raise RuntimeError(f'HTTP/URL acceptance tests missing: {missing}')
    save(evidence, 'test-plan.json', {'total_registered': len(tests), 'http_url_tests': http,
         'execution': 'Existing unfiltered CTest step; no HTTP exclusions or skips added',
         'long_tests': [n for n in ('http_tls_memory', 'http_source_faults') if n in http],
         'schannel_memory_note': 'AWS-LC corpus is not registered on Windows; this is not a Schannel allocator audit'})
    print(f'HTTP/URL test registration verified: {len(http)} of {len(tests)} total tests; full CTest remains unfiltered')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('phase', choices=['identity', 'bootstrap', 'verify', 'test-plan'])
    parser.add_argument('--configuration', choices=['Debug', 'Release', 'Sanitizers'], default=os.environ.get('EXPERIMENT_CONFIGURATION', 'Release'))
    parser.add_argument('--evidence', type=Path, default=Path(os.environ.get('EXPERIMENT_DIR', 'build/link-experiment')) / 'http')
    parser.add_argument('--build', type=Path)
    args = parser.parse_args()
    evidence = args.evidence.resolve(); evidence.mkdir(parents=True, exist_ok=True)
    plan = make_plan(args.configuration)
    if args.phase == 'identity':
        run(evidence, 'verify-curl-source', [sys.executable, ROOT / 'tools/verify_curl_source.py'])
        save(evidence, 'identity.json', plan)
        for variable, values in [('GITHUB_OUTPUT', {k: plan[k] for k in ('profile', 'prefix', 'deps-key', 'sources-key', 'fixtures-key')}),
                                 ('GITHUB_ENV', {'MACOSX_DEPLOYMENT_TARGET': plan['identity']['host'].get('deployment_target', '')} if platform.system() == 'Darwin' else {})]:
            if os.environ.get(variable):
                with open(os.environ[variable], 'a', encoding='utf-8') as out:
                    for name, value in values.items(): out.write(f'{name}={value}\n')
    elif args.phase == 'bootstrap':
        cached = os.environ.get('HTTP_DEPS_CACHE_HIT') == 'true'
        prefix = ROOT / plan['prefix']
        built = False
        before = set((ROOT / 'docs/net/http/evidence').glob('dependencies-*'))
        try:
            if not cached:
                if prefix.exists():
                    raise RuntimeError('HTTP prefix exists on a cache miss; refusing unverified reuse')
                built = True
                run(evidence, 'build-dependencies', [sys.executable, ROOT / 'tools/setup_http_deps.py', '--profile', plan['profile'], '--jobs', '3'])
            manifest = verify_prefix(plan, evidence, cached=cached)
            if not cached:
                save(prefix, 'VOID-HTTP-CI-IDENTITY.json', {'identity': plan['identity'], 'manifest_sha256': digest(manifest)})
            command = [sys.executable, ROOT / 'tools/setup_http_tests.py', '--jobs', '3']
            if platform.system() == 'Windows': command += ['--certificates']
            run(evidence, 'setup-fixtures', command)
            fixture_preflight(plan, evidence)
            save(evidence, 'bootstrap.json', {'dependency_cache_hit': cached, 'prefix': plan['prefix'], 'result': 'PASS'})
        finally:
            for path in sorted(set((ROOT / 'docs/net/http/evidence').glob('dependencies-*')) - before):
                shutil.copytree(path, evidence / 'dependency-builds' / path.name, dirs_exist_ok=True)
            # Retain configure diagnostics even when setup cannot produce a prefix.
            roots = [ROOT / plan['prefix'].replace('/prefix/', '/build/')] if built else []
            if platform.system() == 'Windows':
                roots += [ROOT / '.toolchains/http/test-certificates-build']
            for index, build_root in enumerate(roots):
                for name in ('CMakeCache.txt', 'CMakeConfigureLog.yaml', 'CMakeError.log', 'config.log', 'commands.json'):
                    for source in build_root.rglob(name):
                        target = evidence / 'bootstrap-configure' / str(index) / source.relative_to(build_root)
                        target.parent.mkdir(parents=True, exist_ok=True)
                        shutil.copy2(source, target)
    elif args.phase == 'verify':
        verify_prefix(plan, evidence)
        fixture_preflight(plan, evidence)
    else:
        test_plan(plan, evidence, args.build or ROOT / 'build' / args.configuration)


if __name__ == '__main__':
    main()
