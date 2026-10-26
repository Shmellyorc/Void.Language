"""Actual source codec execution and linked output on a 1 MiB POSIX stack."""
from pathlib import Path
import resource
import subprocess
import sys
import tempfile


compiler = Path(sys.argv[1]).resolve()
source = Path(__file__).with_name("ownership.void")
checks = 0


def small_stack():
    resource.setrlimit(resource.RLIMIT_STACK, (1024 * 1024, 1024 * 1024))


def run(arguments, expected=None):
    global checks
    result = subprocess.run(list(map(str, arguments)), capture_output=True,
                            preexec_fn=small_stack, timeout=120)
    assert result.returncode == 0, (arguments, result.returncode, result.stderr)
    assert b"warning" not in result.stderr.lower(), result.stderr
    if expected is not None:
        assert result.stdout == expected, (arguments, result.stdout)
    checks += 1


with tempfile.TemporaryDirectory(prefix="VOID binary 1MiB ") as directory:
    run([compiler, "check", source])
    for flags in (["--no-opt"], []):
        run([compiler, "run", source, "12345", *flags], b"12345\n")
        executable = Path(directory) / ("binary" + str(bool(flags)))
        run([compiler, "build", source, "-o", executable, *flags])
        run([executable, "12345"], b"12345\n")

print(f"Binary 1 MiB stack: {checks} processes passed (check, JIT/O2, linked build/execution/O2)")
