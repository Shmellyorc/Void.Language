"""Bound receiver graph retention, mutable application state and GC without deinit."""
from pathlib import Path
import sys
import tempfile
import support as h

compiler = Path(sys.argv[1]).resolve()
source = h.ROOT / 'tests/channel/receiver.void'
with tempfile.TemporaryDirectory(prefix='VOID-channel-receiver-') as directory:
    native = Path(directory) / ('receiver.exe' if sys.platform == 'win32' else 'receiver')
    h.run([compiler, 'check', source], small=True)
    for optimize in (False, True):
        switches = [] if optimize else ['--no-opt']
        h.run([compiler, 'run', source, '--no-cache', *switches], expected=b'7\n', small=True)
        h.run([compiler, 'build', source, '-o', native, *switches], small=True)
        h.run([native], expected=b'7\n', small=True)
print('Channel bound receivers: 7 check/JIT/build/native operations passed')
