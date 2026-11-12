"""Inspect verified local graphs and preserve exact filesystem state."""
from hashlib import sha256
import json
from pathlib import Path
import subprocess
from tempfile import TemporaryDirectory

from dependencies import compiler, dependency, diamond, invoke, library, manifest, manifest_name, source, tree_digest, write


def filesystem_state(root):
    def state(path):
        stat = path.stat()
        return stat.st_mode, stat.st_mtime_ns, path.read_bytes() if path.is_file() else None

    return {path.relative_to(root).as_posix(): state(path) for path in [root, *root.rglob("*")]}


def explain(root, *, status=0, message=None, json_mode=True, env=None):
    before = filesystem_state(root)
    command = [compiler, "deps", "explain", "--project", root]
    if json_mode:
        command.append("--json")
    result = subprocess.run(command, capture_output=True, timeout=40, env=env)
    assert filesystem_state(root) == before, "Inspection changed the project filesystem"
    assert result.returncode == status, (command, result)
    if json_mode:
        assert not result.stderr, result
        record = json.loads(result.stdout)
        assert record["schema"] == 1 and record["status"] == ("ok" if status == 0 else "error"), record
        if status:
            assert set(record) == {"schema", "status", "message"} and message in record["message"], record
    elif status:
        assert not result.stdout and message.encode() in result.stderr, result
    return result.stdout


if __name__ == "__main__":
    with TemporaryDirectory(prefix="VOID dependency explanation 雪 ") as directory:
        base = Path(directory).resolve()
        root = base / "plain"
        write(root, manifest_name, manifest("demo"))
        assert json.loads(explain(root)) == {"schema": 1, "status": "ok", "project_id": "org.example.demo.project",
                                            "lock": {"status": "not_required"}, "packages": [], "relationships": []}
        assert b"No source package dependencies" in explain(root, json_mode=False)
        edge = dependency("choice", "deps/units", identity="org.example.units")
        write(root, manifest_name, manifest("demo", edge))
        units = library(root, "deps/units", "units")
        explain(root, status=1, message="Missing void.lock")
        explain(root, status=1, message="Run voidc resolve --project", json_mode=False)
        invoke(root)
        locked = root / "void.lock"
        verified = json.loads(explain(root))
        assert verified["lock"] == {"status": "verified", "sha256": sha256(locked.read_bytes()).hexdigest()}
        assert verified["packages"] == [{
            "id": "org.example.units", "version": "1.4.2", "entry": "src/api.void",
            "manifest_sha256": sha256((units / manifest_name).read_bytes()).hexdigest(),
            "tree_sha256": tree_digest({manifest_name: (units / manifest_name).read_bytes(), "src/api.void": source}, []),
            "source": {"kind": "local", "path": "deps/units"},
        }]
        assert verified["relationships"] == [{"direct": True, "parent": "org.example.demo.project",
            "alias": "choice", "target": "org.example.units", "version_requirement": ">=1.4.0,<2.0.0",
            "source": {"kind": "local", "path": "deps/units"}}]

        write(root, manifest_name, manifest("demo", edge.replace("dependencies.choice", "dependencies.renamed")))
        explain(root, status=1, message="Stale or tampered void.lock")
        write(root, manifest_name, manifest("demo", edge))
        write(units, "src/api.void", source + b"# changed\n")
        explain(root, status=1, message="--refresh")
        write(units, "src/api.void", source).unlink()
        explain(root, status=1, message="Package entry is unavailable")
        write(units, "src/api.void", source)
        write(root, manifest_name, manifest("demo", edge.replace("org.example.units", "org.example.wrong")))
        explain(root, status=1, message="Wrong dependency package ID")
        write(root, manifest_name, manifest("demo", edge,
            dependency("other", "deps/other", identity="org.example.units")))
        library(root, "deps/other", "units")
        explain(root, status=1, message="Dependency origin conflict")
        write(root, manifest_name, manifest("demo", edge))
        write(units, manifest_name, manifest("units", dependency("other", "../other"), library=True))
        write(root / "deps/other", manifest_name, manifest("other", dependency("units", "../units"), library=True))
        explain(root, status=1, message="Dependency cycle")
        write(root, manifest_name, manifest("demo", '\n[dependencies.choice]\nid="org.example.units"\n'
            'git="ssh://example.invalid/repo"\nrev="' + 'a' * 40 + '"\nversion="=1.4.2"\n'))
        explain(root, status=1, message="Invalid Git dependency origin or revision")

        original, relocated = base / "diamond", base / "relocated with spaces 雪"
        for project, reverse in [(original, False), (relocated, True)]:
            diamond(project, reverse)
            invoke(project)
        expected = explain(original)
        assert expected == explain(original) == explain(relocated)
        assert explain(original, json_mode=False) == explain(relocated, json_mode=False)
        graph = json.loads(expected)
        assert [p["id"] for p in graph["packages"]] == ["org.example.algebra", "org.example.geometry", "org.example.units"]
        assert [(edge["direct"], edge["parent"], edge["alias"], edge["target"]) for edge in graph["relationships"]] == [
            (True, "org.example.demo.project", "algebra", "org.example.algebra"),
            (True, "org.example.demo.project", "geometry", "org.example.geometry"),
            (False, "org.example.algebra", "units", "org.example.units"),
            (False, "org.example.geometry", "units", "org.example.units"),
        ]
        lock = original / "void.lock"
        before = lock.read_bytes()
        prefix, units_record = before.rsplit(b"[[package]]\n", 1)
        assert units_record.startswith(b'id = "org.example.units"')
        lock.write_bytes(prefix)
        explain(original, status=1, message="Stale or tampered void.lock")
    print("Dependency explanation verified deterministic graph and error output without project writes")
