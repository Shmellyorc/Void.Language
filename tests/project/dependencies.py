"""Local graph, independently computed lock digests and publication controls."""
from concurrent.futures import ThreadPoolExecutor
from hashlib import sha256
from pathlib import Path
import os
import subprocess
import sys
import tempfile
import time
import tomllib

compiler = Path(sys.argv[1]).resolve()
manifest_name = "void.project.toml"
source = b"public func answer() -> i32:\n    return 42\n"
# Captured from the D1 diamond fixture with the pinned Linux x64 compiler.
linux_diamond_lock_sha256 = "36d360ad1cf886ac3bac74468b7583fb67035e3c59093eb846a52761ae3a9662"


def invoke(root, *options, status=0, message=None, env=None, command="resolve"):
    argv = [str(compiler), command, "--project", str(root), *options]
    result = subprocess.run(argv, capture_output=True, env=env, timeout=40)
    assert result.returncode == status, (argv, result.returncode, result.stdout, result.stderr)
    if message is not None:
        assert message.encode() in (result.stdout if status == 0 else result.stderr), result
    return result


def write(root, name, data):
    path = root / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    return path


def dependency(alias, path, version=">=1.4.0, <2.0.0", identity=None):
    return (f'\n[dependencies.{alias}]\nid = "{identity or "org.example." + alias}"\n'
            f'path = "{path}"\nversion = "{version}"\n')


def manifest(name, *edges, library=False, entry="src/api.void", version="1.4.2", package_version=None):
    kind = "library" if library else "application"
    text = (f'schema = 1\n[project]\nid = "org.example.{name}.project"\nname = "{name}"\n'
            f'version = "{version}"\nkind = "{kind}"\n')
    if library:
        text += f'\n[package]\nid = "org.example.{name}"\nname = "{name}"\n'
        if package_version:
            text += f'version = "{package_version}"\n'
        if entry:
            text += f'entry = "{entry}"\n'
    else:
        text += f'\n[application]\nid = "org.example.{name}.app"\n'
    return (text + "".join(edges)).encode()


def library(root, path, name, *edges, **settings):
    directory = root / path
    write(directory, manifest_name, manifest(name, *edges, library=True, **settings))
    write(directory, "src/api.void", source)
    return directory


def frame(value):
    return len(value).to_bytes(8, "big") + value


def tree_digest(files, boundaries):
    records = {name: (b"file", data) for name, data in files.items()}
    records.update({name: (b"boundary", None) for name in boundaries})
    digest = sha256(frame(b"void-package-tree-v1"))
    for name, (kind, data) in sorted(records.items()):
        digest.update(frame(kind) + frame(name.encode("ascii")))
        if data is not None:
            digest.update(frame(data))
    return digest.hexdigest()


def read_lock(root):
    return (root / "void.lock").read_bytes()


def diamond(root, reverse):
    edge_a = dependency("algebra", "deps/algebra", "=1.4.2+local")
    edge_b = dependency("geometry", "deps/geometry", "<2.0.0, >=1.4.0")
    order = [edge_b, edge_a] if reverse else [edge_a, edge_b]
    write(root, manifest_name, manifest("demo", *order))
    write(root, "main.void", b"func main() -> i32:\n    return 0\n")
    specs = [
        ("algebra", [dependency("units", "../units")], {"package_version": "1.4.2+local"}),
        ("geometry", [dependency("units", "../units", "=1.4.2")], {}),
        ("units", [], {}),
    ]
    for name, edges, settings in reversed(specs) if reverse else specs:
        package = library(root, f"deps/{name}", name, *edges, **settings)
        extra = [
            ("a/file", b"\x00\x01\n"),
            ("a.txt", b"letters\r\n"),
            ("nested/" + manifest_name, b"not valid TOML and never parsed"),
            ("nested/data", b"not part of this package"),
            (".git/config", b"ignored"),
            (".void/ignored", b"ignored"),
            ("build/ignored", b"ignored"),
            ("void.lock", b"a consuming project ignores this lock"),
        ]
        for path, data in reversed(extra) if reverse else extra:
            write(package, path, data)
        if reverse:
            os.utime(package / "src/api.void", (1000000, 1000000))
            (package / "empty").mkdir()


def test_diamond(base):
    root, moved = base / "original", base / "relocated with spaces 雪"
    diamond(root, False)
    diamond(moved, True)
    invoke(root, "--locked", status=1, message="Missing void.lock")
    assert not (root / ".void").exists()
    invoke(root, message="Created void.lock")
    first = read_lock(root)
    digest = sha256(first).hexdigest()
    assert digest == linux_diamond_lock_sha256, (linux_diamond_lock_sha256, digest)
    print(f"D1 diamond lock SHA-256 matches Linux: {digest}")
    assert str(root).encode() not in first and b"\r" not in first
    lock = tomllib.loads(first.decode())
    assert lock["format"] == lock["resolver"] == 1
    assert lock["root"]["id"] == "org.example.demo.project"
    assert lock["root"]["dependencies"] == {"algebra": "org.example.algebra", "geometry": "org.example.geometry"}
    inputs = (b"void-dependency-inputs-v1", b"org.example.demo.project", b"application",
              b"algebra", b"org.example.algebra", b"deps/algebra", b"=1.4.2+local",
              b"geometry", b"org.example.geometry", b"deps/geometry", b">=1.4.0,<2.0.0")
    assert lock["root"]["inputs"] == sha256(b"".join(map(frame, inputs))).hexdigest()
    assert [item["id"] for item in lock["package"]] == [f"org.example.{name}" for name in ("algebra", "geometry", "units")]
    for item in lock["package"]:
        package = root / item["path"]
        manifest_bytes = (package / manifest_name).read_bytes()
        assert item["manifest"] == sha256(manifest_bytes).hexdigest()
        assert item["tree"] == tree_digest({
            manifest_name: manifest_bytes, "src/api.void": source,
            "a/file": b"\x00\x01\n", "a.txt": b"letters\r\n",
        }, ["nested"])
        assert item["dependencies"] == ({} if package.name == "units" else {"units": "org.example.units"})
        assert item["version"] == ("1.4.2+local" if package.name == "algebra" else "1.4.2")

    stamp = (root / "void.lock").stat().st_mtime_ns
    invoke(root, message="Verified void.lock")
    invoke(root, "--locked", message="Verified void.lock")
    assert (root / "void.lock").stat().st_mtime_ns == stamp
    invoke(moved)
    assert read_lock(moved) == first
    write(root, "deps/geometry/nested/data", b"changed outside the parent package boundary")
    invoke(root, "--locked")
    write(root, "deps/geometry/src/api.void", source + b"# changed\n")
    invoke(root, "--locked", status=1, message="Stale or tampered")
    invoke(root, status=1, message="--refresh")
    assert read_lock(root) == first
    invoke(root, "--refresh", message="Refreshed void.lock")
    refreshed = read_lock(root)
    assert refreshed != first
    assert tomllib.loads(refreshed.decode())["root"] == lock["root"]
    invoke(root, "--refresh", message="Verified void.lock")
    assert read_lock(root) == refreshed
    write(root, "void.lock", refreshed.replace(b'units = "org.example.units"\n', b"", 1))
    invoke(root, "--locked", status=1, message="Stale or tampered")
    invoke(root, "--refresh")
    write(root, "void.lock", refreshed.replace(b'path = "deps/algebra"', b'path = "/outside"'))
    invoke(root, "--locked", status=1, message="Malformed void.lock")
    write(root, "void.lock", refreshed.replace(b'id = "org.example.geometry"', b'id = "org.example.algebra"'))
    invoke(root, "--locked", status=1, message="Conflicting package IDs")
    write(root, "void.lock", b"format = [\n")
    invoke(root, "--refresh", status=1, message="Malformed void.lock")
    assert read_lock(root) == b"format = [\n"
    write(root, "void.lock", refreshed.replace(b"resolver = 1", b"resolver = 2"))
    invoke(root, "--refresh", status=1, message="Unsupported void.lock")
    write(root, "void.lock", refreshed)
    if os.name == "posix":
        (root / "void.lock").unlink()
        outside = write(base, "outside.lock", refreshed)
        (root / "void.lock").symlink_to(outside)
        invoke(root, "--refresh", status=1, message="without following aliases")
        assert outside.read_bytes() == refreshed
        (root / "void.lock").unlink()
        write(root, "void.lock", refreshed)
    invoke(root, command="check")
    subprocess.run([str(compiler), "check", str(root / "main.void")], check=True, capture_output=True, timeout=40)
    return root


def test_negative(base):
    root = base / "negatives"
    a = library(root, "deps/algebra", "algebra")
    library(root, "deps/other", "algebra")
    library(root, "deps/geometry", "geometry", dependency("algebra", "../algebra", ">=1.9.0,<2.0.0"))
    write(root, "deps/app/" + manifest_name, manifest("demo"))
    nested = library(a, "nested", "units")
    library(root, "deps/noentry", "units", entry=None)
    bad_entry = library(root, "deps/badentry", "units", entry="nested/src/api.void")
    library(bad_entry, "nested", "geometry")
    for edge, diagnostic in [
        (dependency("algebra", "deps/other", identity="org.example.other"), "Wrong dependency package ID"),
        (dependency("algebra", "deps/algebra", "=1.4.2+wrong"), "version conflict"),
        (dependency("algebra", "deps/app"), "library project"),
        (dependency("algebra", "deps/Algebra"), "wrong case"),
        (dependency("algebra", "../outside"), "escapes"),
        (dependency("units", "deps/algebra/nested"), "crosses nested project boundary"),
        (dependency("units", "deps/noentry"), "requires package.entry"),
        (dependency("units", "deps/badentry"), "Package entry is unavailable"),
        (dependency("algebra", "deps/BUILD/algebra"), "Invalid local dependency path"),
        (dependency("algebra", "deps/algebra", ">=2.0.0,<1.0.0"), "Invalid dependency version constraint"),
    ]:
        write(root, manifest_name, manifest("demo", edge))
        invoke(root, status=1, message=diagnostic)
        assert not (root / "void.lock").exists()
    write(root, manifest_name, manifest("demo", dependency("algebra", "deps/algebra"),
                                      dependency("geometry", "deps/geometry")))
    conflict = invoke(root, status=1, message="version conflict").stderr
    assert b"geometry (org.example.geometry) -> algebra (org.example.algebra)" in conflict
    assert b"First selection via algebra" in conflict
    write(root, manifest_name, manifest("demo", dependency("algebra", "deps/algebra"),
                                      dependency("second", "deps/other", identity="org.example.algebra")))
    invoke(root, status=1, message="origin conflict")

    write(root, manifest_name, manifest("demo", dependency("algebra", "deps/algebra")))
    write(a, manifest_name, manifest("algebra", dependency("geometry", "../geometry"), library=True))
    write(root, "deps/geometry/" + manifest_name,
          manifest("geometry", dependency("algebra", "../algebra"), library=True))
    cycle = invoke(root, status=1, message="Dependency cycle").stderr
    assert b"algebra (org.example.algebra) -> geometry (org.example.geometry) -> algebra" in cycle

    write(a, manifest_name, manifest("algebra", library=True))
    write(a, "src/extra", b"data")
    os.link(a / "src/extra", a / "src/alias")
    invoke(root, status=1, message="without aliases")
    (a / "src/alias").unlink()
    alias = root / "deps/alias"
    if os.name == "nt":
        import _winapi
        _winapi.CreateJunction(str(a), str(alias))
    else:
        alias.symlink_to(a, target_is_directory=True)
    try:
        write(root, manifest_name, manifest("demo", dependency("algebra", "deps/alias")))
        invoke(root, status=1, message="filesystem aliases")
    finally:
        if os.name == "nt":
            alias.rmdir()
        else:
            alias.unlink()
    write(root, manifest_name, manifest("demo", dependency("algebra", "deps/algebra")))
    if os.name == "posix":
        (a / "src/alias").symlink_to(nested / "src/api.void")
        invoke(root, status=1, message="filesystem aliases")
        (a / "src/alias").unlink()
    with (a / "src/extra").open("wb") as oversized:
        oversized.truncate(64 * 1024 * 1024 + 1)
    invoke(root, status=1, message="byte limit")
    (a / "src/extra").unlink()
    write(a, manifest_name, manifest("algebra", library=True, version="1.4.2-rc.1"))
    invoke(root, status=1, message="version conflict")
    write(root, manifest_name, manifest("demo", dependency("algebra", "deps/algebra", "=1.4.2-rc.1")))
    invoke(root, message="Created void.lock")


def test_depth(base):
    root = base / "depth"
    write(root, manifest_name, manifest("demo", dependency("node0", "deps/node0")))
    for index in range(65):
        edges = [dependency(f"node{index + 1}", f"../node{index + 1}")] if index < 64 else []
        library(root, f"deps/node{index}", f"node{index}", *edges)
    invoke(root, status=1, message="graph depth limit")
    assert not (root / "void.lock").exists()


def test_file_limit(base):
    root = base / "file-limit"
    write(root, manifest_name, manifest("demo", dependency("geometry", "deps/geometry")))
    package = library(root, "deps/geometry", "geometry")
    files = package / "many"
    files.mkdir()
    for index in range(8192):
        (files / f"file{index}").touch()
    invoke(root, status=1, message="file and boundary limit")


def test_concurrent(base):
    root = base / "parallel"
    diamond(root, False)

    def resolve(_):
        return invoke(root).stdout

    with ThreadPoolExecutor(max_workers=4) as pool:
        results = list(pool.map(resolve, range(8)))
    assert results.count(b"Created void.lock.\n") == 1 or results.count(b"Created void.lock.\r\n") == 1, results
    before = read_lock(root)
    invoke(root, "--locked")
    assert read_lock(root) == before


def test_interrupted(root, base, clang):
    before = read_lock(root)
    write(root, "deps/geometry/src/api.void", source + b"# refresh after interruption\n")
    hook = base / "dependency_lock_fault.so"
    subprocess.run([clang, "-shared", "-fPIC", "-Wall", "-Wextra",
                    str(Path(__file__).with_name("dependency_lock_fault.c")), "-o", str(hook)], check=True, timeout=40)
    env = dict(os.environ, LD_PRELOAD=str(hook), VOID_TEST_FAIL_LOCK_RENAME="1")
    invoke(root, "--refresh", status=1, message="Cannot replace project dependency lock", env=env)
    assert read_lock(root) == before
    assert not list((root / ".void").glob("resolve-*"))
    marker = base / "rename-waiting"
    env = dict(os.environ, LD_PRELOAD=str(hook), VOID_TEST_LOCK_MARKER=str(marker))
    process = subprocess.Popen([str(compiler), "resolve", "--project", str(root), "--refresh"],
                               env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        deadline = time.monotonic() + 30
        while not marker.exists() and process.poll() is None and time.monotonic() < deadline:
            time.sleep(0.02)
        assert marker.exists(), "writer did not reach atomic replacement"
        assert read_lock(root) == before
    finally:
        if process.poll() is None:
            process.kill()
        stdout, stderr = process.communicate(timeout=10)
    assert process.returncode == -9, (stdout, stderr)
    assert read_lock(root) == before
    invoke(root, "--refresh", message="Refreshed void.lock")
    assert read_lock(root) != before
    invoke(root, "--locked")


with tempfile.TemporaryDirectory(prefix="void-project-dependencies-") as folder:
    base = Path(folder).resolve()
    plain = base / "plain"
    write(plain, manifest_name, manifest("demo"))
    write(plain, "main.void", b"func main() -> i32:\n    return 0\n")
    invoke(plain, "--locked", message="No project dependencies")
    invoke(plain, command="check", message="Source checked successfully")
    assert not (plain / "void.lock").exists() and not (plain / ".void").exists()
    standalone = library(base, "standalone", "geometry", entry=None)
    invoke(standalone, command="check", message="Library project checked successfully")
    assert not (standalone / "void.lock").exists()
    write(standalone, manifest_name,
          manifest("geometry", dependency("units", "deps/units"), library=True, entry=None))
    library(standalone, "deps/units", "units")
    invoke(standalone, message="Created void.lock")
    assert tomllib.loads(read_lock(standalone).decode())["root"]["kind"] == "library"
    root = test_diamond(base)
    test_negative(base)
    test_depth(base)
    test_file_limit(base)
    test_concurrent(base)
    if sys.platform.startswith("linux"):
        test_interrupted(root, base, sys.argv[2])
