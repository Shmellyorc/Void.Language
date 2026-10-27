"""Independent source check and linked Runtime-only native execution."""
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tests/random'))
import harness as h

compiler, native, source = map(lambda x: Path(x).resolve(), sys.argv[1:4])
small = sys.platform == 'linux'
h.run([compiler, 'check', source], ROOT, expected=b'Source checked successfully\n', sources=[source], small=small)
# Re-emit under the actual restricted stack and prove that the linked object
# has the same bytes. The native target links this object with Runtime only.
with tempfile.TemporaryDirectory(prefix='void-await-aot-') as temporary:
    emitted = Path(temporary) / 'syntax.o'
    h.run([sys.argv[5], '--emit', source, emitted, sys.argv[6]], ROOT, sources=[source], small=small)
    assert emitted.read_bytes() == Path(sys.argv[7]).read_bytes(), 'limited-stack AOT object differs from linked object'
expected = sys.argv[4]
if expected.startswith('failure:'):
    result = h.run([native, '7'], ROOT, status=1, sources=[source], small=small)
    assert expected[8:].encode() in result.stderr, result
else:
    h.run([native, '7'], ROOT, expected=(expected + '\n').encode(), sources=[source], small=small)
print(f'await syntax: independent source check and linked AOT passed; actual 1 MiB stack={small}')
