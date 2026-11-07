"""Run the exact selected registered local commands with crash preservation."""
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
import json
import os
import re
import sys
import time
from crash import ROOT, run

profile = sys.argv[1]
build = ROOT / 'build' / ('ci-official-' + profile)
evidence = ROOT / 'docs/text-encoding/evidence'
pattern = json.loads((evidence / 'selection-debug.json').read_text())['regex']
selection = run(['ctest', '--test-dir', build, '--show-only=json-v1'], capture_output=True, check=True)
tests = [test for test in json.loads(selection.stdout)['tests'] if re.match(pattern, test['name'])]
record = {'profile': profile, 'regex': pattern, 'tests': tests}
(evidence / ('selection-' + profile + '.json')).write_text(json.dumps(record, indent=2) + '\n')
environment = dict(os.environ)
environment['VOID_CRASH_EVIDENCE'] = str(evidence / 'crashes')
environment['PYTHONPATH'] = str(Path(__file__).parent.resolve()) + os.pathsep + environment.get('PYTHONPATH', '')
if profile == 'sanitizers':
    environment['ASAN_OPTIONS'] = 'detect_leaks=1:halt_on_error=1:exitcode=87'
    environment['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1:exitcode=86'
started = time.monotonic()


def execute(test):
    properties = {item['name']: item['value'] for item in test.get('properties', [])}
    cwd = properties.get('WORKING_DIRECTORY', str(build))
    beginning = time.monotonic()
    result = run(test['command'], cwd=cwd, capture_output=True, timeout=max(1800, properties.get('TIMEOUT', 180)),
                 env=environment)
    log = evidence / (profile + '-' + test['name'] + '.log')
    log.write_bytes(result.stdout + b'\nSTDERR\n' + result.stderr)
    return {'name': test['name'], 'command': test['command'], 'cwd': cwd,
            'status': result.returncode, 'seconds': time.monotonic() - beginning, 'log': log.name}


results = []
# Keep installation outside the concurrent batch because its CTest is RUN_SERIAL.
parallel = [test for test in tests if test['name'] != 'text_encoding_install']
serial = [test for test in tests if test['name'] == 'text_encoding_install']
with ThreadPoolExecutor(max_workers=2) as pool:
    futures = [pool.submit(execute, test) for test in parallel]
    for future in as_completed(futures):
        result = future.result()
        results.append(result)
        print(result['name'], 'PASS' if result['status'] == 0 else 'FAIL', f"{result['seconds']:.3f}s", flush=True)
        (evidence / (profile + '-results.json')).write_text(json.dumps(results, indent=2) + '\n')
for test in serial:
    result = execute(test)
    results.append(result)
    print(result['name'], 'PASS' if result['status'] == 0 else 'FAIL', f"{result['seconds']:.3f}s", flush=True)
results.sort(key=lambda item: item['name'])
(evidence / (profile + '-results.json')).write_text(json.dumps(results, indent=2) + '\n')
print(json.dumps({'profile': profile, 'passed': sum(item['status'] == 0 for item in results),
                  'total': len(results), 'seconds': time.monotonic() - started}), flush=True)
raise SystemExit(any(item['status'] != 0 for item in results))
