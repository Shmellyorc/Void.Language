from pathlib import Path
import sys
import harness as h
exe=Path(sys.argv[1]).resolve()
work=Path(sys.argv[2]).resolve();work.mkdir(parents=True,exist_ok=True)
result=h.run([exe,work],work,timeout=600)
print(result.stdout.decode(),end='')
