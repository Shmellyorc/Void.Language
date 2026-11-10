"""Prove the Linux denial controls are live, then run all parser cases under them."""
from pathlib import Path
import resource
import signal
import subprocess
import sys

resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
fixture = str(Path(sys.argv[1]).resolve())
for operation in ("file", "process", "network", "mutation"):
    result = subprocess.run([fixture, "--deny-" + operation], capture_output=True, timeout=20, check=False)
    assert result.returncode == -signal.SIGSYS, (operation, result.returncode, result.stdout, result.stderr)
result = subprocess.run([fixture, "--sandbox"], capture_output=True, timeout=30, check=False)
assert result.returncode == 0, (result.returncode, result.stdout, result.stderr)
sys.stdout.buffer.write(result.stdout)
print("project_manifest_no_authority: four live SIGSYS controls passed")
