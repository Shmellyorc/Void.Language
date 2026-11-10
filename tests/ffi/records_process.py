import subprocess
import sys

for value, expected in [(7, "103"), (-1, "outside admitted extent"),
                        (-2, "overlapping C byte spans"),
                        (-3, "oversized written prefix"), (-4, "outside admitted extent")]:
    result = subprocess.run([sys.argv[1], str(value)], text=True, capture_output=True, timeout=45)
    print(value, result.returncode, (result.stdout + result.stderr).strip())
    if value > 0:
        assert result.returncode == 0 and result.stdout.strip() == expected, result
    else:
        assert result.returncode == 1 and expected in result.stdout + result.stderr, result
