"""D2 locked packages through the CLI, typed IR, Runtime and real voidls."""
from pathlib import Path
import json
import os
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from lsp_client import Client

compiler, server, ir_probe = (Path(arg).resolve() for arg in sys.argv[1:])
manifest = "void.project.toml"
units = """var shared = initialize()
struct box:
    value: i32
func initialize() -> box:
    print("units-init")
    return box(value = 4)
public func initialized() -> bool:
    return shared.value > 0
public func next() -> i32:
    shared.value += 1
    return shared.value
"""
application = """use pkg/geometry as geo
use pkg/algebra
func main():
    print("雪", geo.answer() + algebra.answer())
"""


def write(root, relative, text):
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(text.encode("utf-8"))
    return path


def policy(name, library=False, edges=()):
    text = (f'schema = 1\n[project]\nid = "org.example.{name}.project"\n'
            f'name = "{name}"\nversion = "1.0.0"\nkind = "{"library" if library else "application"}"\n')
    if library:
        text += f'[package]\nid = "org.example.{name}"\nname = "{name}"\nentry = "src/api.void"\n'
    else:
        text += f'[application]\nid = "org.example.{name}.app"\n'
    for alias, path in edges:
        text += f'[dependencies.{alias}]\nid = "org.example.{alias}"\npath = "{path}"\nversion = "=1.0.0"\n'
    return text


def package(root, location, name, source, edges=()):
    directory = root / location
    write(directory, manifest, policy(name, library=True, edges=edges))
    return write(directory, "src/api.void", source)


def run(*args, status=0, message=None, cwd=None):
    result = subprocess.run([str(arg) for arg in args], capture_output=True, timeout=45, cwd=cwd)
    assert result.returncode == status, (args, result.returncode, result.stdout, result.stderr)
    if message:
        assert message in result.stderr.decode(errors="replace"), (args, result.stderr)
    return result


def cli(root, command, *args, **expected):
    return run(compiler, command, "--project", root, *args, **expected)


def rpc(client, method, path, line, character):
    response, _ = client.request(method, {
        "textDocument": {"uri": path.as_uri()},
        "position": {"line": line, "character": character},
    })
    assert "result" in response, response
    return response["result"]


def editor(root, main, geometry):
    def labels(items):
        return {item["label"] for item in (items["items"] if isinstance(items, dict) else items)}

    def update(client, text, version):
        client.notify("textDocument/didChange", {
            "textDocument": {"uri": main.as_uri(), "version": version}, "contentChanges": [{"text": text}],
        })
        return client.sync()

    with Client(server, root, project=True) as client:
        assert not any(n["diagnostics"] for n in client.initialize())
        assert rpc(client, "textDocument/definition", main, 0, 10)["uri"] == geometry.as_uri()
        assert rpc(client, "textDocument/definition", main, 3, 20)["uri"] == geometry.as_uri()
        assert rpc(client, "textDocument/definition", geometry, 0, 10)["uri"] == (root / "deps/units/src/api.void").as_uri()
        assert labels(rpc(client, "textDocument/completion", geometry, 0, 10)) == {"units"}
        client.notify("textDocument/didOpen", {"textDocument": {
            "uri": geometry.as_uri(), "version": 1, "languageId": "void", "text": "dirty package text",
        }})
        assert not any(n["diagnostics"] for n in client.sync())
        assert rpc(client, "textDocument/definition", main, 3, 20)["uri"] == geometry.as_uri()

        client.notify("textDocument/didOpen", {"textDocument": {
            "uri": main.as_uri(), "version": 1, "languageId": "void", "text": application,
        }})
        client.sync()
        diagnostics = update(client, application.replace("pkg/geometry", "pkg/"), 2)
        assert any(n["diagnostics"] for n in diagnostics), diagnostics
        assert labels(rpc(client, "textDocument/completion", main, 0, 8)) == {"geometry", "algebra"}
        assert not any(n["diagnostics"] for n in update(client, application, 3))
        assert rpc(client, "textDocument/definition", main, 3, 20)["uri"] == geometry.as_uri()
        client.shutdown()


with tempfile.TemporaryDirectory(prefix="VOID D2 雪 ") as directory:
    base = Path(directory).resolve()
    root = base / "source project"
    main = write(root, "src/app.void", application)
    write(root, manifest, policy("app", edges=[("geometry", "deps/geometry"), ("algebra", "deps/algebra")]))
    package(root, "deps/units", "units", units)
    geometry_text = """use pkg/units as units
use "./helper shared.void" as local
use lib/tuning as tuning
use math as math
var ready = units.initialized()
public func answer() -> i32:
    if not ready:
        return -1
    return units.next() + local.adjust() + tuning.adjust() + math.abs(0 as i32)
internal func hidden() -> i32:
    return 1
private func secret() -> i32:
    return 2
func main():
    print("not the app")
"""
    geometry = package(root, "deps/geometry", "geometry", geometry_text, [("units", "../units")])
    algebra_text = """use pkg/units as units
use "/src/helper shared.void" as local
public func answer() -> i32:
    return units.next() + local.adjust()
"""
    algebra = package(root, "deps/algebra", "algebra", algebra_text, [("units", "../units")])
    for name, adjustment in [("geometry", 2), ("algebra", 4)]:
        write(root / "deps" / name, "src/helper shared.void", f"internal func adjust() -> i32:\n    return {adjustment}\n")
    write(root / "deps/geometry", "libs/tuning/tuning.void", "internal func adjust() -> i32:\n    return 1\n")

    lock = root / "void.lock"
    cli(root, "check", status=1, message="Missing void.lock")
    assert not lock.exists()
    cli(root, "resolve")
    original = lock.read_bytes()
    stamp = lock.stat().st_mtime_ns
    cli(root, "check")
    cli(root, "check", "--entry", "deps/geometry/src/api.void",
        status=1, message="nested project boundary")
    preflight = run(server, "--discover-project", root, geometry, root, "")
    assert json.loads(preflight.stdout)["status"] == "ready", preflight.stdout
    editor(root, main, geometry)
    expected = "units-init\n雪 18\n".encode()
    for profile in ("debug", "release"):
        assert cli(root, "run", "--profile", profile, "--no-cache").stdout == expected
        native = base / f"package-{profile}{'.exe' if os.name == 'nt' else ''}"
        cli(root, "build", "--profile", profile, "-o", native)
        assert run(native).stdout == expected
    obj = base / "packages.o"
    cli(root, "emit", "-o", obj)
    assert obj.stat().st_size > 0
    first = cli(root, "run", "--cache")
    assert first.stdout == expected
    second = cli(root, "run", "--cache")
    assert second.stdout == expected
    cache_available = b"unavailable" not in first.stderr
    if os.name == "posix":
        assert cache_available, first.stderr
    if cache_available:
        assert b"Debug cache: hit" in second.stderr, second.stderr
    else:
        print("Debug object cache unavailable on this host")
    assert (lock.read_bytes(), lock.stat().st_mtime_ns) == (original, stamp)

    # Relocation preserves the D1 bytes and canonical D2 source identities.
    moved = base / "relocated project"
    shutil.copytree(root, moved, ignore=shutil.ignore_patterns(".void"))
    cli(moved, "resolve", "--locked")
    assert cli(moved, "run", "--no-cache").stdout == expected
    run(ir_probe, root.name, moved.name, cwd=base)

    # The IR probe refreshes this lock after changing the shared source.
    refreshed = lock.read_bytes()
    assert refreshed != original
    after = cli(root, "run", "--cache")
    assert after.stdout == "units-init\n雪 20\n".encode()
    if cache_available:
        assert b"Debug cache: miss" in after.stderr, after.stderr
    assert lock.read_bytes() == refreshed

    for contents, message in [
        (original, "Stale or tampered void.lock"),
        (refreshed.replace(b'org.example.algebra"', b'org.example.fakepkg"', 1), "Stale or tampered void.lock"),
        (b"format = [\n", "Malformed void.lock"),
    ]:
        lock.write_bytes(contents)
        for command in ("check", "run", "build", "emit"):
            cli(root, command, *({"run": ("--no-cache",), "build": ("-o", base / "denied"),
                                  "emit": ("-o", base / "denied.o")}.get(command, ())),
                status=1, message=message)
        assert lock.read_bytes() == contents
        assert not (base / "denied").exists() and not (base / "denied.o").exists()
    lock.write_bytes(refreshed)

    for text, message in [
        (application.replace("pkg/geometry", "pkg/units"), "Undeclared direct package import"),
        (application.replace("geo.answer()", "geo.hidden()"), "hidden"),
        (application.replace("geo.answer()", "geo.secret()"), "secret"),
        (application.replace("pkg/geometry", '"/deps/geometry/src/api.void"'), "nested project boundary"),
        (application.replace("pkg/geometry", '"/@pkg/org.example.geometry/src/api.void"'), "Reserved package source identity"),
    ]:
        main.write_bytes(text.encode())
        cli(root, "check", status=1, message=message)
    main.write_bytes(application.encode())

    # A consumed package gets neither the app's direct edges nor its implicit root scope.
    for text, message in [
        (algebra_text.replace("pkg/units", "pkg/geometry"), "Undeclared direct package import"),
        (algebra_text.replace('"/src/helper shared.void"', '"../../src/app.void"'), "Import escapes"),
        (algebra_text.replace("units.next()", "app_only()"), "app_only"),
        (algebra_text.replace("units.next()", "local.secret()"), "secret"),
    ]:
        main.write_bytes((application + "public func app_only() -> i32:\n    return 3\n").encode())
        write(root / "deps/algebra", "src/helper shared.void",
              "internal func adjust() -> i32:\n    return 4\nprivate func secret() -> i32:\n    return 5\n")
        algebra.write_bytes(text.encode())
        cli(root, "resolve", "--refresh")
        cli(root, "check", status=1, message=message)

    algebra.write_bytes(b"func broken(\n")
    cli(root, "resolve", "--refresh")
    with Client(server, root, project=True) as client:
        errors = client.initialize()
        assert any(n["uri"] == algebra.as_uri() and n["diagnostics"] for n in errors), errors
        algebra.write_bytes(algebra_text.encode())
        cli(root, "resolve", "--refresh")
        reply, notes = client.request("void/refreshProject", {})
        assert "result" in reply, reply
        assert any(n["uri"] == algebra.as_uri() and not n["diagnostics"] for n in notes), notes
        assert rpc(client, "textDocument/definition", main, 3, 20)["uri"] == geometry.as_uri()
        client.shutdown()

    library = base / "library project"
    package(library, ".", "library", "use pkg/units\npublic func read() -> i32:\n    return units.next()\n",
            [("units", "units")])
    package(library, "units", "units", units)
    cli(library, "resolve")
    cli(library, "check")

    plain = base / "dependency free"
    write(plain, manifest, policy("plain"))
    write(plain, "main.void", "use lib/tuning as t\nfunc main():\n    print(t.answer())\n")
    write(plain, "libs/tuning/tuning.void", "public func answer() -> i32:\n    return 42\n")
    assert cli(plain, "run", "--no-cache").stdout == b"42\n"
    assert not (plain / "void.lock").exists()

print("D2 locked package CLI, typed IR, JIT/AOT, cache and voidls checks passed")
