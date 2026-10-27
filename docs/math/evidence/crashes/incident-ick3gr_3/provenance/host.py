"""Preserve snapshots emitted by the native cache/reload/GC harness."""
from pathlib import Path
import sys
import bootstrap as h
exe=Path(sys.argv[1]).resolve()
work=Path(sys.argv[2]).resolve();work.mkdir(parents=True,exist_ok=True)
result=h.run([exe,work],work,timeout=600)
print(result.stdout.decode(),end='')
