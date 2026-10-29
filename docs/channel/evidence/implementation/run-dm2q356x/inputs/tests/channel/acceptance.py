"""Run the focused final engineering selection with immutable first-failure logs."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import sys
import tempfile
import time
import xml.etree.ElementTree as ET
import support as h

profile = sys.argv[1]
assert profile in ('debug', 'release', 'sanitizers')
build = h.ROOT / 'build' / ('ci-official-san' if profile == 'sanitizers' else 'ci-official-' + profile)
receipt = Path(tempfile.mkdtemp(prefix='acceptance-' + profile + '-', dir=h.EVIDENCE))
selection = '^(channel_.*|lsp_channel|advanced_source|generics_source|dictionaries_source|collections_source|properties_source|managed_source|stage11_globals|lsp_stage11_globals|debug_cache|automatic_cache_processes|gc_schemas|gc_buffers|documentation_execution|lsp_documentation)$'
env = dict(os.environ)
env['VOID_CRASH_EVIDENCE'] = str(h.EVIDENCE / 'crashes')
env['PYTHONPATH'] = str(h.ROOT / 'tests/text_encoding') + os.pathsep + env.get('PYTHONPATH', '')
if profile == 'sanitizers':
    env['ASAN_OPTIONS'] = 'detect_leaks=1:halt_on_error=1:abort_on_error=0:exitcode=87'
    env['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1:exitcode=86'
command = ['ctest', '--test-dir', str(build), '-R', selection, '-j', '2', '--output-on-failure', '--output-junit', str(receipt / 'tests.xml')]
start = time.perf_counter()
result = subprocess.run(command, cwd=h.ROOT, env=env, capture_output=True, timeout=7200)
(receipt / 'stdout').write_bytes(result.stdout); (receipt / 'stderr').write_bytes(result.stderr)
tests = []
if (receipt / 'tests.xml').exists():
    for case in ET.parse(receipt / 'tests.xml').getroot().iter('testcase'):
        tests.append({'name': case.attrib['name'], 'seconds': float(case.attrib.get('time', 0)),
                      'failed': case.find('failure') is not None, 'skipped': case.find('skipped') is not None})
files = [h.ROOT / 'CMakeLists.txt', *sorted((h.ROOT / 'stdlib').rglob('*.void')),
         *sorted((h.ROOT / 'src').rglob('*')), *sorted((h.ROOT / 'include').rglob('*')),
         *sorted((h.ROOT / 'tests/channel').glob('*.*')), *sorted((h.ROOT / 'examples/channel').glob('*.void'))]
record = {'profile': profile, 'command': command, 'status': result.returncode,
          'seconds': time.perf_counter() - start, 'tests': tests,
          'passed': sum(not x['failed'] and not x['skipped'] for x in tests),
          'failed': sum(x['failed'] for x in tests),
          'environment': {name: env[name] for name in ('ASAN_OPTIONS', 'UBSAN_OPTIONS', 'PYTHONPATH') if name in env},
          'build_config': (build / 'CMakeCache.txt').read_text(),
          'source_hashes': {str(p.relative_to(h.ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in files if p.is_file()},
          'build_hashes': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in
                           (build / name for name in ('voidc', 'voidls', 'void_channel_host', 'libvoid_runtime.a', 'void-native.txt')) if p.is_file()}}
(receipt / 'receipt.json').write_text(json.dumps(record, indent=2) + '\n')
print(result.stdout.decode(errors='replace'), end='')
print(result.stderr.decode(errors='replace'), end='')
assert result.returncode == 0, receipt
print(f'{profile}: {record["passed"]}/{len(tests)} focused CTests passed; {receipt}')
