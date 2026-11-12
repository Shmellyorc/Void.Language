"""Exercise compiler-owned return metadata in the native project LSP."""
from pathlib import Path
import sys
from tempfile import TemporaryDirectory

from editor_recovery import Client, labels, query, request, roles, update

MANIFEST = """schema=1
[project]
id='org.test.multireturn'
name='Multiple returns'
version='1.0.0'
kind='application'
[application]
id='org.test.multireturn'
[sources]
roots=['src']
"""
HELPER = """public func named(value: i32) -> (count: i32, status: string):
    return value, "ready"
public func unnamed() -> (bool, string):
    return true, "ready"
public func echo<t>(item: t) -> (copy: t, ready: bool):
    return item, true
"""
SOURCE = """use "./helper.void" as lib
func main() -> i32:
    var input = 2
    var first, message = lib.named(input)
    var valid, label = lib.unnamed()
    var value, ready = lib.echo(message)
    print("雪😀", first, message, valid, label, value, ready)
    return 0
"""
NAMED = "named(value: i32) -> (count: i32, status: string)"
UNNAMED = "unnamed() -> (bool, string)"


def hover(client, path, source, needle):
    return query(client, "hover", path, source, needle)["contents"]["value"]


def signature(client, path, source, prefix):
    result = query(client, "signatureHelp", path, source, prefix, after=True)
    assert result and len(result["signatures"]) == 1, result
    return result["signatures"][0]["label"]


with TemporaryDirectory(prefix="VOID LSP returns 雪 ") as directory:
    root = Path(directory)
    (root / "src").mkdir()
    (root / "void.project.toml").write_text(MANIFEST, encoding="utf-8")
    main, helper, other = (root / "src" / name for name in ("main.void", "helper.void", "other.void"))
    main.write_text(SOURCE, encoding="utf-8")
    helper.write_text(HELPER, encoding="utf-8")
    with Client(Path(sys.argv[1]).resolve(), root, project=True) as client:
        capabilities = {"textDocument": {
            "semanticTokens": {"requests": {"full": True}, "formats": ["relative"],
                               "tokenTypes": ["variable", "parameter"], "tokenModifiers": ["declaration"]},
            "publishDiagnostics": {"versionSupport": True}}}
        result = request(client, "initialize", {"capabilities": capabilities})
        legend = result["capabilities"]["semanticTokensProvider"]["legend"]["tokenTypes"]
        client.notify("initialized", {})
        client.sync()
        assert not update(client, main, SOURCE, 1)
        assert not update(client, helper, HELPER, 1)
        assert {(0, "count", "parameter"), (0, "status", "parameter"),
                (4, "copy", "parameter")} <= roles(client, helper, HELPER, legend)
        assert {(3, "message", "variable"), (6, "first", "variable"),
                (6, "message", "variable")} <= roles(client, main, SOURCE, legend)
        assert NAMED in hover(client, main, SOURCE, "named(input)")
        assert UNNAMED in hover(client, main, SOURCE, "unnamed()")
        assert "echo(item: string) -> (copy: string, ready: bool)" in hover(client, main, SOURCE, "echo(message)")
        assert hover(client, main, SOURCE, "message, valid") == "message: string"
        definition = query(client, "definition", main, SOURCE, "named(input)")
        assert definition["uri"] == helper.as_uri() and definition["range"]["start"]["line"] == 0, definition
        local = query(client, "definition", main, SOURCE, "message, valid")
        assert local["uri"] == main.as_uri() and local["range"]["start"] == {"line": 3, "character": 15}, local
        assert signature(client, main, SOURCE, "lib.named(") == NAMED
        assert signature(client, main, SOURCE, "lib.unnamed(") == UNNAMED
        assert "message" in labels(client, main, SOURCE, '"雪😀", first, mes')
        assert "count" not in labels(client, main, SOURCE, '"雪😀", first, ')

        dirty_helper = HELPER.replace("status: string", "dirty_status: string")
        assert not update(client, helper, dirty_helper, 2)
        dirty_named = NAMED.replace("status:", "dirty_status:")
        assert signature(client, main, SOURCE, "lib.named(") == dirty_named
        assert dirty_named in hover(client, main, SOURCE, "named(input)")
        assert "dirty_status" not in helper.read_text(encoding="utf-8")

        incomplete = SOURCE.replace("    return 0", "    pass editing\n    return 0")
        assert update(client, main, incomplete, 2)
        assert dirty_named in hover(client, main, incomplete, "named(input)")
        assert "message" in labels(client, main, incomplete, '"雪😀", first, mes')
        assert (6, "first", "variable") in roles(client, main, incomplete, legend)

        no_rhs = SOURCE.replace("lib.named(input)", "")
        assert update(client, main, no_rhs, 3)
        assert query(client, "hover", main, no_rhs, "first, message =") is None

        bad_arity = SOURCE.replace("first, message =", "first, message, excess =")
        diagnostics = update(client, main, bad_arity, 4)
        error = next(d for d in diagnostics if "requires 2 bindings, received 3" in d["message"])
        assert error["range"]["start"] == {"line": 3, "character": 24}, error
        assert query(client, "hover", main, bad_arity, "message, valid") is None

        assert update(client, helper, "public func named(\n", 3)
        assert query(client, "hover", main, bad_arity, "named(input)") is None
        assert query(client, "signatureHelp", main, bad_arity, "lib.named(", after=True) is None
        assert not roles(client, main, bad_arity, legend)

        other_text = """func named() -> (selected: i32, enabled: bool):
    return 1, true
func main() -> i32:
    var value, flag = named()
    return value
"""
        other.write_text(other_text, encoding="utf-8")
        selected = request(client, "void/reselectProject", {"entry": "src/other.void"})
        assert selected["entry"] == other.as_posix(), selected
        assert not update(client, other, other_text, 1)
        assert "named() -> (selected: i32, enabled: bool)" in hover(client, other, other_text, "named()\n")
        assert (0, "selected", "parameter") in roles(client, other, other_text, legend)
        client.notify("textDocument/didClose", {"textDocument": {"uri": main.as_uri()}})
        assert not roles(client, main, SOURCE, legend)
        client.sync()
        client.shutdown()
print("Multiple-return LSP labels, UTF-16, definitions, completion, recovery and project switch passed")
