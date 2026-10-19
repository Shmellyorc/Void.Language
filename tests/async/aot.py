import subprocess
import sys
result = subprocess.run([sys.argv[1], "7"], capture_output=True, text=True)
if sys.argv[2].startswith("failure:"):
    # The checked CLI failure status is 1; sanitizer exits/crashes must fail.
    assert result.returncode == 1 and sys.argv[2][8:] in result.stderr + result.stdout, result
else:
    assert result.returncode == 0 and result.stdout.strip() == sys.argv[2], result
