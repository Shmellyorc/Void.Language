"""A deep receiver must reach the semantic guard before exhausting the stack.

This reproduces run #38's Release failure independently of JIT or other tests.
Linux children use 1 MiB, matching the default Windows executable stack reserve.
Keep the ordinary compiler optimization and the existing 256-depth limit intact.
"""
import pathlib
import re
import subprocess
import sys
import tempfile


def small_stack():
    import resource
    resource.setrlimit(resource.RLIMIT_STACK, (1048576, 1048576))


with tempfile.TemporaryDirectory(prefix="void-semantic-depth-") as directory:
    source = pathlib.Path(directory) / "deep-methods.void"
    prefix = ("value struct point:\n    x: i32\n"
              "func (p point) same() -> point:\n    return p\n"
              "func (p point) inspect() -> i32:\n    return p.x\n"
              "func entry(value: i32) -> i32:\n    return point()")
    # Ordinary chains still compile; malformed deep chains must recover below.
    source.write_text(prefix + ".same()" * 16 + ".inspect()\n", encoding="utf-8")
    accepted = subprocess.run(
        [sys.argv[1], "check", str(source)], capture_output=True,
        encoding="utf-8", errors="strict", timeout=20,
        preexec_fn=small_stack if sys.platform == "linux" else None)
    assert accepted.returncode == 0, (accepted.returncode, accepted.stdout, accepted.stderr)
    assert accepted.stderr == "", accepted.stderr
    for suffix in (".inspect()", ".inspect() + 1"):
        text = prefix + ".same()" * 400 + suffix + "\nfunc later():\n    missing()\n"
        source.write_text(text, encoding="utf-8")
        result = subprocess.run(
            [sys.argv[1], "check", str(source)], capture_output=True,
            encoding="utf-8", errors="strict", timeout=20,
            preexec_fn=small_stack if sys.platform == "linux" else None)
        assert result.returncode == 1, (result.returncode, result.stdout, result.stderr)
        assert result.stdout == "", result.stdout
        diagnostic = result.stderr
        assert diagnostic.count("code: VOID1107") == 1, diagnostic
        assert diagnostic.count("code: VOID2003") == 1, diagnostic
        assert diagnostic.count(": error:") == 2, diagnostic
        nesting = re.search(r":8:(\d+): error: Source nesting limit exceeded", diagnostic)
        assert nesting, diagnostic
        column = int(nesting.group(1))
        assert text.splitlines()[7][column - 1:column + 3] == "same", diagnostic
        assert ":10:5: error: Unknown function 'missing'" in diagnostic, diagnostic
    print("Deep receiver chains: bounded stack, nesting location and later error verified")
