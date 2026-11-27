"""Generator language contracts through the same JIT and native entry paths."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from test_output import output_directory

compiler = Path(sys.argv[1]).resolve()
fixtures = Path(__file__).resolve().parent


def limits():
    import resource

    resource.setrlimit(resource.RLIMIT_STACK, (1048576, 1048576))
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def run(*command, status=0, output=None, error=None):
    result = subprocess.run(command, text=True, capture_output=True, timeout=55,
                            preexec_fn=limits if os.name == "posix" else None)
    info = (command, result.returncode, result.stdout, result.stderr)
    assert result.returncode == status, info
    if output is not None:
        assert result.stdout == output, info
    if error is not None:
        assert error.lower() in result.stderr.lower(), info
    assert "AddressSanitizer" not in result.stderr and "UndefinedBehaviorSanitizer" not in result.stderr, info


REENTRANT = """struct owner:
    current: generator<i32>?

generator func items(state: owner) -> i32:
    if state.current is Ok(var iterator):
        iterator.next()
    yield 1

func main():
    var state = owner()
    var sequence = items(state)
    state.current = sequence
    for item in sequence:
        print(item)
"""

TRANSFER = """async generator func items() -> i32:
    yield 1

async func main():
    var sequence = items()
    for await item in sequence:
        for await repeated in sequence:
            print(repeated)
"""

BAD = {
    "outside": ("func main():\n    yield 1\n", "yield requires a generator"),
    "returned": ("generator func invalid() -> i32:\n    return 1\n", "return takes no value"),
    "type": ("generator func invalid() -> i32:\n    yield true\n", "expected i32"),
    "wrong_protocol": (TRANSFER.replace("for await item", "for item"), "requires forward for await"),
    "readonly": (REENTRANT.replace("var sequence", "const sequence"), "readonly"),
}

CASES = {
    "sync": (0, "0 true\n0\n7\n123 true\n7 123\n123\n14 123\n106\n250000\n", None),
    "async": (0, "1140\n1120\n11 1 1 1\n", None),
    "failure": (1, "generator 7\n", "index"),
    "async_failure": (1, "child 7\ngenerator 7\n", "index"),
    "cancel": (1, "child 7\ngenerator 7\n", "cancelled"),
    "reentrant": (1, "", "reentrant resumption"),
    "transfer": (1, "", "already has a consuming scope"),
}

with tempfile.TemporaryDirectory(prefix="generators-", dir=output_directory()) as scratch:
    scratch = Path(scratch)
    for name, (source, error) in BAD.items():
        path = scratch / (name + ".void")
        path.write_text(source)
        run(compiler, "check", path, status=1, error=error)
    (scratch / "reentrant.void").write_text(REENTRANT)
    (scratch / "transfer.void").write_text(TRANSFER)
    for name, (status, output, error) in CASES.items():
        source = (scratch if name in {"reentrant", "transfer"} else fixtures) / (name + ".void")
        for flags in (("--no-opt",), ()):
            run(compiler, "run", source, "--no-cache", *flags, status=status, output=output, error=error)
            executable = scratch / ("test.exe" if os.name == "nt" else "test")
            run(compiler, "build", source, "-o", executable, *flags)
            run(executable, status=status, output=output, error=error)
            executable.unlink()
