"""Automatic optimized caching, policy separation and fresh CLI application state."""
import concurrent.futures
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import shutil

compiler = Path(sys.argv[1]).resolve()
def small_stack():
    import resource
    resource.setrlimit(resource.RLIMIT_STACK, (1048576, 1048576))
with tempfile.TemporaryDirectory(prefix="void-automatic-cache-") as temporary:
    root = Path(temporary)
    source = root / "main.void"
    source.write_text("var counter: i32 = 0\nfunc main():\n    counter += 1\n    print(counter)\n")
    directory = root / ".void/cache/v1"

    def run(flags=(), expected="1\n", status=0):
        result = subprocess.run([str(compiler), "run", str(source), *flags], capture_output=True, text=True, timeout=60,
                                preexec_fn=small_stack if sys.platform == "linux" else None)
        assert result.returncode == status and result.stdout == expected, result
        return result

    run(["--no-cache"])
    assert not (root / ".void").exists(), "bypass created a persistent cache"
    run()
    supported = directory.exists() and any(directory.iterdir())
    if supported:
        optimized = next(directory.iterdir())
        published = (optimized.stat().st_mtime_ns, optimized.read_bytes())
        run()
        assert published == (optimized.stat().st_mtime_ns, optimized.read_bytes()), "warm object republished"
        run(["--no-opt"])
        assert len(list(directory.iterdir())) == 2, "optimization modes shared an identity"
        assert "Debug cache: hit" in run(["--cache"]).stderr, "legacy --cache lost no-IR-O2 behavior"
        before = {p.name: p.stat().st_mtime_ns for p in directory.iterdir()}
        run(["--no-cache"])
        assert before == {p.name: p.stat().st_mtime_ns for p in directory.iterdir()}
        optimized.write_bytes(b"corrupt optimized object")
        run()
        # Publication never replaces an existing winner. Corruption is a safe
        # miss until that candidate is removed, matching the existing cache.
        assert optimized.read_bytes() == b"corrupt optimized object"
        optimized.unlink()
        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
            list(pool.map(lambda _: run(), range(4)))
        assert len(list(directory.iterdir())) == 2
        if os.name != "nt":
            optimized.chmod(0o666)
            run()
            assert optimized.stat().st_mode & 0o077, "unsafe candidate unexpectedly overwritten"
            optimized.unlink()
            optimized.symlink_to(root / "missing")
            run()
            assert optimized.is_symlink(), "unsafe symlink unexpectedly replaced"
            optimized.unlink()
        run()
        interrupted = directory / "tmp-interrupted"
        interrupted.write_bytes(b"partial")
        run()
        assert interrupted.read_bytes() == b"partial"
        interrupted.unlink()
    # Exercise default optimized invalidation with exact artifact identities.
    helper = root / "helper.void"
    helper.write_text('func amount() -> i32:\n    return 2\n')
    source.write_text('use "/helper.void" as h\nfunc main():\n    print(h.amount())\n')
    run(expected="2\n")
    before = set(directory.iterdir()) if supported else set()
    run(expected="2\n")
    assert not supported or before == set(directory.iterdir())
    helper.write_text('func amount() -> i32:\n    return 3\n')
    run(expected="3\n")
    assert not supported or len(set(directory.iterdir()) - before) == 1, "import edit reused stale identity"
    before = set(directory.iterdir()) if supported else set()
    source.write_text('use "/helper.void" as h\nfunc main():\n    print(h.amount() + 1)\n')
    run(expected="4\n")
    assert not supported or len(set(directory.iterdir()) - before) == 1, "source edit reused stale identity"
    if supported:
        shutil.rmtree(root / ".void")
        (root / ".void").write_text("unavailable cache directory")
        run(expected="4\n")
        assert (root / ".void").read_text() == "unavailable cache directory"
        (root / ".void").unlink()
    # Full diagnostics precede cache reuse, including unused imported bodies.
    helper.write_text('func unused() -> i32:\n    return "invalid"\n')
    source.write_text('use "/helper.void" as h\nfunc main():\n    print(1)\n')
    assert "Type mismatch" in run(expected="", status=1).stderr
    result = run(["--cache", "--no-cache"], expected="", status=2)
    assert "cannot be combined" in result.stderr
print("Automatic O2/no-IR-O2 caching, bypass, fresh globals and diagnostics passed")
