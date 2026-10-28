"""Focused profile selection with immutable results and the shared failure recorder."""
from pathlib import Path
import json
import os
import sys
import tempfile
import time
import xml.etree.ElementTree as ET
import support as h
profile = sys.argv[1]
assert profile in ('debug', 'release', 'san')
build = h.ROOT / 'build' / ('ci-official-' + profile)
receipt = Path(tempfile.mkdtemp(prefix='acceptance-' + profile + '-', dir=h.EVIDENCE))
env = dict(os.environ)
env['VOID_CRASH_EVIDENCE'] = str(h.EVIDENCE / 'crashes')
env['PYTHONPATH'] = str(h.ROOT / 'tests/text_encoding') + os.pathsep + env.get('PYTHONPATH', '')
if profile == 'san':
    env['ASAN_OPTIONS'] = 'detect_leaks=1:halt_on_error=1:abort_on_error=0:exitcode=87'
    env['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1:exitcode=86'
selection = '^(collections_stdlib_(stack|host|failures|frontend|oracles|install|cache|operators)|lsp_collections_stdlib|text_encoding_crash_preservation)$'
command = ['ctest', '--test-dir', str(build), '-R', selection, '-j', '2', '--output-on-failure', '--output-junit', str(receipt / 'tests.xml')]
started = time.perf_counter()
result = h.crash.run(command, cwd=h.ROOT, env=env, capture_output=True, timeout=3600,
                     expected_returncode=0, source_inputs=[h.ROOT / 'tests/collections_stdlib', h.ROOT / 'examples/collections_stdlib'],
                     provenance_inputs=[Path(__file__), Path(__file__).parent], evidence_context={'profile':profile,'mode':'focused-ctest'})
(receipt / 'stdout.bin').write_bytes(result.stdout); (receipt / 'stderr.bin').write_bytes(result.stderr)
tests = []
if (receipt / 'tests.xml').exists():
    for case in ET.parse(receipt / 'tests.xml').getroot().iter('testcase'):
        tests.append({'name':case.attrib['name'], 'seconds':float(case.attrib.get('time',0)),
                      'failed':case.find('failure') is not None, 'skipped':case.find('skipped') is not None})
record = {'profile':profile, 'command':command, 'status':result.returncode, 'seconds':time.perf_counter()-started,
          'tests':tests, 'build_config':(build / 'CMakeCache.txt').read_text(),
          'environment':{key:env[key] for key in ('ASAN_OPTIONS','UBSAN_OPTIONS','PYTHONPATH') if key in env},
          'executables':{name:h.crash.digest(build / name) for name in ('voidc','voidls','void_collections_stdlib_host','void_operator_resolution')},
          'source_hashes':{str(path.relative_to(h.ROOT)):h.crash.digest(path) for folder in ('src','include','stdlib','tests/collections_stdlib') for path in (h.ROOT / folder).rglob('*') if path.is_file() and '__pycache__' not in path.parts}}
(receipt / 'receipt.json').write_text(json.dumps(record,indent=2)+'\n')
print(result.stdout.decode(errors='replace'),end='');print(result.stderr.decode(errors='replace'),end='')
assert not result.expectation_failed and tests and all(not item['failed'] and not item['skipped'] for item in tests), receipt
print(f'{profile}: {len(tests)}/{len(tests)} focused suites passed; {receipt}')
