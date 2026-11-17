"""Exercise the bundled raylib workflow from an empty developer project."""
import argparse
from hashlib import sha256
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time
import tomllib

HEADLESS = """use pkg/raylib as rl

func main() -> i32:
    var image = rl.gen_image_color(16, 16, rl.orange()) as rl.image
    defer rl.unload_image(image)
    rl.image_draw_pixel(image, 2, 3, rl.blue())
    var pixel = rl.get_image_color(image, 2, 3)
    if pixel.b != 241:
        return 19
    print("G59 raylib pixel: 241")
    return 0
"""

WINDOW = """use pkg/raylib as rl

func main() -> i32:
    var window = rl.init_window(320, 180, "VOID G59") as rl.window
    if not rl.is_window_ready():
        return 20
    rl.begin_drawing()
    rl.clear_background(rl.skyblue())
    rl.end_drawing()
    if rl.close_window(window) != 0:
        return 21
    print("G59 raylib window opened, drew a frame and closed")
    return 0
"""

def workspace(args, root):
    env = dict(os.environ)
    env.pop("VOID_NATIVE_CATALOG", None)
    env.pop("VOID_STDLIB_ROOT", None)
    project = root / "TestGame"
    compiler = args.compiler
    windows = sys.platform == "win32"
    storage_key = "LOCALAPPDATA" if windows else "HOME" if sys.platform == "darwin" else "XDG_DATA_HOME"
    env[storage_key] = str(root / "UserData")

    def run(label, *argv, cwd=project, status=0, contains="", environment=env):
        start = time.perf_counter()
        result = subprocess.run(list(map(str, argv)), cwd=cwd, env=environment,
                                capture_output=True, timeout=180)
        elapsed = time.perf_counter() - start
        output = (result.stdout + result.stderr).decode(errors="replace")
        print(f"{label}: exit {result.returncode}, {elapsed:.3f} s", flush=True)
        assert result.returncode == status and contains in output, (argv, status, output)
        assert not re.search(r"AddressSanitizer|LeakSanitizer|UndefinedBehaviorSanitizer|runtime error:", output), output
        return output

    def cli(label, *argv, **options):
        return run(label, compiler, *argv, **options)

    local = root / "local_library"
    cli("create local source package", "new", local, "--kind", "library", cwd=root)
    cli("register local package", "package", "install", "local_lib", "--dir", local, cwd=root)
    cli("list registered package", "package", "list", cwd=root, contains="Installed: local_lib")
    consumer = root / "LocalConsumer"
    cli("create local consumer", "new", consumer, cwd=root)
    cli("copy registered package into consumer", "add", "local_lib", cwd=consumer)
    cli("unregister local package", "package", "uninstall", "local_lib", cwd=root)
    cli("local consumer lock verifies", "resolve", "--project", ".", "--locked", cwd=consumer)

    cli("new", "new", project, cwd=root)
    discovery = cli("discovery", "package", "list", contains="raylib 0.1.0")
    search = cli("search", "package", "search", "ray", contains="Native raylib 6.0.0 (ABI 3)")
    cli("unlisted package", "package", "search", "unknown", contains="No matching bundled packages")
    manifest = project / "void.project.toml"
    original_manifest = manifest.read_bytes()
    cli("unlisted name has no acquisition", "add", "unknown", status=2, contains="registered or bundled")
    project_path = dict(env, PATH=".")
    cli("project PATH cannot select an interpreter", "add", "raylib", environment=project_path,
        status=1, contains="Python 3.12")
    assert manifest.read_bytes() == original_manifest
    assert not (project / "void.lock").exists() and not (project / "libs").exists()

    prepared = cli("add raylib and auto-prepare", "add", "raylib", contains="Verified release asset SHA-256")
    print(prepared, end="", flush=True)
    installed = project / "libs/raylib"
    catalog = installed / ".void/native"
    lock = project / "void.lock"
    locked = tomllib.loads(lock.read_text(encoding="utf-8"))
    assert locked["package"][0]["id"] == "org.voidlang.raylib"
    assert locked["native"]["id"] == "org.voidlang.raylib" and locked["native"]["abi"] == 3
    receipt = installed / "SOURCE_ORIGIN.txt"
    provenance = dict(line.split(": ", 1) for line in receipt.read_text(encoding="utf-8").splitlines())
    assert provenance["Source origin"] == "https://github.com/Shmellyorc/Void.Language.git"
    assert provenance["Base revision"] == "808d4dce8564cce439213363b2e5f53e238e0a21"
    source_files = {p.relative_to(installed).as_posix(): p for p in installed.rglob("*")
                    if p.is_file() and p != receipt and catalog not in p.parents}
    hashes = "".join(f"{sha256(source_files[name].read_bytes()).hexdigest()}  {name}\n"
                     for name in sorted(source_files))
    snapshot = sha256(hashes.encode()).hexdigest()
    assert provenance["Shipped snapshot SHA-256"] == snapshot
    for result in (discovery, search):
        assert f'Base revision: {provenance["Base revision"]}' in result
        assert f"Shipped snapshot SHA-256: {snapshot}" in result
    assert (installed / "src/raylib_native.void").read_text(encoding="utf-8").splitlines()[1] == 'native "org.voidlang.raylib"'
    entry = project / "src/app.void"
    entry.write_text(HEADLESS, encoding="utf-8")

    cli("execution without consent", "run", ".", status=2, contains="Pass --allow-native")
    cli("build without consent", "build", ".", status=2, contains="Pass --allow-native")
    cli("verify locked catalog", "resolve", "--project", ".", "--locked")
    original_receipt = receipt.read_bytes()
    try:
        receipt.write_bytes(original_receipt + b"\nEdited provenance\n")
        cli("changed receipt invalidates the lock", "resolve", "--project", ".", "--locked",
            status=1, contains="Stale or tampered void.lock")
    finally:
        receipt.write_bytes(original_receipt)
    cli("check catalog", "check", ".")
    cli("tidy with catalog", "tidy", "--check")
    cli("explain locked source", "deps", "explain", "--project", ".", "--json", contains="org.voidlang.raylib")
    cli("local catalog is scoped to the installed package", "check", ".",
        environment=dict(env, VOID_NATIVE_CATALOG=str(root / "missing")))
    cli("installed catalog discovery", "package", "list", contains="Local catalog present")
    backup = catalog.with_name("absent")
    catalog.rename(backup)
    try:
        cli("missing catalog", "run", ".", "--allow-native", status=1, contains="No native catalog selected")
    finally:
        backup.rename(catalog)

    def fingerprints():
        return {p.name: (p.stat().st_mtime_ns, sha256(p.read_bytes()).hexdigest())
                for p in catalog.iterdir()}

    before = fingerprints()
    lock_before = lock.read_bytes(), lock.stat().st_mtime_ns
    print("Approved catalog SHA-256 and timestamps:", before, flush=True)
    repeated = cli("add unchanged raylib", "add", "raylib", contains="reused the prepared catalog")
    assert "Fetching pinned" not in repeated
    for profile in ("debug", "release"):
        for trial in ("first", "repeat"):
            cli(f"{profile} JIT {trial}", "run", ".", "--allow-native",
                "--profile", profile, contains="G59 raylib pixel: 241")
        app = root / profile / ("app.exe" if windows else "app")
        cli(f"{profile} AOT build", "build", ".", "--allow-native", "--profile", profile, "-o", app)
        if windows:
            # Use the compiler's configured SDK rather than a PATH-selected reader.
            driver = Path((compiler.parent / "void-native.txt").read_text(encoding="utf-8").splitlines()[1])
            readobj = driver.with_name("llvm-readobj.exe")
            run(f"{profile} AOT stack", readobj, "--file-headers", app, contains="SizeOfStackReserve: 1048576")
        run(f"{profile} AOT execution", app, contains="G59 raylib pixel: 241")
    assert before == fingerprints()
    assert lock_before == (lock.read_bytes(), lock.stat().st_mtime_ns)

    descriptor, = catalog.glob("*.descriptor")
    adapter = catalog / ("void_raylib.dll" if windows else "libvoid_raylib.so")
    for artifact in (descriptor, adapter):
        original = artifact.read_bytes()
        try:
            artifact.write_bytes(original[:-1] + bytes([original[-1] ^ 1]))
            cli(f"tampered {artifact.name}", "run", ".", "--allow-native",
                status=1, contains="artifact identity mismatch")
        finally:
            artifact.write_bytes(original)

    native = installed / "native/raylib_api.c"
    original = native.read_bytes()
    try:
        native.write_bytes(original + b"\n/* G59 deliberate native input change. */\n")
        cli("changed native source invalidates the lock", "check", ".", status=1, contains="Stale or tampered void.lock")
    finally:
        native.write_bytes(original)
    cli("run after exact restoration", "run", ".", "--allow-native", contains="G59 raylib pixel: 241")

    saved = root / "installed raylib backup"
    installed.rename(saved)
    try:
        cli("restore from original manifest and lock", "add", "raylib", contains="Installed and locked bundled raylib")
        assert lock_before == (lock.read_bytes(), lock.stat().st_mtime_ns)
        assert {n: content[1] for n, content in before.items()} == {n: content[1] for n, content in fingerprints().items()}
        cli("verify restored lock", "resolve", "--project", ".", "--locked")
    finally:
        # The test workspace is disposable. Retain a backup for a failed restore.
        if not installed.exists():
            saved.rename(installed)

    if args.window:
        entry.write_text(WINDOW, encoding="utf-8")
        try:
            output = cli("display execution", "run", ".", "--allow-native",
                         contains="G59 raylib window opened, drew a frame and closed")
            print(output, end="", flush=True)
        finally:
            entry.write_text(HEADLESS, encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("compiler", type=Path)
    parser.add_argument("--keep", type=Path, help="Use a new directory and retain the consumer for editor tests")
    parser.add_argument("--window", action="store_true")
    parser.add_argument("--scratch", type=Path, help="Existing private directory for the temporary developer project")
    args = parser.parse_args()
    args.compiler = args.compiler.resolve()
    if args.keep:
        args.keep.mkdir()
        workspace(args, args.keep.resolve())
    else:
        with tempfile.TemporaryDirectory(prefix="g59-raylib-", dir=args.scratch) as work:
            workspace(args, Path(work))
    print("G59 local raylib workflow passed", flush=True)


if __name__ == "__main__":
    main()
