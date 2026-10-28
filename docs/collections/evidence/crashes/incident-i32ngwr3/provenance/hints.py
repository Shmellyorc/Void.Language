"""Shared inference regressions through uncached JIT and independently linked AOT."""
from pathlib import Path
import os
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tests/random'))
import harness as h
os.environ['VOID_CRASH_EVIDENCE'] = str(ROOT / 'docs/compiler-health/stabilization/incidents')
compiler = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='void-hint-contracts-') as temporary:
    for name in ['nested_direct', 'nested_list', 'nested_dictionary', 'double_propagate',
                 'choice_in_argument', 'choice_pattern', 'dictionary_identity', 'order', 'readonly']:
        source = Path(__file__).with_suffix('') / (name + '.void')
        expected = b'1\n' if name == 'nested_dictionary' else b'7\n'
        for optimized in (False, True):
            flags = [] if optimized else ['--no-opt']
            h.run([compiler, 'run', source, '--no-cache', *flags], ROOT, expected=expected, sources=[source], small=True)
            native = Path(temporary) / (name + ('-O2' if optimized else '-O0'))
            h.run([compiler, 'build', source, '-o', native, *flags], ROOT, sources=[source], small=True)
            h.run([native], ROOT, expected=expected, sources=[source], small=True)
print(f'Shared hint contracts: {h.checks} process checks on 1 MiB stacks passed')
