"""Frozen pre-repair oracle: every work budget, error offset and path."""
from pathlib import Path
import tempfile
import harness as h

source = Path(__file__).with_suffix('.void')
expected = source.with_suffix('.stdout').read_bytes()
with tempfile.TemporaryDirectory(prefix='void-json-metering-') as temporary:
    for optimized in (False, True):
        flags = [] if optimized else ['--no-opt']
        h.run([h.compiler, 'run', source, '--no-cache', *flags], source, expected)
        native = Path(temporary) / ('metering-O2' if optimized else 'metering-O0')
        h.run([h.compiler, 'build', source, '-o', native, *flags], source)
        h.run([native], source, expected)
print('Frozen JSON budget/order oracle: four modes passed')
