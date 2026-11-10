import subprocess
import sys

for value, status, expected in [(7, 0, "104"), (-1, 1, "closed, null or invalid"),
                                 (-2, 1, "closed, null or invalid"),
                                 (-3, 1, "Null"), (-4, 0, "22")]:
    result = subprocess.run([sys.argv[1], str(value)], text=True, capture_output=True, timeout=45)
    output = result.stdout + result.stderr
    print(value, result.returncode, output.strip())
    assert result.returncode == status, result
    assert (result.stdout.strip() == expected if status == 0 else expected in output), result
