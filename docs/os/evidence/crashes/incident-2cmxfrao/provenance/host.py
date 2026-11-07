from pathlib import Path
import sys
import support as h
exe=Path(sys.argv[1]).resolve();work=Path(sys.argv[2]).resolve()
work.mkdir(parents=True,exist_ok=True)
r=h.run([exe,work],work,timeout=900,small="--small-stack" in sys.argv)
print(r.stdout.decode(),end='')
