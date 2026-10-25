"""Globals use compiler-owned bindings, versioned edits and overlay recovery."""
from pathlib import Path
import sys
import os
import shutil
import tempfile
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from lsp_client import Client

server = Path(sys.argv[1]).resolve()
def at(text, needle, delta=0):
    prefix = text[:text.index(needle) + delta]
    return {"line": prefix.count("\n"), "character": len(prefix.rsplit("\n", 1)[-1].encode("utf-16-le")) // 2}

with tempfile.TemporaryDirectory(prefix="VOID globals editor é🎮 ") as directory:
    root = Path(directory)
    entry = root / "main.void"
    module = root / "game.void"
    text = ('use "./game.void" as game\nuse state as shared\n## Shared score.\nvar score = 0\n'
            'func main():\n    game.reward()\n    print("🎮", score, game.defeated)\n    print(shared.count)\n')
    game = ('## Shared defeat count.\nvar defeated = 0\nprivate var secret = 7\n'
            'func reward():\n    score += 10\n    defeated += 1\n')
    entry.write_text(text, encoding="utf-8")
    module.write_text(game, encoding="utf-8")
    standard = root / "standard"
    shutil.copytree(Path(__file__).resolve().parents[2] / "stdlib", standard)
    (standard / "state.void").write_text('## Installed shared counter.\nvar count = 1\n', encoding="utf-8")
    os.environ["VOID_STDLIB_ROOT"] = str(standard)
    uri = entry.as_uri()
    with Client(server, entry) as client:
        client.request("initialize", {"capabilities": {
            "textDocument": {"publishDiagnostics": {"versionSupport": True}},
            "workspace": {"workspaceEdit": {"documentChanges": True, "failureHandling": "textOnlyTransactional"}}}})
        client.notify("initialized", {})
        client.sync()
        version = 0
        active = text
        def replace(value, stale=False):
            global version, active
            if not stale:
                version += 1
                active = value
            client.notify("textDocument/didOpen" if version == 1 else "textDocument/didChange",
                {"textDocument": {"uri": uri, "languageId": "void", "version": version if not stale else version - 1,
                                  **({"text": value} if version == 1 else {})},
                 **({"contentChanges": [{"text": value}]} if version != 1 else {})})
            return [d for n in client.sync() for d in n["diagnostics"]]
        def query(method, needle, delta=0, **extra):
            return client.request(method, {"textDocument": {"uri": uri}, "position": at(active, needle, delta), **extra})[0]
        assert not replace(text)
        hover = query("textDocument/hover", "score, game")["result"]["contents"]["value"]
        assert "var score: i32" in hover and "Shared score" in hover, hover
        definition = query("textDocument/definition", "game.defeated", 5)["result"]
        assert definition["uri"] == module.as_uri() and definition["range"]["start"]["line"] == 1, definition
        refs = query("textDocument/references", "var score", 4, context={"includeDeclaration": True})["result"]
        assert len(refs) == 3, refs
        rename = query("textDocument/rename", "var score", 4, newName="points")
        assert "result" in rename, rename
        changes = rename["result"]["documentChanges"]
        assert len(changes) == 2 and sum(len(x["edits"]) for x in changes) == 3, changes
        assert query("textDocument/rename", "var score", 4, newName="game").get("error")
        standard_hover = query("textDocument/hover", "shared.count", 7)["result"]["contents"]["value"]
        assert "var count: i32" in standard_hover and "Installed shared counter" in standard_hover
        assert query("textDocument/rename", "shared.count", 7, newName="changed").get("error")
        partial = text.replace('print("🎮", score, game.defeated)', 'print(game.)')
        replace(partial)
        items = query("textDocument/completion", "print(game.", len("print(game."))["result"]
        if isinstance(items, dict):
            items = items["items"]
        names = {x["label"] for x in items}
        assert {"defeated", "reward"} <= names and "secret" not in names, names
        partial = text.replace('print("🎮", score, game.defeated)', 'sco = score')
        replace(partial)
        items = query("textDocument/completion", "sco =", 3)["result"]
        assert "score" in {x["label"] for x in items}, items
        partial = text.replace('print("🎮", score, game.defeated)', 'game.def = 3')
        replace(partial)
        items = query("textDocument/completion", "game.def", len("game.def"))["result"]
        assert "defeated" in {x["label"] for x in items}, items
        partial = text.replace('print("🎮", score, game.defeated)', 'score = sco')
        replace(partial)
        items = query("textDocument/completion", "= sco", len("= sco"))["result"]
        assert "score" in {x["label"] for x in items}, items
        broken = text.replace("var score = 0", "var score = missing")
        assert replace(broken)
        assert query("textDocument/hover", "score, game").get("result") is None
        assert not replace(text)
        replace(broken, stale=True)
        assert query("textDocument/hover", "score, game")["result"]
        assert not replace(text.replace("var score = 0", "var score = 12"))
        assert query("textDocument/definition", "score, game")["result"]
        assert entry.read_text(encoding="utf-8") == text
        client.shutdown()
print("Global hover, completion, cross-file definition/references/rename, overlays and stale-version checks passed")
