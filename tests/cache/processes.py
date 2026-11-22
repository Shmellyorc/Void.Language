"""Real CLI processes; disposable private fixtures, no compiler substitute."""
import concurrent.futures
import hashlib
import hmac
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

compiler, repository = map(Path, sys.argv[1:])
with tempfile.TemporaryDirectory(prefix="void-cache-") as temporary:
    root = Path(temporary)
    source = root / "main.void"
    source.write_text("func entry(value: i32) -> i32:\n    return value + 1\n")

    def run(expected=8, mode=None, **options):
        result = subprocess.run([str(compiler), "run", str(source), "7", "--cache"], capture_output=True, text=True, **options)
        assert result.returncode == 0 and result.stdout.strip() == str(expected), result
        if mode:
            assert f"Debug cache: {mode}" in result.stderr, result
        return result

    first = run()
    supported = "Debug cache: unavailable" not in first.stderr
    if supported:
        run(mode="hit")
        directory = root / ".void/cache/v1"
        entry = next(directory.iterdir())
        valid = entry.read_bytes()
        if os.name != "nt":
            credential = root / '.void/cache' / f'.authentication-{os.geteuid()}'
            secret = credential.read_bytes()
            assert credential.stat().st_mode & 0o777 == 0o600
            assert len(secret) == 32
            assert valid[-64:] == hmac.new(secret, b'VOID native cache v2\n' + valid[64:-64], hashlib.sha256).hexdigest().encode()
        interrupted = directory / "tmp-interrupted"
        interrupted.write_bytes(b"partial")
        run(mode="hit")
        interrupted.unlink()
        for broken in [b"short", valid[:100], valid[:-1], valid[:128] + b"9" + valid[129:]]:
            entry.write_bytes(broken)
            run(mode="miss")
        # Even a correctly authenticated envelope cannot hide an invalid object.
        body = valid[64:129] + valid[129:193]
        body += hmac.new(secret, b'VOID native cache v2\n' + body, hashlib.sha256).hexdigest().encode()
        entry.write_bytes(hashlib.sha256(body).hexdigest().encode() + body)
        run(mode="miss")
        entry.write_bytes(valid)
        run(mode="hit")
        # Another shared-directory writer can recompute the public checksum,
        # but cannot supply a valid authentication tag for changed code.
        forged = valid[64:-65] + bytes([valid[-65] ^ 1]) + valid[-64:]
        entry.write_bytes(hashlib.sha256(forged).hexdigest().encode() + forged)
        run(mode="miss")
        entry.write_bytes(valid)
        run(mode="hit")
        # Ordinary modes retain cache hits; symbolic links remain rejected.
        if os.name != "nt":
            entry.chmod(0o666)
            run(mode="hit")
            entry.chmod(0o600)
            entry.unlink()
            entry.symlink_to(root / "missing")
            run(mode="miss")
            entry.unlink()
        shutil.rmtree(root / ".void")
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
            list(pool.map(lambda _: run(), range(8)))
        run(mode="hit")
        assert len(list((root / ".void/cache/v1").iterdir())) == 1
        # A directory at the key must not block or be overwritten.
        entry = next((root / ".void/cache/v1").iterdir())
        entry.unlink(); entry.mkdir()
        run(mode="miss")
        assert entry.is_dir()
        shutil.rmtree(root / ".void")
        if os.name != "nt":
            import resource
            def limit_file_size():
                resource.setrlimit(resource.RLIMIT_FSIZE, (1, 1))
            run(mode="miss", preexec_fn=limit_file_size)
            assert not list((root / ".void/cache/v1").iterdir())
            run(mode="miss")
            run(mode="hit")
            shutil.rmtree(root / ".void")
    else:
        run(mode="unavailable")

    # Each string local and expression has a precise native root. This makes
    # the ordinary generated frame exceed a Windows stack page, requiring the
    # real ABI stack-probe helper. Exercise fresh source generations and objects.
    source.write_text("func entry(value: i32) -> i32:\n" +
                      "".join(f'    var text{i} = "root{i}"\n' for i in range(640)) +
                      "    return value + 1\n")
    run(mode="miss" if supported else "unavailable")
    run(mode="hit" if supported else "unavailable")
    for flags in [["--no-opt"], []]:
        result = subprocess.run([str(compiler), "run", str(source), "7", *flags], capture_output=True, text=True)
        assert result.returncode == 0 and result.stdout.strip() == "8", result

    source.write_text("use \"/helper.void\" as h\nfunc entry(value: i32) -> i32:\n    return h.add(value)\n")
    helper = root / "helper.void"
    helper.write_text("func add<t>(value: t) -> t:\n    return value + 2\n")
    run(9)
    run(9, "hit" if supported else "unavailable")
    helper.write_text("func add<t>(value: t) -> t:\n    return value + 3\n")
    run(10, "miss" if supported else "unavailable")
    helper.write_text("func add(value: i32) -> i32:\n    return \"invalid\"\n")
    invalid = subprocess.run([str(compiler), "run", str(source), "7", "--cache"], capture_output=True, text=True)
    assert invalid.returncode == 1 and "Type mismatch" in invalid.stderr and "Debug cache:" not in invalid.stderr
    for text in ["func entry(value: i64) -> i32:\n    return 0\n", "func entry<t>(value: i32) -> i32:\n    return value\n"]:
        source.write_text(text)
        results = [subprocess.run([str(compiler), "run", str(source), "7", flag], capture_output=True, text=True) for flag in ["--no-opt", "--cache"]]
        assert results[0].returncode == results[1].returncode == 1
        assert results[0].stderr == results[1].stderr, results

    for name, answer in [("combined", 47), ("large", 17), ("cleanup_failure", 321)]:
        shutil.copyfile(repository / "examples/advanced" / (name + ".void"), source)
        run(answer)
        run(answer, "hit" if supported else "unavailable")
    shutil.copyfile(repository / "examples/patterns/nullable.void", source)
    ordinary = subprocess.run([str(compiler), "run", str(source), "7", "--no-opt"], capture_output=True, text=True)
    assert ordinary.returncode == 0 and ordinary.stdout.strip() == "7" and "warning:" in ordinary.stderr, ordinary
    for _ in range(2):
        cached = run(7)
        warnings = "\n".join(line for line in cached.stderr.splitlines() if not line.startswith("Debug cache:")) + "\n"
        assert warnings == ordinary.stderr, (ordinary, cached)
    if os.name != "nt":
        # A killed first writer may leave an unpublished credential stage.
        # A new process creates a complete canonical key and resumes warm hits.
        credential = root / '.void/cache' / f'.authentication-{os.geteuid()}'
        # An unsuitable local credential never imposes chmod repair on users.
        # Persistence can use an automatically protected user-cache credential.
        credential.chmod(0o644)
        fallback_env = dict(os.environ, XDG_CACHE_HOME=str(root / 'user-cache'),
                            HOME=str(root / 'user-cache-home'))
        run(7, "miss", env=fallback_env)
        run(7, "hit", env=fallback_env)
        assert credential.stat().st_mode & 0o777 == 0o644
        credential.unlink()
        interrupted_key = credential.parent / '.authentication-stage-interrupted'
        interrupted_key.write_bytes(b'partial')
        interrupted_key.chmod(0o600)
        run(7, "miss")
        run(7, "hit")
        assert len(credential.read_bytes()) == 32
        assert interrupted_key.read_bytes() == b'partial'
        shutil.rmtree(root / ".void")
        (root / ".void").symlink_to(root / "unsafe")
        run(7, "unavailable")
print("Fresh processes, corruption, concurrent publication, imports, diagnostics and large arrays passed")
