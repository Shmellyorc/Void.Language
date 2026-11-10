"""D3 acquisition and offline compilation from real disposable Git repositories."""
from concurrent.futures import ThreadPoolExecutor
from hashlib import sha256
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import tomllib

compiler, git, clang = (Path(arg).resolve() for arg in sys.argv[1:])
manifest = "void.project.toml"
store = ".void/git-deps"
units_origin = "file:origins/units"
shapes_origin = "file:origins/shapes repo"


def run(*argv, status=0, message=None, env=None, input=None, cwd=None):
    command = list(map(str, argv))
    result = subprocess.run(command, capture_output=True, timeout=45, env=env, input=input, cwd=cwd)
    assert result.returncode == status, (command, result.returncode, result.stdout, result.stderr)
    if message:
        assert message.encode() in result.stderr, (command, result.stderr)
    return result.stdout


def cli(root, *command, **options):
    return run(compiler, *command, "--project", root, **options)


def write(root, relative, text):
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(text.encode())
    return path


def git_run(repo, *args, **kwargs):
    return run(git, "-C", repo, *args, env=git_env, **kwargs)


def initialize(repo):
    repo.mkdir(parents=True)
    git_run(repo, "init", "--quiet", "--object-format=sha1")


def commit(repo):
    git_run(repo, "add", ".")
    git_run(repo, "-c", "user.name=D3 Fixture", "-c", "user.email=d3@example.invalid",
            "-c", "commit.gpgsign=false", "commit", "--quiet", "--no-verify", "-m", "D3 fixture")
    return git_run(repo, "rev-parse", "HEAD").decode().strip()


def edge(alias, origin, rev, subdir=None, identity=None, version="=1.0.0"):
    return (f'\n[dependencies.{alias}]\nid="{identity or "org.example." + alias}"\n'
            f'git="{origin}"\nrev="{rev}"\nversion="{version}"\n'
            + (f'subdir="{subdir}"\n' if subdir is not None else ""))


def policy(name, dependencies=(), library=False):
    kind = "library" if library else "application"
    target = (f'[package]\nid="org.example.{name}"\nname="{name}"\nentry="src/api.void"\n'
              if library else f'[application]\nid="org.example.{name}.app"\n[sources]\nroots=["src"]\n')
    return (f'schema=1\n[project]\nid="org.example.{name}.project"\nname="{name}"\n'
            f'version="1.0.0"\nkind="{kind}"\n' + target + "".join(dependencies))


def library(repo, subdir, name, source, dependencies=()):
    package = repo / subdir
    write(package, manifest, policy(name, dependencies, library=True))
    return write(package, "src/api.void", source)


def frame(value):
    return len(value).to_bytes(8, "big") + value


def tree_digest(files):
    digest = sha256(frame(b"void-package-tree-v1"))
    for path, data in sorted(files.items()):
        digest.update(frame(b"file") + frame(path.encode()) + frame(data))
    return digest.hexdigest()


def interrupted(base, root, allow):
    if not sys.platform.startswith("linux"):
        return
    recovery = base / "interrupted project"
    shutil.copytree(root, recovery, ignore=shutil.ignore_patterns(".void"))
    hook = base / "publication-hook.so"
    run(clang, "-shared", "-fPIC", "-Wall", "-Wextra", Path(__file__).with_name("git_snapshot_fault.c"), "-ldl", "-o", hook)
    marker = base / "publication-waiting"
    previous = (recovery / "void.lock").read_bytes()
    command = [str(compiler), "deps", "fetch", "--project", str(recovery), *allow]
    # The loader splits LD_PRELOAD on spaces even when the filename is quoted.
    with hook.open("rb") as library:
        fd = library.fileno()
        env = dict(os.environ, LD_PRELOAD=f"/proc/self/fd/{fd}", VOID_TEST_GIT_MARKER=str(marker))
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env, pass_fds=(fd,))
    try:
        deadline = time.monotonic() + 25
        while not marker.exists() and process.poll() is None and time.monotonic() < deadline:
            time.sleep(0.01)
        assert marker.exists(), (process.poll(), process.communicate(timeout=1) if process.poll() is not None else None)
    finally:
        if process.poll() is None:
            process.kill()
        process.communicate(timeout=10)
    entries = list((recovery / store).iterdir())
    assert len(entries) == 1 and entries[0].name.startswith("acquire-"), entries
    cli(recovery, "check", status=1, message="Missing Git dependency snapshot")
    assert (recovery / "void.lock").read_bytes() == previous
    cli(recovery, "deps", "fetch", *allow)
    cli(recovery, "resolve", "--locked")
    assert cli(recovery, "run", "--no-cache") == b"13\n"


def negative(base):
    root = base / "negative"
    origin = root / "origins/repository"
    initialize(origin)
    source = library(origin, ".", "units", "public func next() -> i32:\n    return 1\n")
    valid = commit(origin)
    write(root, "src/app.void", "func main():\n    print(1)\n")
    locator = "file:origins/repository"

    def selection(rev, **options):
        write(root, manifest, policy("app", [edge("units", locator, rev, **options)]))

    for origin_value, rev, subdir in [
        ("ssh://example.invalid/repo", valid, None), ("https://user:pass@example.invalid/repo", valid, None),
        ("file:../outside", valid, None), (locator, "main", None), (locator, valid, "../outside"),
        (locator, valid.upper(), None),
    ]:
        write(root, manifest, policy("app", [edge("units", origin_value, rev, subdir)]))
        cli(root, "deps", "fetch", status=1)
    selection("f" * 40)
    cli(root, "deps", "fetch", status=1, message="Pinned Git object is unavailable")
    blob = git_run(origin, "hash-object", str(source)).decode().strip()
    selection(blob)
    cli(root, "deps", "fetch", status=1, message="not a commit")
    selection(valid, identity="org.example.wrong")
    cli(root, "deps", "fetch", status=1, message="Wrong dependency package ID")
    selection(valid, version="=9.0.0")
    cli(root, "deps", "fetch", status=1, message="version conflict")
    selection(valid, subdir="src")
    cli(root, "deps", "fetch", status=1, message="Missing exact void.project.toml")
    write(root, manifest, policy("app", [edge("units", "https://example.invalid/units.git", valid)]))
    cli(root, "deps", "fetch", status=1, message="HTTPS Git acquisition is unavailable")
    assert not (root / store).exists()

    for mode, oid in [("120000", blob), ("160000", valid)]:
        git_run(origin, "update-index", "--add", "--cacheinfo", f"{mode},{oid},unsupported")
        git_run(origin, "-c", "user.name=D3 Fixture", "-c", "user.email=d3@example.invalid",
                "-c", "commit.gpgsign=false", "commit", "--quiet", "-m", "unsupported entry")
        selection(git_run(origin, "rev-parse", "HEAD").decode().strip())
        cli(root, "deps", "fetch", status=1, message="unsupported link, submodule or special entry")
        git_run(origin, "reset", "--hard", valid)
    write(origin, "src/bad:name.void" if os.name != "nt" else "src/bad name.void", "ignored\n")
    if os.name != "nt":
        selection(commit(origin))
        cli(root, "deps", "fetch", status=1, message="Invalid Git source path")
    git_run(origin, "reset", "--hard", valid)
    write(origin, manifest, policy("units", [
        '\n[dependencies.escape]\nid="org.example.escape"\npath="../elsewhere"\nversion="=1.0.0"\n'
    ], library=True))
    selection(commit(origin))
    cli(root, "deps", "fetch", status=1, message="escapes the selected project enclosure")
    git_run(origin, "reset", "--hard", valid)
    write(origin, ".git/objects/info/alternates", str(base / "foreign") + "\n")
    selection(valid)
    cli(root, "deps", "fetch", status=1, message="alternate")
    assert not (root / store).exists()


with tempfile.TemporaryDirectory(prefix="VOID D3 雪 ") as temp:
    base = Path(temp).resolve()
    git_env = {key: value for key, value in os.environ.items() if not key.startswith("GIT_")}
    git_env.update(GIT_CONFIG_NOSYSTEM="1", GIT_CONFIG_GLOBAL=str(base / "empty-git-config"))
    root = base / "project"
    units_repo = root / "origins/units"
    initialize(units_repo)
    units_source = library(units_repo, ".", "units",
        "var counter = 4\npublic func next() -> i32:\n    counter += 1\n    return counter\n")
    units_rev = commit(units_repo)
    git_run(units_repo, "gc", "--quiet", "--prune=now")

    shapes = root / "origins/shapes repo"
    initialize(shapes)
    transitive = edge("units", units_origin, units_rev)
    geometry_source = library(shapes, "packages/geometry", "geometry",
        'use pkg/units\nuse pkg/helper\npublic func answer() -> i32:\n    return units.next() + helper.adjust()\n'
        'internal func secret() -> i32:\n    return 8\n', [transitive,
        '\n[dependencies.helper]\nid="org.example.helper"\npath="../helper"\nversion="=1.0.0"\n'])
    library(shapes, "packages/algebra", "algebra",
        "use pkg/units\npublic func answer() -> i32:\n    return units.next()\n", [transitive])
    library(shapes, "packages/helper", "helper", "public func adjust() -> i32:\n    return 2\n")
    write(shapes, ".gitattributes", "packages/geometry/src/api.void export-ignore\n")
    rev = commit(shapes)
    geometry_source.write_text("the branch and worktree no longer contain the pinned bytes\n")
    assert commit(shapes) != rev
    config = shapes / ".git/config"
    original_config = config.read_bytes()
    config.write_text("[invalid Git config\n")
    library(root, "deps/plain", "plain", "public func answer() -> i32:\n    return 1\n")

    def root_manifest(revision):
        return policy("app", [edge("geometry", shapes_origin, revision, "packages/geometry"),
                              edge("algebra", shapes_origin, revision, "packages/algebra"),
                              '\n[dependencies.plain]\nid="org.example.plain"\npath="deps/plain"\nversion="=1.0.0"\n'])

    selected = write(root, manifest, root_manifest(rev))
    program = "use pkg/geometry\nuse pkg/algebra\nfunc main():\n    print(geometry.answer() + algebra.answer())\n"
    main = write(root, "src/app.void", program)
    allow = ("--allow-git", units_origin, units_rev)
    cli(root, "resolve", status=1, message="Missing Git dependency snapshot")
    cli(root, "deps", "fetch", status=1, message="Transitive Git pin requires explicit acquisition authorization")
    assert not (root / store).exists() and not (root / "void.lock").exists()
    hostile = dict(os.environ, GIT_DIR=str(base / "foreign"), GIT_OBJECT_DIRECTORY=str(base / "foreign"),
                   GIT_CONFIG_COUNT="1", GIT_CONFIG_KEY_0="core.repositoryformatversion", GIT_CONFIG_VALUE_0="999")
    try:
        with ThreadPoolExecutor(max_workers=4) as pool:
            results = list(pool.map(lambda _: cli(root, "deps", "fetch", *allow, env=hostile), range(4)))
    finally:
        config.write_bytes(original_config)
    assert all(b"Verified Git source snapshots: 2" in result for result in results)
    snapshots = sorted((root / store).iterdir())
    assert len(snapshots) == 2 and all(len(path.name) == 64 for path in snapshots)
    cli(root, "resolve")
    lock = root / "void.lock"
    locked = lock.read_bytes()
    decoded = tomllib.loads(locked.decode())
    assert decoded["format"] == decoded["resolver"] == 2
    rows = {row["id"]: row for row in decoded["package"]}
    assert len(rows) == 5 and rows["org.example.helper"]["subdir"] == "packages/helper"
    assert rows["org.example.plain"]["path"] == "deps/plain" and "git" not in rows["org.example.plain"]
    assert rows["org.example.units"]["rev"] == units_rev
    assert rows["org.example.units"]["tree"] == tree_digest(
        {manifest: (units_repo / manifest).read_bytes(), "src/api.void": units_source.read_bytes()})
    assert os.fsencode(root) not in locked and b".void/git-deps" not in locked
    interrupted(base, root, allow)

    # Offline reuse ignores dirty origin files and needs neither origin checkout.
    geometry_source.write_text("dirty checkout bytes\n")
    cli(root, "deps", "fetch")
    assert sorted((root / store).iterdir()) == snapshots and lock.read_bytes() == locked
    origins = root / "origins"
    parked = base / "unavailable origins"
    origins.rename(parked)
    cli(root, "deps", "fetch")
    cli(root, "resolve", "--locked")
    for profile in ("debug", "release"):
        assert cli(root, "run", "--profile", profile, "--no-cache") == b"13\n"
        executable = base / (profile + (".exe" if os.name == "nt" else ""))
        cli(root, "build", "--profile", profile, "-o", executable)
        assert run(executable, cwd=base) == b"13\n"
    parked_project = base / "unavailable project"
    root.rename(parked_project)
    try:
        assert run(executable, cwd=base) == b"13\n"
    finally:
        parked_project.rename(root)

    moved = base / "relocated project"
    shutil.copytree(root, moved)
    cli(moved, "resolve", "--locked")
    assert (moved / "void.lock").read_bytes() == locked
    assert cli(moved, "run", "--no-cache") == b"13\n"
    for text, message in [
        (program.replace("pkg/geometry", "pkg/units"), "Undeclared direct package import"),
        (program.replace("geometry.answer()", "geometry.secret()"), "secret"),
    ]:
        main.write_text(text)
        cli(root, "check", status=1, message=message)
    exported, = (root / store).glob("*/*/tree/packages/geometry/src/api.void")
    main.write_text(program.replace("use pkg/geometry", f'use "/{exported.relative_to(root).as_posix()}" as geometry'))
    cli(root, "check", status=1, message="Git dependency store is reserved")
    main.write_text(program)

    bad_snapshot = snapshots[0]
    parked_snapshot = base / "missing snapshot"
    bad_snapshot.rename(parked_snapshot)
    cli(root, "check", status=1, message="Missing Git dependency snapshot")
    parked_snapshot.rename(bad_snapshot)
    blob = next(bad_snapshot.glob("*/tree/**/api.void"))
    original = blob.read_bytes()
    blob.write_bytes(original + b"# corrupted snapshot\n")
    for command in [("check",), ("resolve", "--refresh"), ("deps", "fetch")]:
        cli(root, *command, status=1, message="Git object identity mismatch")
    assert lock.read_bytes() == locked
    blob.write_bytes(original)
    proof = next(bad_snapshot.glob("*/proof"))
    proof_bytes = proof.read_bytes()
    header_end = proof_bytes.index(b"\n") + 1
    proof.write_bytes(proof_bytes[:header_end] + b"X" + proof_bytes[header_end + 1:])
    cli(root, "resolve", "--refresh", status=1, message="Git object identity mismatch")
    proof.write_bytes(proof_bytes)
    extra = write(blob.parent, "unexpected.txt", "unexpected source\n")
    cli(root, "check", status=1, message="unexpected files")
    extra.unlink()
    cli(root, "resolve", "--locked")

    first = subprocess.run([str(compiler), "run", "--cache", "--project", str(root)], capture_output=True, timeout=45)
    assert first.returncode == 0 and first.stdout == b"13\n", first
    if os.name == "posix":
        assert b"Debug cache: miss" in first.stderr
        reused = subprocess.run([str(compiler), "run", "--cache", "--project", str(root)], capture_output=True, timeout=45)
        assert reused.returncode == 0 and reused.stdout == b"13\n" and b"Debug cache: hit" in reused.stderr, reused
    parked.rename(origins)
    geometry_source.write_bytes(git_run(shapes, "show", f"{rev}:packages/geometry/src/api.void"))
    geometry_source.write_text(geometry_source.read_text().replace("helper.adjust()", "helper.adjust() + 2"))
    new_rev = commit(shapes)
    selected.write_text(root_manifest(new_rev) +
                        edge("older", shapes_origin, rev, "packages/geometry", identity="org.example.geometry"))
    cli(root, "deps", "fetch", status=1, message="origin conflict")
    selected.write_text(root_manifest(new_rev))
    cli(root, "check", status=1, message="Missing Git dependency snapshot")
    cli(root, "deps", "fetch")
    cli(root, "check", status=1, message="Stale or tampered void.lock")
    cli(root, "resolve", "--refresh")
    changed = subprocess.run([str(compiler), "run", "--cache", "--project", str(root)], capture_output=True, timeout=45)
    assert changed.returncode == 0 and changed.stdout == b"15\n", changed
    if os.name == "posix":
        assert b"Debug cache: miss" in changed.stderr
    negative(base)

print("D3 pinned Git acquisition, publication and offline package compilation passed")
