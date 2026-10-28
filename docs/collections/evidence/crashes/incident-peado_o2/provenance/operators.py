"""Snapshot generated Stage 10 inputs for the existing shared crash recorder."""
from pathlib import Path
import os
import sys
import tempfile
import support as h
with tempfile.TemporaryDirectory(prefix='VOID-operator-source-snapshots-') as directory:
    env = dict(os.environ, VOID_TEST_SOURCE_SNAPSHOTS=directory)
    result = h.run([Path(sys.argv[1]).resolve(), h.ROOT], env=env,
                   sources=[Path(directory), h.ROOT / 'tests/stage10'],
                   context={'mode': 'generic-operator-regressions'})
    print(result.stdout.decode(), end='')
