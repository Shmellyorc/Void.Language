"""First end-to-end authority gate (not merely frontend checks)."""
from pathlib import Path
import sys
import support as h
compiler = Path(sys.argv[1]).resolve()
fixture = Path(__file__).with_name('authority.void')
h.run([compiler, 'check', fixture])
for switches in (['--no-opt'], []):
    h.run([compiler, 'run', fixture, '--no-cache', *switches], expected=b'7\n')
