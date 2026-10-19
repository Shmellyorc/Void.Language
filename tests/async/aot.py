import subprocess
import sys
result = subprocess.run([sys.argv[1], "7"], capture_output=True, text=True)
assert result.returncode == 0 and result.stdout.strip() == sys.argv[2], result
