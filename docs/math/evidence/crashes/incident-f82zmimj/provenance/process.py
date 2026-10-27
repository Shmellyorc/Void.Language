"""Crash-preserving wrapper for a native source test process."""
import sys
from crash import run

result = run(sys.argv[1:], capture_output=True, timeout=1800)
sys.stdout.buffer.write(result.stdout)
sys.stderr.buffer.write(result.stderr)
raise SystemExit(result.returncode if result.returncode >= 0 else 1)
