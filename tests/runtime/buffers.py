import subprocess
import sys

subprocess.run([sys.argv[1]], check=True, timeout=150)
for mode in ("private-root", "private-edge"):
    result = subprocess.run([sys.argv[1], mode], timeout=20)
    if result.returncode != 79:
        raise SystemExit(f"{mode}: expected fatal invariant handler 79, got {result.returncode}")
print("private buffer roots/edges cannot bypass publication")
