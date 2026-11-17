"""Exercise installed custom templates through the CLI, JIT, AOT and voidls."""
from concurrent.futures import ThreadPoolExecutor
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import tomllib

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from lsp_client import Client

compiler, server, readobj = map(lambda x: Path(x).resolve(), sys.argv[1:4])
fixture = Path(__file__).resolve().with_name("template_fixture")
manifest = "void.project.toml"
template = ".void/template.toml"


def run(*args, cwd, env=None, status=0, message=None):
    result = subprocess.run(list(map(str, args)), cwd=cwd, env=env, capture_output=True, timeout=90)
    assert result.returncode == status and (message is None or message.encode() in result.stdout + result.stderr), (args, result)
    assert b"AddressSanitizer" not in result.stderr and b"runtime error:" not in result.stderr, result
    return result.stdout.replace(b"\r\n", b"\n")


def editor(root):
    entry, parts = root / "src/FirstGame.void", root / "src/parts.void"
    document = {"uri": entry.as_uri()}
    text = entry.read_text()
    with Client(server, root, project=True) as client:
        assert not any(event["diagnostics"] for event in client.initialize())
        local = {"textDocument": document, "position": {"line": 4, "character": 17}}
        result, _ = client.request("textDocument/definition", local)
        assert result["result"]["uri"] == parts.as_uri(), result
        client.notify("textDocument/didOpen", {"textDocument": dict(document, languageId="void", version=1, text=text)})
        client.sync()
        incomplete = text.replace("parts.score()", "parts.sco")
        client.notify("textDocument/didChange", {"textDocument": dict(document, version=2),
                                                "contentChanges": [{"text": incomplete}]})
        assert any(event["diagnostics"] for event in client.sync())
        result, _ = client.request("textDocument/completion", local)
        items = result["result"]
        assert any(item["label"] == "score" for item in (items["items"] if isinstance(items, dict) else items)), result
        client.notify("textDocument/didChange", {"textDocument": dict(document, version=3),
                                                "contentChanges": [{"text": text}]})
        assert not any(event["diagnostics"] for event in client.sync())
        client.notify("textDocument/didClose", {"textDocument": document})
        before = parts.read_bytes()
        parts.write_bytes(before.replace(b"func score", b"func points"))
        client.notify("workspace/didChangeWatchedFiles", {"changes": [{"uri": parts.as_uri(), "type": 2}]})
        assert any(event["diagnostics"] for event in client.sync())
        parts.write_bytes(before)
        client.notify("workspace/didChangeWatchedFiles", {"changes": [{"uri": parts.as_uri(), "type": 2}]})
        assert not any(event["diagnostics"] for event in client.sync())
        result, _ = client.request("textDocument/definition", local)
        assert result["result"]["uri"] == parts.as_uri(), result
        client.shutdown()


with tempfile.TemporaryDirectory(prefix="VOID G62 templates ") as temporary:
    root = Path(temporary).resolve()
    env = dict(os.environ)
    env.pop("VOID_NATIVE_CATALOG", None)
    if os.name == "nt":
        env["LOCALAPPDATA"] = str(root / "user")
        store = root / "user/VOID/templates"
    else:
        env["XDG_DATA_HOME"] = str(root / "user")
        store = root / "user/void/templates"

    def cli(*args, cwd=root, **kw):
        return run(compiler, *args, cwd=cwd, env=env, **kw)

    source = root / "authored"
    shutil.copytree(fixture, source)
    work = root / "work"
    work.mkdir()
    assert b"voidgame" not in cli("new", "list") and not store.exists()
    # Registration copies data, including scripts, without executing it.
    script = source / "make.sh"
    script.write_text("#!/bin/sh\ntouch G62-unexpected-script\n", encoding="utf-8")
    with ThreadPoolExecutor(2) as pool:
        assert all(b"Installed template" in output for output in pool.map(
            lambda _: cli("new", "install", "--dir", "../authored", cwd=work), range(2)))
    registration = store / "org.example.voidgame"
    assert sorted(p.name for p in store.iterdir()) == [".void", registration.name]
    assert not (root / "G62-unexpected-script").exists() and not (work / "G62-unexpected-script").exists()
    assert not (store.parent / "packages").exists()
    listing = cli("new", "list")
    assert b"VOID 2D Game" in listing and b"voidgame, game-starter" in listing and b"Game, 2D" in listing
    assert cli("new") == listing
    cli("new", "voidgame", status=1, message="Use --name NAME")
    cli("new", "voidgame", "--name", "../Outside", status=1, message="portable identifier")
    cli("new", "voidgame", "--name", "CON", status=1, message="portable identifier")
    cli("new", "--", "voidgame")
    assert (root / "voidgame" / manifest).is_file()
    cli("new", "install")
    cli("new", "uninstall")
    assert (root / "install" / manifest).exists() and (root / "uninstall" / manifest).exists()
    for name, alias in (("FirstGame", "voidgame"), ("SecondGame", "game-starter")):
        cli("new", alias, "--name", name)
        instance = root / name
        parsed = tomllib.loads((instance / manifest).read_text())
        assert parsed["project"]["name"] == name and parsed["project"]["entry"] == f"src/{name}.void"
        assert parsed["application"]["resources"] == [f"assets/{name}.bin"]
        assert (instance / f"assets/{name}.bin").read_bytes() == (fixture / "assets/TemplateGame.bin").read_bytes()
        assert (instance / "make.sh").read_bytes() == script.read_bytes()
        assert not (instance / ".void").exists() and not (instance / "void.lock").exists()
        assert b"TemplateGame" not in (instance / f"src/{name}.void").read_bytes()
        assert (instance / "README.md").read_text().splitlines()[0] == "# " + name
        for profile in ("debug", "release"):
            expected = f"{name}\n42\n".encode()
            assert cli("run", ".", "--profile", profile, "--no-cache", cwd=instance) == expected
            exe = root / (f"{name}-{profile}" + (".exe" if os.name == "nt" else ""))
            cli("build", ".", "--profile", profile, "-o", exe, cwd=instance)
            assert run(exe, cwd=instance, env=env) == expected
            if os.name == "nt":
                assert b"SizeOfStackReserve: 1048576" in run(readobj, "--file-headers", exe, cwd=root)
    ids = [tomllib.loads((root / name / manifest).read_text()) for name in ("FirstGame", "SecondGame")]
    assert len({data[role]["id"] for data in ids for role in ("project", "application")}) == 4
    authored_hash = (root / "FirstGame" / manifest).read_bytes()
    cli("new", "voidgame", "--name", "FirstGame", status=1, message="already exists")
    assert authored_hash == (root / "FirstGame" / manifest).read_bytes()
    editor(root / "FirstGame")

    invalid = root / "invalid"
    shutil.copytree(fixture, invalid)
    metadata = (invalid / template).read_text()
    cases = [
        (metadata.replace("org.example.voidgame", "org.example.other"), "short name already installed"),
        (metadata.replace('["voidgame", "game-starter"]', '["app"]'), "Invalid, reserved or duplicate"),
        (metadata.replace('["voidgame", "game-starter"]', '["classlib"]'), "Invalid, reserved or duplicate"),
        (metadata.replace('["voidgame", "game-starter"]', '["one", "one"]'), "Invalid, reserved or duplicate"),
        (metadata.replace('format = 1', 'format = "1"'), "format must be integer"),
        (metadata + "\ninstall-script = \"make.sh\"\n", "Unknown template field"),
        (metadata.replace('TemplateGame', '../Escape'), "Invalid template identity or source-name"),
    ]
    for text, message in cases:
        (invalid / template).write_text(text, encoding="utf-8")
        cli("new", "install", "--dir", invalid, status=1, message=message)
    (invalid / template).write_text(metadata, encoding="utf-8")
    (invalid / "void.lock").write_bytes(b"authored lock")
    cli("new", "install", "--dir", invalid, status=1, message="reserved template path")
    (invalid / "void.lock").unlink()
    if os.name != "nt":
        (invalid / "escape").symlink_to(root / "FirstGame", target_is_directory=True)
        cli("new", "install", "--dir", invalid, status=1)
        (invalid / "escape").unlink()

    stored = registration / "source/src/parts.void"
    original = stored.read_bytes()
    stored.write_bytes(original + b"\n# changed\n")
    cli("new", "list", status=1, message="Template registration changed")
    cli("new", "voidgame", "--name", "Tampered", status=1, message="Template registration changed")
    cli("new", "uninstall", "voidgame", status=1, message="Template registration changed")
    assert not (root / "Tampered").exists() and registration.exists()
    stored.write_bytes(original)
    cli("new", "uninstall", "game-starter")
    assert not registration.exists() and b"voidgame" not in cli("new", "list")
    assert cli("run", ".", cwd=root / "FirstGame") == b"FirstGame\n42\n"

    (invalid / template).write_text(metadata.replace("org.example.voidgame", "org.example.collision"), encoding="utf-8")
    (invalid / "src/Conflict.void").write_text("# intentionally colliding path\n", encoding="utf-8")
    cli("new", "install", "--dir", invalid)
    cli("new", "voidgame", "--name", "Conflict", status=1, message="filename collision")
    assert not (root / "Conflict").exists() and not list(root.glob("acquire-*"))
    cli("new", "uninstall", "voidgame")
    print("Template lifecycle, concurrency, binary preservation, JIT/AOT, editor refresh and rejection controls passed.")
