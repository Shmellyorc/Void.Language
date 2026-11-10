import subprocess
import sys

result = subprocess.run(
    [sys.argv[1], "--build", sys.argv[2], "--config", sys.argv[3],
     "--target", "void_g1_aot_missing"], text=True, capture_output=True, timeout=150
)
output = result.stdout + result.stderr
print(output, end="")
assert result.returncode != 0 and "vd_c_primitive_dispatch_v1" in output, result
assert "undefined symbol" in output or "unresolved external symbol" in output, result
