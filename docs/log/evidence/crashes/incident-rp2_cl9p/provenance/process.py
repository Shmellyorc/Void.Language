"""Established shared crash envelope for CMake's generated native probes."""
from pathlib import Path
import sys
import support as h
result=h.run(sys.argv[1:],small=True,sources=[Path(__file__).parent],timeout=1800)
print(result.stdout.decode(),end='')
print(result.stderr.decode(),end='',file=sys.stderr)
