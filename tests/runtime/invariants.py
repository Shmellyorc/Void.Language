import subprocess
import sys

for case in ("corrupt-root", "frame-order"):
    result = subprocess.run([sys.argv[1], case], timeout=20)
    if result.returncode != 79:
        raise SystemExit(f"{case}: expected fatal invariant handler 79, got {result.returncode}")
print("corrupt roots and frame order cannot resume managed execution")
