"""Focused, separately retained receipts for Golden Wrench's two review repairs."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
OUT = HERE / 'evidence'
BASELINE = 'a4fd7fb0b79c1fb2ac17073bde492118a50516b3'
SELECTION = '^(random_host|lsp_random|random_frontend|random_frontend_stack|random_worker|random_reference|core|frontend|numeric|modules|lsp_documentation|lsp_binary)$'
SOURCES = ['src/ir/module.cpp', 'src/frontend/compiler.cpp', 'tests/random/host.cpp',
           'tests/random/lsp.py', 'stdlib/random.void', 'src/frontend/random.inc',
           'src/frontend/capabilities.inc', 'src/runtime/random.cpp',
           'src/backend/llvm/backend.cpp', 'tests/random/reference.py',
           'tests/random/execution.py', 'tests/text_encoding/crash.py']

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def save(name, value):
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / name).write_text(json.dumps(value, indent=2) + '\n')

def historical():
    return {str(p.relative_to(ROOT)): digest(p)
            for p in sorted((ROOT / 'docs/random').rglob('*'))
            if p.is_file() and HERE not in p.parents}

def source_hashes():
    return {p: digest(ROOT / p) for p in SOURCES}

def execute(label, command, env):
    OUT.mkdir(parents=True, exist_ok=True)
    started = time.time()
    with (OUT / (label + '.log')).open('wb') as log:
        result = subprocess.run(command, cwd=ROOT, env=env, stdout=log,
                                stderr=subprocess.STDOUT, timeout=1800)
    record = {'command': command, 'cwd': str(ROOT), 'status': result.returncode,
              'seconds': time.time() - started,
              'environment': {k: env[k] for k in ['TMPDIR', 'ASAN_OPTIONS',
                              'UBSAN_OPTIONS', 'PYTHONPATH', 'VOID_CRASH_EVIDENCE']
                              if k in env}, 'sources': source_hashes()}
    return record

mode = sys.argv[1]
if mode == 'prepare':
    head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT).decode().strip()
    assert head == BASELINE, head
    save('preservation-before.json', {'baseline': head, 'root_readme': digest(ROOT / 'README.md'),
         'historical_files': historical(),
         'baseline_sources': {p: hashlib.sha256(subprocess.check_output(
             ['git', 'show', BASELINE + ':' + p], cwd=ROOT)).hexdigest() for p in SOURCES}})
    print('Historical random records and baseline source hashes retained.')
elif mode in ('build', 'test'):
    profile = sys.argv[2]
    build = ROOT / 'build' / ('ci-official-' + profile)
    env = dict(os.environ)
    if mode == 'build':
        temporary = build / 'review-repair-tmp'
        temporary.mkdir(exist_ok=True)
        env['TMPDIR'] = str(temporary)
        command = ['cmake', '--build', str(build), '--target', 'voidc', 'voidls',
                   'void_random_host', 'void_tests', 'void_io_runner', '-j', '3']
    else:
        env['PYTHONPATH'] = str(ROOT / 'tests/text_encoding') + os.pathsep + env.get('PYTHONPATH', '')
        env['VOID_CRASH_EVIDENCE'] = str(OUT / 'crashes')
        if profile == 'sanitizers':
            env['ASAN_OPTIONS'] = 'detect_leaks=1:halt_on_error=1:abort_on_error=0:exitcode=87'
            env['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1:exitcode=86'
        command = ['ctest', '--test-dir', str(build), '-R', SELECTION, '-j', '2', '-V',
                   '--output-junit', str(OUT / ('test-' + profile + '.xml'))]
    label = mode + '-' + profile
    record = execute(label, command, env)
    if mode == 'test':
        cases = ET.parse(OUT / (label + '.xml')).getroot().iter('testcase')
        record['tests'] = [{'name': c.attrib['name'], 'seconds': float(c.attrib.get('time', 0)),
                           'failed': c.find('failure') is not None,
                           'skipped': c.find('skipped') is not None} for c in cases]
        record['binaries'] = {p: digest(build / p) for p in
                              ['voidc', 'voidls', 'void_random_host', 'void_tests', 'void_io_runner']}
    save(label + '.json', record)
    print(label, 'status', record['status'], 'seconds', round(record['seconds'], 2), flush=True)
    if mode == 'test':
        print([(c['name'], 'FAIL' if c['failed'] else 'PASS') for c in record['tests']], flush=True)
    assert record['status'] == 0, label
elif mode == 'finalize':
    before = json.loads((OUT / 'preservation-before.json').read_text())
    now = historical()
    assert now == before['historical_files'], 'Historical records changed'
    assert digest(ROOT / 'README.md') == before['root_readme'], 'Root README changed'
    changed = [p for p, h in source_hashes().items() if h != before['baseline_sources'][p]]
    assert set(changed) == {'src/ir/module.cpp', 'src/frontend/compiler.cpp',
                            'tests/random/host.cpp', 'tests/random/lsp.py'}, changed
    tests = {p: json.loads((OUT / ('test-' + p + '.json')).read_text())
             for p in ['debug', 'release', 'sanitizers']}
    assert all(t['status'] == 0 and len(t['tests']) == 12 and
               all(not c['failed'] and not c['skipped'] for c in t['tests']) for t in tests.values())
    save('FINAL.json', {'baseline': BASELINE, 'sources': source_hashes(),
         'changed_source_files': changed, 'full_matrix_run': False,
         'historical_files_unchanged': len(now), 'root_readme_unchanged': before['root_readme'],
         'tests': {p: {'passed': len(t['tests']), 'failed': 0} for p, t in tests.items()},
         'new_incident_receipts': [str(p.relative_to(HERE)) for p in sorted((OUT / 'crashes').rglob('receipt.json'))]})
    print('36/36 focused CTests passed; historical records unchanged:', len(now))
else:
    raise ValueError(mode)
