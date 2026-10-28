from pathlib import Path
import sys
import support as h
compiler=Path(sys.argv[1]).resolve()
source=h.ROOT/'tests/os/smoke.void'
r=h.run([compiler,'check',source],h.ROOT)
print(r.stdout.decode(),end='')
