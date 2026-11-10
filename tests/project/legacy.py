"""Slice 1 must not opt existing CLI callers into manifest semantics."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

compiler = Path(sys.argv[1]).resolve()
checks = 0


def run(args, expected, cwd):
    global checks
    result = subprocess.run([str(compiler), *map(str, args)], cwd=cwd,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            timeout=90, check=False)
    assert result.returncode == expected, (args, result.returncode, result.stdout, result.stderr)
    checks += 1
    return result


with tempfile.TemporaryDirectory(prefix="void manifest legacy ") as temporary:
    root = Path(temporary)
    manifest = root / "void.project.toml"
    manifest_bytes = b"schema = 'not-supported'\n[hooks]\nrun = 'NEVER_EXECUTE'\n[native]\nload = 'NEVER_LOAD'\n"
    manifest.write_bytes(manifest_bytes)
    app = root / "game.void"
    app.write_text("func main() -> i32:\n    return 23\n", encoding="utf-8")
    main = root / "main.void"
    main.write_text("func main() -> i32:\n    return 5\n", encoding="utf-8")
    for options in ([], ["--no-opt"]):
        result = run(["run", app, *options], 23, root)
        assert not result.stdout and not result.stderr
        # Slice 2 selects an adjacent manifest only for directory operands.
        run(["run", root, *options], 1, root)
        manifest.rename(root / "manifest-held.toml")
        result = run(["run", root, *options], 5, root)
        assert not result.stdout and not result.stderr
        (root / "manifest-held.toml").rename(manifest)
    run(["check", app], 0, root)
    main.unlink()
    manifest.rename(root / "manifest-held.toml")
    result = run(["check", root], 1, root)
    assert b"main.void" in result.stderr
    (root / "manifest-held.toml").rename(manifest)
    run(["check", "--project", root], 1, root)
    # Malformed creation commands cannot touch the existing manifest or source.
    for args in (["init", root, "--source", "main.void"], ["new"]):
        run(args, 2, root)
    no_id = root / "missing_context.void"
    no_id.write_text(
        'use os\nfunc main() -> i32:\n'
        '    if os.data_dir() is Err(var reason):\n'
        '        if reason.code == 6 and reason.message == "Application identity is unavailable.":\n'
        '            return 19\n'
        '    return 1\n', encoding="utf-8")
    run(["check", no_id], 0, root)
    result = run(["run", no_id, "--no-cache"], 19, root)
    assert not result.stdout and not result.stderr
    # Native AOT/default output spelling remains tied to the explicit source file.
    result = run(["build", app], 0, root)
    executable = root / "build" / ("game.exe" if os.name == "nt" else "game")
    assert executable.is_file(), result.stdout
    result = subprocess.run([str(executable)], cwd=root, capture_output=True, timeout=30, check=False)
    assert result.returncode == 23 and not result.stdout and not result.stderr, result
    checks += 1
    assert manifest.read_bytes() == manifest_bytes
    assert not (root / "NEVER_EXECUTE").exists() and not (root / "new-project").exists()
    assert app.read_text(encoding="utf-8") == "func main() -> i32:\n    return 23\n"
    checks += 3
print(f"project_manifest_legacy: {checks} checks passed")
