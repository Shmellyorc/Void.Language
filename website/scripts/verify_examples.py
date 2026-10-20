#!/usr/bin/env python3
"""Compile and execute the website's exact downloadable teaching programs."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import textwrap

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('compiler', type=Path, help='Existing current voidc binary')
parser.add_argument('--async-build', type=Path, required=True,
                    help='Matching existing build containing void_async_tests and runtime archives')
parser.add_argument('--cxx', default='clang++-19', help='Matching C++ linker driver')
args = parser.parse_args()
compiler = args.compiler.resolve(strict=True)
build = args.async_build.resolve(strict=True)
emitter = (build / 'void_async_tests').resolve(strict=True)
archives = [(build / name).resolve(strict=True) for name in
            ('libvoid_runtime.a', 'libvoid_native_fixture.a')]
cxx = shutil.which(args.cxx)
if not cxx:
    parser.error(f'Cannot find C++ linker driver: {args.cxx}')
root = Path(__file__).resolve().parents[1]
samples = json.loads((root / 'scripts/guide-examples.json').read_text())
# Keep a real recoverable-error branch in the verification set.
samples.append({'source': 'public/examples/guides/errors.void', 'input': 0, 'expected': -1})

def run(command):
    result = subprocess.run([str(arg) for arg in command], capture_output=True,
                            text=True, timeout=60)
    if result.returncode:
        raise SystemExit(f'FAIL: {command}\n{result.stdout}\n{result.stderr}')
    return result.stdout.strip()

executed = async_compilations = 0
with tempfile.TemporaryDirectory(prefix='void-website-examples-') as temporary:
    output = Path(temporary)
    for index, sample in enumerate(samples):
        source = root / sample['source']
        if sample.get('kind') == 'fragment':
            # Compile the exact displayed fragment in its explicit teaching context.
            fragment = textwrap.indent(source.read_text(), ' ' * sample.get('context_indent', 0))
            source = output / f'fragment-{index}.void'
            source.write_text(sample.get('context_before', '') + fragment
                              + sample.get('context_after', ''))
        if sample.get('maintenance') == 'repository-copy':
            if source.read_bytes() != (root.parent / sample['repository_source']).read_bytes():
                raise SystemExit(f'FAIL: website copy differs from {sample["repository_source"]}')
        cases = [{'input': sample['input'], 'expected': sample['expected']}]
        cases += sample.get('cases', [])
        if sample.get('mode') == 'async':
            run([compiler, 'check', source])
            # Emit from this exact website source, never reuse fixture executables.
            # Both LLVM modes exercise the supported explicit async adapter.
            for optimization in ('normal', 'optimized'):
                obj = output / f'{index}-{optimization}.o'
                executable = output / f'{index}-{optimization}'
                run([emitter, '--emit', source, obj, optimization])
                run([cxx, obj, *archives, '-o', executable])
                async_compilations += 1
                for case in cases:
                    actual = run([executable, case['input']])
                    if actual != str(case['expected']):
                        raise SystemExit(f'FAIL: {source} {optimization} input {case["input"]}: '
                                         f'expected {case["expected"]}, got {actual}')
                    executed += 1
        else:
            modes = ([], ['--no-opt']) if sample.get('guide') or sample.get('showcase') else ([],)
            for flags in modes:
                for case in cases:
                    actual = run([compiler, 'run', source, case['input'], *flags])
                    if actual != str(case['expected']):
                        raise SystemExit(f'FAIL: {source} input {case["input"]}: '
                                         f'expected {case["expected"]}, got {actual}')
                    executed += 1
print(f'PASS: {executed} result checks; {async_compilations} fresh async AOT builds; all website programs executed')
