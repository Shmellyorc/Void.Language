"""Keep README/Wiki examples executable and tied to canonical repository sources."""
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from test_platform import native_manifest

compiler = Path(sys.argv[1]).resolve()
build = Path(sys.argv[2]).resolve()
cases = {'basics': (5, 9), 'types': (2, 120), 'functions': (3, 36),
         'collections': (5, 35), 'memory': (2, 19), 'async': (2, 27)}
canonical = {name: (ROOT / 'examples/learn' / (name + '.void')).read_text(encoding='utf-8') for name in cases}
hello = (ROOT / 'examples/hello/main.void').read_text(encoding='utf-8')
documents = [ROOT / 'README.md', ROOT / 'docs/FIRST_PROGRAM.md', *(ROOT / 'docs/wiki').glob('*.md')]
blocks = 0
for document in documents:
    for snippet in re.findall(r'```void\n(.*?)\n```', document.read_text(encoding='utf-8'), re.S):
        assert snippet + '\n' in [hello, *canonical.values()], f'Unverified/divergent example: {document}'
        blocks += 1
assert blocks == 9, blocks


def run(argv):
    result = subprocess.run(list(map(str, argv)), capture_output=True, text=True, encoding='utf-8', timeout=120)
    assert result.returncode == 0, (argv, result.returncode, result.stdout, result.stderr)
    return result.stdout.strip()


with tempfile.TemporaryDirectory(prefix='VOID learning é ') as directory:
    scratch = Path(directory)
    for name, (value, expected) in cases.items():
        source = scratch / (name + '.void')
        source.write_text(canonical[name], encoding='utf-8')
        run([compiler, 'check', source])
        if name != 'async':
            for flags in ([], ['--no-opt', '--no-cache']):
                assert run([compiler, 'run', source, value, *flags]) == str(expected)
            print(f'PASS: {name} input {value} => {expected} in both JIT modes')
            continue
        suffix = '.exe' if os.name == 'nt' else ''
        emitter = build / ('void_async_tests' + suffix)
        link = native_manifest(compiler)
        flags = ['--driver-mode=g++', *link[3:]]
        if link[2]:
            flags.append('--ld-path=' + link[2])
        for mode in ('normal', 'optimized'):
            obj = scratch / ('async-' + mode + '.o')
            output = scratch / ('async-' + mode + suffix)
            run([emitter, '--emit', source, obj, mode])
            run([link[1], obj, *flags, '-o', output])
            assert run([output, value]) == str(expected)
        print(f'PASS: async input {value} => {expected} in normal and optimized AOT modes')
print(f'PASS: {blocks} README/first-program/Wiki blocks match canonical examples; 12 execution cases')
