"""Run the actual memory stream module with a 1 MiB POSIX process stack."""
from pathlib import Path
import resource
import subprocess
import sys
import tempfile


compiler = Path(sys.argv[1]).resolve()
source = Path(__file__).with_name("stack.void")
checks = 0


def small_stack():
    resource.setrlimit(resource.RLIMIT_STACK, (1024 * 1024, 1024 * 1024))


def run(arguments, expected=None):
    global checks
    result = subprocess.run(list(map(str, arguments)), capture_output=True,
                            preexec_fn=small_stack, timeout=180)
    assert result.returncode == 0, (arguments, result.returncode, result.stdout, result.stderr)
    assert b"warning" not in result.stderr.lower(), result.stderr
    if expected is not None:
        assert result.stdout == expected, (arguments, result.stdout)
    checks += 1


with tempfile.TemporaryDirectory(prefix="VOID memory 1MiB ") as directory:
    run([compiler, "check", source])
    for flags in (["--no-opt"], []):
        run([compiler, "run", source, *flags], b"memory small stack passed\n")
        executable = Path(directory) / ("memory" + str(bool(flags)))
        run([compiler, "build", source, "-o", executable, *flags])
        run([executable], b"memory small stack passed\n")

print(f"Memory 1 MiB stack: {checks} processes passed (check, JIT/O2, linked build and native execution/O2)")
