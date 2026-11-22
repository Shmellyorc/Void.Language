"""Real source booleans across Runtime calls in JIT and independently linked AOT."""
from pathlib import Path
import os
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "text_encoding"))
from crash import run as run_preserving

compiler, dwarf = map(str, map(Path, sys.argv[1:3]))
SOURCE = '''func main():
    for i in 0..<4:
        var value = i % 2 == 0
        var stored = not value
        var left = "hello {i}"
        var right = "hello {i % 2}"
        print(value, stored, left == right, flush = stored)
'''
EXPECTED = "true false true\nfalse true true\ntrue false false\nfalse true false\n"


def run(*argv):
    result = run_preserving(list(map(str, argv)), capture_output=True, text=True, timeout=90,
                            source_inputs=(source,), provenance_inputs=(Path(__file__).resolve(),))
    assert result.returncode == 0, (argv, result.stdout, result.stderr)
    return result.stdout


with tempfile.TemporaryDirectory(prefix="VOID Runtime bool ABI ") as directory:
    root = Path(directory)
    source = root / "main.void"
    source.write_text(SOURCE, encoding="utf-8")
    for optimized in (False, True):
        flags = [] if optimized else ["--no-opt"]
        assert run(compiler, "run", source, "--no-cache", *flags) == EXPECTED
        binary = root / ("bool.exe" if os.name == "nt" else "bool")
        run(compiler, "build", source, "-o", binary, *flags)
        assert run(binary) == EXPECTED
        obj = root / "bool.o"
        run(compiler, "emit", source, "-o", obj, *flags)
        run(dwarf, "--verify", obj)
        print(f"PASS: {'O2' if optimized else 'O0'} JIT/AOT booleans and verified standalone DWARF")
