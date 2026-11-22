"""Bounded validated history; races, active readers and failed optional writes."""
import concurrent.futures
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile

compiler = Path(sys.argv[1]).resolve()

def limits():
    import resource
    resource.setrlimit(resource.RLIMIT_STACK, (1048576, 1048576))

with tempfile.TemporaryDirectory(prefix="void-cache-retention-") as temporary:
    root = Path(temporary)
    source = root / "main.void"
    directory = root / ".void/cache/v1"

    def run(path=source, value=0, hit=None, fail_write=False, fault=None):
        def restricted():
            limits()
            if fail_write:
                import resource
                resource.setrlimit(resource.RLIMIT_FSIZE, (1, 1))
        environment = dict(os.environ)
        if fault:
            environment.update(LD_PRELOAD=str(shim), VOID_CACHE_FAULT=fault,
                               VOID_CACHE_FAULT_DIRECTORY=str(directory))
        result = subprocess.run([str(compiler), "run", str(path), "--cache"], env=environment,
                                capture_output=True, text=True, timeout=60,
                                preexec_fn=restricted if os.name != "nt" else None)
        assert result.returncode == 0 and result.stdout == f"{value}\n", result
        if hit is not None:
            assert f"Debug cache: {hit}" in result.stderr, result
        if fault:
            assert f"G95 cache syscall fault: {fault}" in result.stderr, result
        return result

    def edit(value):
        source.write_text(f"func entry(value: i32) -> i32:\n    return {value}\n")

    def read(path):
        # Inspection must not itself turn old entries into recent ones.
        with os.fdopen(os.open(path, os.O_RDONLY | getattr(os, "O_NOATIME", 0)), "rb") as file:
            return file.read()

    def entries():
        result = {}
        for path in directory.iterdir():
            if len(path.name) != 64 or path.is_symlink() or not path.is_file():
                continue
            stat = path.stat()
            if stat.st_nlink != 1:
                continue
            data = read(path)
            if (len(data) >= 129 and data[64:128] == path.name.encode()
                    and data[128:129] in (b"0", b"1")
                    and data[:64] == hashlib.sha256(data[64:]).hexdigest().encode()):
                result[path.name] = path
        return result

    edit(0)
    first = run()
    if "Debug cache: unavailable" in first.stderr:
        assert not (root / ".void").exists()
        print("Persistent cache unavailable on this host; safe bypass passed")
        sys.exit(0)
    original = next(iter(entries().values()))
    initial_bytes, initial_mtime = read(original), original.stat().st_mtime_ns
    for value in range(1, 10):
        edit(value); run(value=value, hit="miss")
    assert len(entries()) == 10
    # A warm hit refreshes recency, while immutable bytes and mtime stay intact.
    edit(0); run(hit="hit")
    assert (read(original), original.stat().st_mtime_ns) == (initial_bytes, initial_mtime)
    for value in range(10, 14):
        edit(value); run(value=value, hit="miss")
        assert len(entries()) == 10
    edit(0); run(hit="hit")
    edit(1); run(value=1, hit="miss")  # Oldest unused generation was evicted.
    assert len(entries()) == 10
    # Failed writes must neither evict an existing winner nor leave scratch.
    before = set(entries())
    edit(99); run(value=99, hit="miss", fail_write=True)
    assert set(entries()) == before
    assert not any(p.name.startswith("tmp-") for p in directory.iterdir())
    directory.chmod(0o500)
    try:
        run(value=99, hit="miss")
        assert set(entries()) == before, "failed open evicted validated history"
    finally:
        directory.chmod(0o700)
    if sys.platform.startswith("linux"):
        assert len(sys.argv) == 3, "supply configured C compiler for real syscall fault tests"
        shim = root / "cache-fault.so"
        compiled = subprocess.run([sys.argv[2], "-shared", "-fPIC", str(Path(__file__).with_name("fault.c")),
                                   "-ldl", "-o", str(shim)], capture_output=True, timeout=60)
        assert compiled.returncode == 0, compiled.stderr
        for fault in ("partial", "fsync", "link"):
            run(value=99, hit="miss", fault=fault)
            assert set(entries()) == before, (fault, "failed publication evicted validated history")
            assert not any(p.name.startswith("tmp-") for p in directory.iterdir()), fault
        edit(0)
        for fault in ("noatime", "other-owner"):
            run(value=0, hit="hit", fault=fault)

    # Unknown objects are outside eviction authority, even at key-shaped names.
    sentinels = {directory / "tmp-interrupted": b"partial", directory / ("a" * 64): b"corrupt"}
    for path, data in sentinels.items():
        path.write_bytes(data); path.chmod(0o600)
    unknown = directory / ("b" * 64); unknown.mkdir()
    external = root / "external"; external.write_bytes(b"untouched")
    alias = directory / ("c" * 64); alias.symlink_to(external)
    hardlink = directory / ("d" * 64); os.link(external, hardlink)
    shared = directory / ("e" * 64); shared.write_bytes(initial_bytes); shared.chmod(0o666)
    # An active disk reader retains its open inode while its name is evicted.
    edit(0); run(hit="hit")
    held_path = next(p for p in entries().values() if read(p) == initial_bytes)
    with held_path.open("rb") as reader:
        for value in range(100, 111):
            edit(value); run(value=value, hit="miss")
        assert not held_path.exists() and reader.read() == initial_bytes

    # Competing writers use different identities but the same project store.
    paths = []
    for value in range(20):
        path = root / f"concurrent{value}.void"
        path.write_text(f"func entry(value: i32) -> i32:\n    return {value}\n")
        paths.append((path, value))
    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
        list(pool.map(lambda item: run(item[0], item[1]), paths))
    assert len(entries()) == 10
    for path, data in sentinels.items():
        assert path.read_bytes() == data
    assert unknown.is_dir() and alias.is_symlink() and external.read_bytes() == b"untouched"
    assert hardlink.stat().st_ino == external.stat().st_ino
    assert shared.read_bytes() == initial_bytes and shared.stat().st_mode & 0o077
    assert set(p.name for p in directory.iterdir() if p.name.startswith("tmp-")) == {"tmp-interrupted"}
print("10-entry validated LRU, restoration, failed writes, concurrent eviction and protected contents passed")
