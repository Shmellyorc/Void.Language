import subprocess
import sys

subprocess.run([sys.argv[1]], check=True, timeout=150)
for mode in ("private-root", "private-edge", "ref-spare", "ref-spare-witness", "ref-required-null", "ref-wrong-target", "ref-wrong-string", "ref-forged-witness", "ref-private-edge", "ref-private-spare"):
    result = subprocess.run([sys.argv[1], mode], timeout=20)
    if result.returncode != 79:
        raise SystemExit(f"{mode}: expected fatal invariant handler 79, got {result.returncode}")
print("private buffer roots/edges cannot bypass publication")

# Leak detection must reject both outstanding bytes and a zero-byte allocation.
# The child releases the deliberate allocation only after the strict check fails,
# so sanitizers still run cleanly without turning the expected rejection green.
for mode, size in (("reference-leak-bytes", 37), ("reference-leak-zero", 0)):
    result = subprocess.run([sys.argv[1], mode], capture_output=True, text=True, timeout=20)
    expected = f"native_count=1 native_bytes={size}"
    if (result.returncode != 1 or expected not in result.stderr
            or "reference failure sweep leaked native storage" not in result.stderr):
        raise SystemExit(f"{mode}: strict leak control failed: {result.returncode}\n{result.stdout}\n{result.stderr}")
print("post-owner native leak checks reject bytes and zero-byte allocations")
