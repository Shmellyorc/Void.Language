"""Reconcile full consolidated CTests with explicitly ordered focused reruns.

Run from the repository root after the named runs finish. This does not assert
one fresh all-tests run or native-platform/provider-memory acceptance.
"""
import hashlib
import json
from pathlib import Path
import re

ROOT = Path.cwd()
BASE = ROOT / 'docs/net/http/evidence'
RUNS = {
    'release': ['http-consolidated-tests-release.log',
                'http-final-tests-release.log',
                'http-reaping-final-tests-release.log',
                'http-reload-owner-tests-release.log',
                'http-frontend-final-tests-release.log',
                'http-dns-port-final-release.log',
                'http-frontend-channel-final-release.log'],
    'debug': ['http-consolidated-tests-debug.log',
              'http-frontend-stack-tests-debug.log',
              'http-final-tests-debug-repaired.log',
              'http-frontend-channel-final-debug.log'],
    'strict': ['http-consolidated-tests-strict.log',
               'http-final-tests-strict.log',
               'http-frontend-final-tests-strict.log',
               'http-frontend-channel-final-strict.log'],
}
LINE = re.compile(r'^\s*(\d+)/(\d+) Test\s*#\d+: (\S+)\s+\.+\s*(.*?)\s+([\d.]+) sec\s*$', re.M)

def sha(path):
    return hashlib.sha256((ROOT / path).read_bytes()).hexdigest()

report = {'scope': 'Registered test coverage from the original consolidated runs plus the named focused reruns; not a single fresh all-tests run or full HTTP acceptance',
          'profiles': {}}
for profile, names in RUNS.items():
    registered = json.loads((BASE / f'http-registered-tests-{profile}.json').read_text())
    latest, runs = {}, []
    for name in names:
        content = (BASE / name).read_text()
        footer = re.search(r'(\d+)% tests passed, (\d+) tests failed out of (\d+)', content)
        assert footer, f'incomplete run: {name}'
        rows = LINE.findall(content)
        total, failures = int(footer[3]), int(footer[2])
        assert len(rows) == total, (name, len(rows), total)
        assert sum(result != 'Passed' for _, _, _, result, _ in rows) == failures
        runs.append({'log': name, 'sha256': sha(BASE.relative_to(ROOT) / name),
                     'total': total, 'passed': total - failures, 'failed': failures})
        for _, _, test, result, seconds in rows:
            latest[test] = {'result': result, 'seconds': float(seconds), 'log': name}
    missing = sorted(set(registered) - latest.keys())
    failed = sorted(test for test in registered if test in latest and latest[test]['result'] != 'Passed')
    assert not missing and not failed, (profile, missing, failed)
    report['profiles'][profile] = {
        'registered_tests': len(registered), 'latest_available_passes': len(registered),
        'missing': missing, 'failed': failed, 'ordered_runs': runs,
        'latest_result_per_test': {test: latest[test] for test in registered},
        'artifacts_sha256': {name: sha(f'build/llvm23-{profile}/{name}') for name in
                             ['voidc', 'voidls', 'libvoid_runtime.a', 'curl/lib/libcurl.a']},
    }
for profile in ['debug', 'strict']:
    identity = json.loads((BASE / f'http-tls-corpus-final-{profile}/identity.json').read_text())
    assert all(sha(name) == digest for name, digest in identity['sha256'].items()), profile
report['tls_corpus_runtime_identities_unchanged'] = True
report['protected_root_readme_sha256'] = sha('README.md')
assert report['protected_root_readme_sha256'] == 'c287f35842b03a1c5ac170718e674cc5498f877ade76eede5273859921d62add'
report['source_sha256'] = {name: sha(name) for name in
    ['CMakeLists.txt', 'src/frontend/compiler.cpp', 'src/runtime/http.cpp',
     'src/runtime/http_file.cpp', 'src/runtime/async.cpp',
     'tests/net/http/dns_fixture.py', 'third_party/http-versions.json']}
fresh = json.loads((BASE / 'http-final-clean-build-identity.json').read_text())
assert fresh['source_sha256'] == report['source_sha256']
assert all(row['identical'] for row in fresh['archive_and_compiler_object_comparisons'].values())
assert fresh['voidls_identical'] and fresh['example_process_checks'] == 18
for name, digest in fresh['logs_sha256'].items():
    assert sha(BASE.relative_to(ROOT) / name) == digest
report['final_source_clean_build'] = {
    'identity_report': 'http-final-clean-build-identity.json',
    'identity_report_sha256': sha(BASE.relative_to(ROOT) / 'http-final-clean-build-identity.json'),
    'identical_archives_and_compiler_objects': 8,
    'example_process_checks': 18, 'example_ctests': 1, 'lsp_ctests': 2,
    'explicit_linker': fresh['linker_version'],
}
report['generator_sha256'] = sha('docs/net/http/evidence/final-verification.py')
(BASE / 'http-final-verification.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps({p: {'registered': r['registered_tests'], 'latest_available_passes': r['latest_available_passes']}
                  for p, r in report['profiles'].items()}, indent=2))
