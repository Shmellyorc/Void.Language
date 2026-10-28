"""Keep compiler/runner assertions inside the shared recorder's scope."""
from pathlib import Path
import sys
import tempfile
import support as h
exe = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='VOID-collections-host-') as directory:
    result = h.run([exe, h.ROOT, directory], sources=[h.ROOT / 'tests/collections_stdlib', Path(directory)], timeout=900,
                   context={'mode': 'host-gc-allocation-cache-reload'})
    print(result.stdout.decode(), end='')
