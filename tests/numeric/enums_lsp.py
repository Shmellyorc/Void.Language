"""Exercise numeric enum declarations, cast tokens and overlays over real LSP pipes."""
from pathlib import Path
import json
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from lsp_client import Client

server, evidence = (Path(arg).resolve() for arg in sys.argv[1:])
events = []
main = '''use "./modes.void" as m

func main() -> i32:
    var raw: i32 = 7
    if raw as? m.mode is Some(var picked):
        if picked is m.mode.ready:
            return 7
    return 1
'''
modes = '''# Snow 🎮
public enum mode(i8):
    ## Value used when ready.
    ready = 7
    gone = -128
'''


def point(source, offset):
    prefix = source[:offset]
    return {"line": prefix.count("\n"), "character": len(prefix.rsplit("\n", 1)[-1].encode("utf-16-le")) // 2}


def request(client, method, params):
    response, notes = client.request(method, params)
    events.append({"method": method, "params": params, "response": response, "diagnostics": notes})
    assert "result" in response, response
    return response["result"]


def overlay(client, path, text, version, opened=False):
    document = {"uri": path.as_uri(), "version": version}
    if opened:
        client.notify("textDocument/didOpen", {"textDocument": dict(document, text=text, languageId="void")})
    else:
        client.notify("textDocument/didChange", {"textDocument": document, "contentChanges": [{"text": text}]})
    notes = client.sync()
    events.append({"uri": path.as_uri(), "version": version, "text": text, "diagnostics": notes})
    return [diagnostic for note in notes if note["uri"] == path.as_uri() for diagnostic in note["diagnostics"]]


try:
    with tempfile.TemporaryDirectory(prefix="VOID S6 LSP ") as temporary:
        root = Path(temporary).resolve()
        entry, owner = root / "entry.void", root / "modes.void"
        entry.write_bytes(main.encode())
        owner.write_bytes(modes.encode())
        with Client(server, entry) as client:
            assert not any(note["diagnostics"] for note in client.initialize())
            document = {"uri": entry.as_uri()}
            position = point(main, main.index(".ready") + 2)
            params = {"textDocument": document, "position": position}
            location = request(client, "textDocument/definition", params)
            assert location["uri"] == owner.as_uri() and location["range"]["start"] == point(modes, modes.index("ready =")), location
            hover = request(client, "textDocument/hover", params)
            assert "mode.ready = 7 (i8)" in hover["contents"]["value"], hover
            references = request(client, "textDocument/references", dict(params, context={"includeDeclaration": True}))
            assert {ref["uri"] for ref in references} == {entry.as_uri(), owner.as_uri()}, references

            assert not overlay(client, owner, modes, 1, opened=True)
            duplicate = modes.replace("gone = -128", "gone = 7")
            diagnostics = overlay(client, owner, duplicate, 2)
            error = next(item for item in diagnostics if "Duplicate numeric enum discriminant" in item["message"])
            assert error["range"]["start"] == point(duplicate, duplicate.rindex("7")), error
            assert request(client, "textDocument/definition", params) is None
            assert not overlay(client, owner, modes, 3)
            assert request(client, "textDocument/definition", params) == location

            bad = main.replace("raw as?", '"🎮" as?')
            diagnostics = overlay(client, entry, bad, 1, opened=True)
            error = next(item for item in diagnostics if "as? requires an integer" in item["message"])
            offset = bad.index("as?")
            assert error["range"] == {"start": point(bad, offset), "end": point(bad, offset + 3)}, error
            dirty = main.replace("m.mode.ready", "m.mode.")
            overlay(client, entry, dirty, 2)
            completion = request(client, "textDocument/completion", {"textDocument": document,
                "position": point(dirty, dirty.index("m.mode.") + len("m.mode."))})
            candidates = completion if isinstance(completion, list) else completion["items"]
            assert any(item["label"] == "gone" and "-128" in item["detail"] for item in candidates), candidates
            rough = main.replace("raw as? m.mode", "raw   as?    m.mode")
            assert not overlay(client, entry, rough, 3)
            edits = request(client, "textDocument/formatting", {"textDocument": document,
                "options": {"tabSize": 4, "insertSpaces": True}})
            assert edits[0]["newText"] == main, edits
            assert not overlay(client, entry, main, 4)
            assert request(client, "textDocument/definition", params) == location
            client.shutdown()
    print(f"S6 LSP passed {len(events)} recorded requests and overlay revisions")
finally:
    evidence.write_text(json.dumps(events, indent=2) + "\n", encoding="utf-8")
