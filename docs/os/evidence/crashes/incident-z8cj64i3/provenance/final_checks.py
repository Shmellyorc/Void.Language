"""Final corrected harness, added cases and rebuilt Unicode helper verification."""
from pathlib import Path
import json
import os
import sys
import time
from crash import ROOT, run

profile = sys.argv[1]
build = ROOT / 'build' / ('ci-official-' + profile)
evidence = ROOT / 'docs/text-encoding/evidence'
selected = {'text_encoding_text', 'text_encoding_errors', 'text_encoding_ownership',
            'text_encoding_ir', 'text_encoding_inventory', 'lsp_text_encoding',
            'text_encoding_crash_preservation', 'text_encoding_depth', 'text_encoding_install'}
listing = run(['ctest', '--test-dir', build, '--show-only=json-v1'], capture_output=True, check=True)
tests = [test for test in json.loads(listing.stdout)['tests'] if test['name'] in selected]
assert {test['name'] for test in tests} == selected
options = dict(os.environ)
options['VOID_CRASH_EVIDENCE'] = str(evidence / 'crashes')
options['PYTHONPATH'] = str(Path(__file__).parent.resolve()) + os.pathsep + options.get('PYTHONPATH', '')
if profile == 'sanitizers':
    options['ASAN_OPTIONS'] = 'detect_leaks=1:halt_on_error=1:exitcode=87'
    options['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1:exitcode=86'
records = []
for test in tests:
    properties = {item['name']: item['value'] for item in test.get('properties', [])}
    cwd = properties.get('WORKING_DIRECTORY', str(build))
    started = time.monotonic()
    result = run(test['command'], cwd=cwd, capture_output=True, env=options, timeout=1800)
    log = evidence / ('final-' + profile + '-' + test['name'] + '.log')
    log.write_bytes(result.stdout + b'\nSTDERR\n' + result.stderr)
    records.append({'name': test['name'], 'command': test['command'], 'cwd': cwd,
                    'status': result.returncode, 'seconds': time.monotonic() - started, 'log': log.name})
    print(test['name'], result.returncode, flush=True)
    (evidence / ('final-checks-' + profile + '.json')).write_text(json.dumps(records, indent=2) + '\n')
assert all(item['status'] == 0 for item in records), records
print(f'{len(records)} final corrected/rebuilt checks passed', flush=True)
