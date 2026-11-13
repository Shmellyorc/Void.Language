"""S5 process contracts for the compiler and editor's creation subprocesses."""
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import tomllib

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from lsp_client import Client

compiler, server, evidence = map(lambda value: Path(value).resolve(), sys.argv[1:])
calls = []
manifest_name = "void.project.toml"
main = "func main() -> i32:\n    return 19\n"
suffix = ".exe" if os.name == "nt" else ""


def invoke(binary, *args, cwd, status=0):
    command = list(map(str, (binary, *args)))
    result = subprocess.run(command, cwd=cwd, capture_output=True, timeout=80)
    record = {
        "argv": command, "cwd": str(cwd), "status": result.returncode,
        "stdout": result.stdout.decode("utf-8", "replace"),
        "stderr": result.stderr.decode("utf-8", "replace"),
    }
    calls.append(record)
    if status is not None:
        assert result.returncode == status, record
    return record


def manifest(root, role="application"):
    data = tomllib.loads((root / manifest_name).read_text(encoding="utf-8"))
    assert data["schema"] == 1 and data["project"]["kind"] == ("application" if role == "application" else "library")
    assert "entry" not in data["project"]
    return data


try:
    with tempfile.TemporaryDirectory(prefix="VOID S5 creation ") as folder:
        base = Path(folder).resolve(strict=True)
        def run(*args, status=0, cwd=base):
            return invoke(compiler, *args, status=status, cwd=cwd)

        def wire(*args):
            return json.loads(invoke(server, *args, cwd=base)["stdout"])

        def directory(name):
            path = base / name
            path.mkdir()
            return path

        root = directory("existing")
        original = root / "game.void"
        original.write_bytes(main.encode())
        preview = run("init", cwd=root)
        app = manifest(root)
        project_id, role_id = app["project"]["id"], app["application"]["id"]
        assert re.fullmatch(r"local\.p[0-9a-f]{32}", project_id) and re.fullmatch(r"local\.a[0-9a-f]{32}", role_id)
        assert project_id[7:] != role_id[7:] and project_id in preview["stdout"] and role_id in preview["stdout"]
        assert app["project"]["name"] == "existing" and app["project"]["version"] == "0.1.0"
        assert {p.name for p in root.iterdir()} == {manifest_name, "game.void"} and original.read_bytes() == main.encode()
        for options in ([], ["--no-opt"]):
            assert "VOID5012" not in run("run", "--project", root, *options, status=19)["stderr"]
        before = (root / manifest_name).read_bytes()
        run("init", root, status=1)
        assert before == (root / manifest_name).read_bytes()

        no_main = directory("empty")
        run("init", no_main)
        assert "main" in run("check", "--project", no_main, status=1)["stderr"]
        many = directory("many")
        for name in ("game.void", "launcher.void"):
            (many / name).write_text(main, encoding="utf-8")
        run("init", many)
        diagnostic = run("check", "--project", many, status=1)["stderr"]
        assert "game.void" in diagnostic and "launcher.void" in diagnostic

        app_root = base / "arbitrary"
        run("new", app_root, "--source", "tools/launcher.void", "--name", 'Orbit "Pilot" \\',
            "--version", "1.2.3-beta.4+qual")
        generated = manifest(app_root)
        generated_id = generated["application"]["id"]
        assert generated["project"]["id"] != project_id and generated_id != role_id
        assert generated["project"]["name"] == 'Orbit "Pilot" \\' and generated["project"]["version"] == "1.2.3-beta.4+qual"
        assert generated["sources"]["roots"] == ["tools"]
        starter = app_root / "tools/launcher.void"
        assert "func main" in starter.read_text(encoding="utf-8")
        for profile in ("release", "debug"):
            run("run", "--project", app_root, "--profile", profile)
        native = base / ("generated" + suffix)
        run("build", "--project", app_root, "-o", native)
        invoke(native, cwd=base)
        starter.write_text(
            'use os\nfunc main() -> i32:\n'
            '    if os.data_dir() is Ok(var automatic):\n'
            f'        if os.data_dir("{generated_id}") is Ok(var explicit):\n'
            '            if automatic == explicit:\n'
            '                return 31\n'
            '    return 1\n', encoding="utf-8")
        for profile in ("release", "debug"):
            run("run", "--project", app_root, "--profile", profile, status=31)
        run("build", "--project", app_root, "-o", native)
        invoke(native, cwd=base, status=31)

        app_default = base / "default-app"
        run("new", app_default, "--project-id", "org.test.project", "--app-id", "org.test.application")
        defaults = manifest(app_default)
        assert defaults["project"]["id"] == "org.test.project" and defaults["application"]["id"] == "org.test.application"
        assert (app_default / "src/app.void").is_file() and defaults["sources"]["roots"] == ["src"]
        # The CLI and editor use the same creation contract. Keep the resulting
        # manifests minimal, then exercise them with a no-result application.
        fresh = base / "quiet-new"
        run("new", fresh)
        initialized = directory("quiet-init")
        created_in_editor = directory("quiet-editor")
        (fresh / "src/app.void").write_bytes(b"func main():\n    pass\n")
        (initialized / "game.void").write_bytes(b"func main():\n\tpass\n")
        (created_in_editor / "game.void").write_bytes(b"func main():\n\tpass\n")
        run("init", initialized)
        inventory = wire("--discover-creation", created_in_editor, "")["creation"]
        plan = wire("--plan-project", created_in_editor, "application", "", "")
        assert wire("--create-project", created_in_editor, inventory["rootIdentity"], plan["kind"], plan["name"],
                    plan["version"], plan["projectId"], plan["roleId"], plan["packageName"])["status"] == "created"
        for generated_root in (fresh, initialized, created_in_editor):
            generated_manifest = generated_root / manifest_name
            generated_before = generated_manifest.read_bytes()
            data = manifest(generated_root)
            assert "build" not in data and re.fullmatch(r"local\.p[0-9a-f]{32}", data["project"]["id"])
            assert re.fullmatch(r"local\.a[0-9a-f]{32}", data["application"]["id"])
            if generated_root != fresh:
                assert set(data) == {"schema", "project", "application"}, data
            for profile in ("debug", "release"):
                flags = ("--project", generated_root, "--profile", profile)
                output = base / (generated_root.name + "-" + profile + suffix)
                for result in (run("check", *flags), run("run", *flags), run("build", *flags, "-o", output)):
                    assert "VOID5012" not in result["stderr"], result
                invoke(output, cwd=base)
            assert generated_manifest.read_bytes() == generated_before
        lib = base / "lib"
        run("new", lib, "--kind", "library", "--package-name", "toolkit", "--package-id", "org.test.toolkit")
        package = manifest(lib, "package")
        assert package["package"] == {"id": "org.test.toolkit", "name": "toolkit", "entry": "src/toolkit.void"}
        assert "main" not in (lib / "src/toolkit.void").read_text(encoding="utf-8")
        run("check", "--project", lib)
        run("run", "--project", lib, status=1)
        library_default = base / "toolkit"
        run("new", library_default, "--kind", "library")
        library = manifest(library_default, "package")
        assert re.fullmatch(r"local\.l[0-9a-f]{32}", library["package"]["id"])
        assert library["package"]["entry"] == "src/toolkit.void"
        custom = base / "custom-library"
        run("new", custom, "--kind", "library", "--package-name", "custom", "--source", "api/custom.void")
        assert manifest(custom, "package")["package"]["entry"] == "api/custom.void"
        run("check", "--project", custom)
        initialized_library = directory("initialized_library")
        (initialized_library / "existing.void").write_text("public func answer() -> i32:\n    return 42\n", encoding="utf-8")
        run("init", initialized_library, "--kind", "library")
        assert "entry" not in manifest(initialized_library, "package")["package"]

        # The unedited new -> add -> tidy journey must compose across commands.
        run("new", "app")
        run("new", "geometry", "--kind", "library", "--package-name", "geometry")
        run("add", "geometry", "--project", "app", "--path", "../geometry")
        run("tidy", "--check", "--project", "app")
        (base / "app/src/app.void").write_text(
            "use pkg/geometry as g\nfunc main() -> i32:\n    return g.answer()\n", encoding="utf-8")
        run("run", "--no-cache", "--project", "app", status=42)
        with Client(server, base / "app", project=True) as client:
            assert not any(n["diagnostics"] for n in client.initialize())
            response, _ = client.request("textDocument/definition", {
                "textDocument": {"uri": (base / "app/src/app.void").as_uri()},
                "position": {"line": 0, "character": 10},
            })
            assert response["result"]["uri"] == (base / "app/libs/geometry/src/geometry.void").as_uri(), response
            client.shutdown()

        child = root / "child"
        child.mkdir()
        run("init", child, status=1)
        assert not list(child.iterdir())
        run("init", child, "--nested")
        run("new", root / "other", status=1)
        run("new", root / "other", "--nested")
        run("new", root / "Other", "--nested", status=1)
        assert before == (root / manifest_name).read_bytes()
        for name in ("VOID.PROJECT.TOML", manifest_name):
            collision = directory("case" if name.isupper() else "malformed")
            (collision / name).write_bytes(b"[=roject]\n")
            run("init", collision, status=1)
            assert [p.name for p in collision.iterdir()] == [name]
            assert (collision / name).read_bytes() == b"[=roject]\n"
        for args in (("new",), ("init", "--source", "game.void"), ("init", "--kind", "other"),
                     ("init", "--package-id", "a"), ("init", "--nested", "--nested"), ("init", "--unknown"),
                     ("init", "--name")):
            run(*args, status=2, cwd=base)
        for starter_path in ("../escape.void", "tests/game.void", "src/BUILD/app.void", "game.txt", "src//game.void", "Tests/game.void", "void.project.toml/game.void"):
            run("new", base / "invalid", "--source", starter_path, status=1)
        run("new", base / "VOID.PROJECT.TOML", status=1)
        run("new", base / "invalid", "--version", "01.2.3", status=1)
        run("new", base / "non-identifier", "--kind", "library", status=1)
        run("new", base / "absent-parent" / "app", status=1)
        assert not (base / "invalid").exists()
        assert not (base / "non-identifier").exists()

        alias = base / "alias"
        if os.name == "nt":
            import _winapi
            _winapi.CreateJunction(str(no_main), str(alias))
        else:
            alias.symlink_to(no_main, target_is_directory=True)
        try:
            run("init", alias, status=1)
            run("new", alias / "attack", "--nested", status=1)
            assert not (no_main / "attack").exists()
        finally:
            if os.name == "nt":
                alias.rmdir()
            else:
                alias.unlink()
        if os.name != "nt" and os.getuid() != 0:
            protected = directory("readonly")
            protected.chmod(0o500)
            try:
                run("init", protected, status=1)
                run("new", protected / "attempt", status=1)
                assert not list(protected.iterdir())
            finally:
                protected.chmod(0o700)

        contested = directory("contested")
        for args, target in ((("init", contested), contested), (("new", base / "contested-new"), base / "contested-new")):
            with ThreadPoolExecutor(2) as pool:
                winners = list(pool.map(lambda _: run(*args, status=None), range(2)))
            assert sorted(result["status"] for result in winners) == [0, 1], winners
            assert not list(target.parent.glob(".void-create-*")) and not list(target.glob(".void-create-*"))
            manifest(target)

        editor = directory("editor")
        first = editor / "game.void"
        first.write_bytes(b"invalid and intentionally not parsed")
        excluded = editor / "tests"
        excluded.mkdir()
        (excluded / "test.void").write_text(main, encoding="utf-8")
        child = editor / "child"
        child.mkdir()
        run("init", child)
        (child / "game.void").write_text(main, encoding="utf-8")
        inventory = wire("--discover-creation", editor, "")["creation"]
        assert inventory["complete"] and not inventory["manifestPresent"] and inventory["sources"] == ["/game.void"], inventory
        for n in range(16):
            (editor / f"extra{n}.void").write_text(main, encoding="utf-8")
        incomplete = wire("--discover-creation", editor, "")["creation"]
        assert not incomplete["complete"] and incomplete["eligibleCount"] == 17 and not incomplete["sources"], incomplete
        plan = wire("--plan-project", editor, "application", "", "")
        assert not (editor / manifest_name).exists()
        args = ("--create-project", editor, inventory["rootIdentity"], plan["kind"], plan["name"],
                plan["version"], plan["projectId"], plan["roleId"], plan["packageName"])
        assert wire(*args)["status"] == "created"
        assert manifest(editor)["application"]["id"] == plan["roleId"]
        saved = (editor / manifest_name).read_bytes()
        assert wire(*args)["status"] == "collision" and (editor / manifest_name).read_bytes() == saved
        assert wire("--plan-project", editor, "application", "", "")["status"] == "collision"
        assert first.read_bytes() == b"invalid and intentionally not parsed"
        renamed = base / "renamed"
        editor.rename(renamed)
        editor.mkdir()
        assert "root changed" in invoke(server, *args, cwd=base, status=2)["stderr"]
        assert not list(editor.iterdir())
        nullable = wire("--discover-project", no_main, "", no_main, "")
        assert nullable["schema"] == 2 and nullable["entry"] is None, nullable
    print(f"S5 creation process: {len(calls)} invocations passed")
finally:
    evidence.write_text(json.dumps(calls, indent=2) + "\n", encoding="utf-8")
