import subprocess
import sys

subprocess.run([sys.argv[1]], check=True, timeout=150)
for mode in ("private-root", "private-edge", "ref-spare", "ref-spare-witness", "ref-required-null", "ref-wrong-target", "ref-wrong-string", "ref-forged-witness", "ref-private-edge", "ref-private-spare"):
    result = subprocess.run([sys.argv[1], mode], timeout=20)
    if result.returncode != 79:
        raise SystemExit(f"{mode}: expected fatal invariant handler 79, got {result.returncode}")
print("private buffer roots/edges cannot bypass publication")
