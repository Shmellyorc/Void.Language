#!/usr/bin/env python3
"""Validate the website's programs using an existing current VOID compiler."""
import argparse
import json
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('compiler', type=Path, help='Path to a current voidc binary')
parser.add_argument('--async-artifacts', type=Path,
                    help='Existing test-build directory containing optimized async AOT examples')
args = parser.parse_args()
compiler = args.compiler.resolve(strict=True)
root = Path(__file__).resolve().parents[1]
samples = json.loads((root / 'scripts/guide-examples.json').read_text())
samples.append({'source': 'public/examples/guides/errors.void', 'input': 0, 'expected': -1})
executed = checked = 0
for sample in samples:
    source = root / sample['source']
    if 'repository_source' in sample:
        if source.read_bytes() != (root.parent / sample['repository_source']).read_bytes():
            raise SystemExit(f'FAIL: website copy differs from {sample["repository_source"]}')
    if sample.get('mode') == 'check':
        result = subprocess.run([str(compiler), 'check', str(source)],
                                capture_output=True, text=True, timeout=30)
        if result.returncode:
            raise SystemExit(f'FAIL: {source}\n{result.stderr}')
        checked += 1
        if not args.async_artifacts:
            continue
        artifact = (args.async_artifacts / f'void_async_aot_{sample["async_artifact"]}_optimized').resolve(strict=True)
        command = [str(artifact), str(sample['input'])]
    else:
        command = [str(compiler), 'run', str(source), str(sample['input'])]
    result = subprocess.run(command, capture_output=True, text=True, timeout=30)
    if result.returncode or result.stdout.strip() != str(sample['expected']):
        raise SystemExit(f'FAIL: {sample["source"]} input {sample["input"]}\n'
                         f'Expected {sample["expected"]}; received {result.stdout}\n{result.stderr}')
    executed += 1
print(f'PASS: {executed} example executions; {checked} async source checks; repository copies match')
if checked and not args.async_artifacts:
    print('Async execution not rerun; provide --async-artifacts to use existing test-suite AOT programs.')
