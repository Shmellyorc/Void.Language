"""Index existing shared-recorder receipts and the final local acceptance selection."""
from pathlib import Path
import hashlib, json, platform, re, subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'docs/log/evidence'
def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
def save(name, value):
    (OUT / name).write_text(json.dumps(value, indent=2) + '\n')

freeze = json.loads((OUT / 'validation-source-freeze.json').read_text())
assert all(digest(ROOT / name) == value for name, value in freeze.items()), 'Validation source changed'
profiles = []
for label, directory in [('debug', 'debug'), ('release', 'release'), ('strict', 'sanitizers')]:
    build = ROOT / ('build/ci-official-' + directory)
    summary = (OUT / f'acceptance-{label}-verified.log').read_text()
    assert '100% tests passed, 0 tests failed out of 31' in summary, label
    detail = (build / 'Testing/Temporary/LastTest.log').read_text()
    results = []
    for match in re.finditer(r'\d+/\d+ Testing: ([^\n]+)\n(.*?)(?=\n\d+/\d+ Testing:|\Z)', detail, re.S):
        name, body = match.groups()
        assert 'Test Passed.' in body, (label, name)
        seconds = re.search(r'Test time =\s*([\d.]+) sec', body)
        results.append({'name': name, 'seconds': float(seconds[1]),
                        'logging': name.startswith('log_') or name == 'lsp_log'})
    assert len(results) == 31, (label, len(results))
    (OUT / f'ctest-{label}-verified.log').write_text(detail)
    tools = {}
    for name in ['voidc', 'voidls', 'void_io_runner', 'void_log_host',
                 'void_log_probe_normal', 'void_log_probe_optimized',
                 'void_io_native_jobs', 'void_async_tests', 'libvoid_runtime.a',
                 'void-native.txt', 'CMakeCache.txt']:
        path = build / name
        if path.exists(): tools[str(path.relative_to(ROOT))] = digest(path)
    profiles.append({'profile': label, 'tests': results, 'tool_sha256': tools})
save('acceptance-index.json', profiles)

incidents = []
for directory in sorted((OUT / 'crashes').glob('incident-*')):
    record = json.loads((directory / 'receipt.json').read_text())
    for source in record.get('sources', []):
        copy = Path(source['copy'])
        assert digest(copy) == source['preserved_sha256'], (directory, copy)
    incidents.append({'directory': str(directory.relative_to(ROOT)),
                      **{key: record.get(key) for key in ['failure_kind', 'command', 'returncode', 'signal', 'timed_out']},
                      'source_snapshots': len(record.get('sources', [])),
                      'stderr_prefix': (directory / 'stderr.bin').read_text(errors='replace')[:700]})
save('incident-index.json', incidents)

receipts = []
for path in sorted(OUT.glob('run-*/receipt.json')):
    record = json.loads(path.read_text())
    receipts.append({'receipt': str(path.relative_to(ROOT)),
                     **{key: record.get(key) for key in ['command', 'returncode', 'seconds', 'hard_stack_bytes', 'executable_sha256']}})
save('receipt-index.json', receipts)

changed = subprocess.check_output(['git', 'diff', '--name-only'], cwd=ROOT, text=True).splitlines()
untracked = subprocess.check_output(['git', 'ls-files', '--others', '--exclude-standard'], cwd=ROOT, text=True).splitlines()
files = sorted(set(changed + [p for p in untracked if not p.startswith('docs/log/evidence/') and '__pycache__' not in p]))
save('source-inventory.json', {name: {'bytes': (ROOT / name).stat().st_size, 'sha256': digest(ROOT / name)} for name in files})
root_readme = subprocess.check_output(['git', 'show', 'HEAD:README.md'], cwd=ROOT)
assert (ROOT / 'README.md').read_bytes() == root_readme, 'Protected README changed'
assert not subprocess.check_output(['git', 'diff', '--cached', '--name-only'], cwd=ROOT), 'Index unexpectedly changed'
save('environment.json', {'system': platform.platform(), 'machine': platform.machine(),
                         'python': platform.python_version(),
                         'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                         'protected_readme_sha256': digest(ROOT / 'README.md'),
                         'logging_stack_soft_bytes': 1048576, 'logging_stack_hard_bytes': 1048576,
                         'strict_environment': {'ASAN_OPTIONS': 'detect_leaks=1:halt_on_error=1:abort_on_error=0:exitcode=87',
                                                'UBSAN_OPTIONS': 'halt_on_error=1:print_stacktrace=1:exitcode=86',
                                                'LSAN_OPTIONS': 'exitcode=87'}})
print(f'Indexed {len(profiles)*31} final CTest outcomes, {len(receipts)} process receipts, '
      f'{len(incidents)} preserved incidents and {len(files)} source/document files')
