"""Own generated output for a test invocation."""
from functools import cache
import os
from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory


@cache
def _workspace(label):
    diagnostics = os.environ.get("VOID_TEST_DIAGNOSTICS_DIR")
    if diagnostics:
        parent = Path(diagnostics).resolve()
        parent.mkdir(parents=True, exist_ok=True)
    else:
        parent = None
    directory = TemporaryDirectory(prefix=f"void-test-{label}-", dir=parent, delete=not diagnostics)
    if diagnostics:
        print(f"Test diagnostics retained in {directory.name}", file=sys.stderr, flush=True)
    return directory


def output_directory():
    """Return the launcher's directory or allocate one for a direct Python test."""
    inherited = os.environ.get("VOID_TEST_OUTPUT_DIR")
    if inherited:
        return Path(inherited)
    return Path(_workspace(Path(sys.argv[0]).stem).name)


def main():
    label, *command = sys.argv[1:]
    private_cwd = command[0] == "--chdir"
    if private_cwd:
        command = command[1:]
    with _workspace(label) as directory:
        environment = dict(os.environ, VOID_TEST_OUTPUT_DIR=directory)
        command = [arg.replace("@VOID_TEST_OUTPUT@", directory) for arg in command]
        result = subprocess.run(command, env=environment, cwd=directory if private_cwd else None)
        if result.returncode and not os.environ.get("VOID_TEST_DIAGNOSTICS_DIR"):
            # Some native/HTTP harnesses capture their child's output to files.
            logs = [p for p in Path(directory).rglob("*") if p.is_file() and
                    p.name.endswith((".stderr", "stderr.bin", ".stdout", "stdout.bin")) and p.stat().st_size]
            for log in sorted(logs, key=lambda p: p.stat().st_mtime_ns, reverse=True)[:6]:
                with log.open("rb") as stream:
                    stream.seek(max(0, log.stat().st_size - 4096))
                    print(f"{log.relative_to(directory)}\n{stream.read().decode('utf-8', errors='backslashreplace')}",
                          file=sys.stderr)
            print("Set VOID_TEST_DIAGNOSTICS_DIR to retain a rerun's full diagnostics.", file=sys.stderr)
        return result.returncode if result.returncode >= 0 else 128 - result.returncode


if __name__ == "__main__":
    sys.exit(main())
