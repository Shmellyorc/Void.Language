"""Guard the compiler's one source correction against stale and unsafe contexts."""
from pathlib import Path
import sys
from tempfile import TemporaryDirectory

from editor_recovery import Client, request, update

MANIFEST = """schema=1
[project]
id="org.example.fixes"
name="Fixes"
version="1.0.0"
kind="application"
entry="src/main.void"
[application]
id="org.example.fixes.app"
[sources]
roots=["src"]
"""
SOURCE = """func main() -> i32:
    while false:
        pass
    return 0
"""


def actions(client, uri, line=1, start=4, end=9, kind="quickfix"):
    return request(client, "textDocument/codeAction", {
        "textDocument": {"uri": uri},
        "range": {"start": {"line": line, "character": start}, "end": {"line": line, "character": end}},
        "context": {"diagnostics": [], "only": [kind]},
    })


with TemporaryDirectory(prefix="VOID quick fixes 雪 ") as directory:
    root = Path(directory)
    src = root / "src"
    src.mkdir()
    main, other = src / "main.void", src / "other.void"
    main.write_text(SOURCE, encoding="utf-8")
    other.write_text("func main() -> i32:\n    return 1\n", encoding="utf-8")
    (root / "void.project.toml").write_text(MANIFEST, encoding="utf-8")
    server = Path(sys.argv[1]).resolve()
    with Client(server, root, project=True) as client:
        capabilities = request(client, "initialize", {"capabilities": {}})["capabilities"]
        assert "codeActionProvider" not in capabilities
        client.notify("initialized", {})
        client.sync()
        assert actions(client, main.as_uri()) == []
        client.shutdown()

    with Client(server, root, project=True) as client:
        capabilities = request(client, "initialize", {
            "capabilities": {}, "initializationOptions": {"voidGuardedFixes": True},
        })["capabilities"]
        assert capabilities["codeActionProvider"] == {"codeActionKinds": ["quickfix"]}
        client.notify("initialized", {})
        client.sync()
        diagnostics = update(client, main, SOURCE, 1)
        assert any(d["code"] == "VOID1104" for d in diagnostics), diagnostics
        action, = actions(client, main.as_uri())
        assert action["kind"] == "quickfix" and "edit" not in action, action
        assert actions(client, main.as_uri(), line=2) == []
        assert actions(client, main.as_uri(), kind="refactor") == []
        assert action["command"]["command"] == "void.applyQuickFix"
        ticket, = action["command"]["arguments"]
        assert ticket["version"] == 1 and ticket["uri"] == main.as_uri(), ticket
        assert request(client, "void/resolveQuickFix", ticket) == {"newText": "for", "expectedText": "while",
            "range": {"start": {"line": 1, "character": 4}, "end": {"line": 1, "character": 9}}}

        dirty = SOURCE.replace("while false", "while true")
        assert update(client, main, dirty, 2)
        assert request(client, "void/resolveQuickFix", ticket) is None
        action, = actions(client, main.as_uri())
        ticket, = action["command"]["arguments"]
        selected = request(client, "void/reselectProject", {"entry": "src/other.void"})
        assert selected["entry"] == other.as_posix()
        assert request(client, "void/resolveQuickFix", ticket) is None
        assert actions(client, main.as_uri()) == []
        request(client, "void/reselectProject", {"entry": "src/main.void"})
        assert update(client, main, "use pkg/missing\n" + SOURCE, 3)
        assert actions(client, main.as_uri(), line=0, start=4, end=15) == []
        assert not (root / "void.lock").exists() and not (root / ".void").exists()

        for name in ("one", "two"):
            (src / (name + ".void")).write_text("public func chosen() -> i32:\n    return 1\n", encoding="utf-8")
        ambiguous = 'use "./one.void"\nuse "./two.void"\nfunc main() -> i32:\n    return chosen()\n'
        diagnostics = update(client, main, ambiguous, 4)
        assert any(d["code"] == "VOID3004" for d in diagnostics), diagnostics
        assert actions(client, main.as_uri(), line=3, start=11, end=17) == []
        assert main.read_text(encoding="utf-8") == SOURCE
        client.notify("textDocument/didClose", {"textDocument": {"uri": main.as_uri()}})
        client.sync()
        assert actions(client, main.as_uri()) == []
        client.shutdown()
print("Compiler quick fix guarded document version, selection, visibility and untouched disk")
