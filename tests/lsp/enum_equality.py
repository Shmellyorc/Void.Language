"""Publish typed enum guidance for the current document and project revision."""

from pathlib import Path
import sys
from tempfile import TemporaryDirectory

from editor_recovery import Client

MANIFEST = """schema=1
[project]
id="org.example.enum"
name="Enum guidance"
version="1.0.0"
kind="application"
entry="src/main.void"
[application]
id="org.example.enum.app"
[sources]
roots=["src"]
"""
SOURCE = """use console
func main() -> i32:
    var pressed = console.key.up
    if "雪😀" == "雪😀" and pressed is console.key.up:
        return 0
    return 1
"""
BAD_CASE = SOURCE.replace("pressed is", "pressed ==")
BAD_VALUES = SOURCE.replace("pressed is console.key.up", "pressed == pressed")
BAD_NEGATED = SOURCE.replace("pressed is", "pressed !=")
MATCHED = """use console
func main() -> i32:
    match console.key.up:
        console.key.up:
            return 0
        _:
            return 1
"""
ERROR = "Tagged enum values do not support built-in '==' or '!='."
CASE = "Use 'is' to test this enum case, or 'match' to handle multiple cases."
NEGATED_CASE = "Use a negated 'is' test to exclude this enum case, or 'match' to handle multiple cases."
MATCH = "Use 'match' to inspect the cases and compare any payloads explicitly."


def open_document(client, uri, text):
    client.notify("textDocument/didOpen", {"textDocument": {
        "uri": uri, "languageId": "void", "version": 1, "text": text,
    }})


def change(client, uri, text, version):
    client.notify("textDocument/didChange", {"textDocument": {"uri": uri, "version": version},
                                            "contentChanges": [{"text": text}]})


def expect(notes, uri, version, diagnostics):
    matching = [note for note in notes if note["uri"] == uri]
    assert matching, notes
    assert matching[-1].get("version") == version, matching
    assert matching[-1]["diagnostics"] == diagnostics, matching
    return matching


def diagnostic(text, operator, hint):
    line = text.splitlines()[3]
    start = len(line[:line.index("pressed " + operator) + len("pressed ")].encode("utf-16-le")) // 2
    return {"range": {"start": {"line": 3, "character": start}, "end": {"line": 3, "character": start + 2}},
            "severity": 1, "code": "VOID2016", "source": "VOID", "message": ERROR + "\nHint: " + hint}


def select(client, entry, status):
    response, notes = client.request("void/reselectProject", {"entry": entry})
    assert response["result"]["status"] == status, response
    if status == "ready":
        assert response["result"]["entry"].endswith("/" + entry), response
    return notes


with TemporaryDirectory(prefix="VOID enum LSP 雪 ") as directory:
    root = Path(directory)
    src = root / "src"
    src.mkdir()
    main, other = src / "main.void", src / "other.void"
    main.write_bytes(SOURCE.encode())
    other.write_bytes(BAD_NEGATED.encode())
    (root / "void.project.toml").write_bytes(MANIFEST.encode())
    uri, other_uri = main.as_uri(), other.as_uri()

    with Client(Path(sys.argv[1]).resolve(), root, project=True) as client:
        response, _ = client.request("initialize", {
            "capabilities": {"general": {"positionEncodings": ["utf-16"]},
                             "textDocument": {"publishDiagnostics": {"versionSupport": True}}},
            "initializationOptions": {"voidGuardedFixes": True},
        })
        caps = response["result"]["capabilities"]
        assert caps["positionEncoding"] == "utf-16" and caps["codeActionProvider"]["codeActionKinds"] == ["quickfix"], caps
        client.notify("initialized", {})
        client.sync()
        open_document(client, uri, BAD_CASE)
        case = diagnostic(BAD_CASE, "==", CASE)
        expect(client.sync(), uri, 1, [case])
        response, _ = client.request("textDocument/codeAction", {
            "textDocument": {"uri": uri}, "range": case["range"],
            "context": {"diagnostics": [case], "only": ["quickfix"]},
        })
        assert response["result"] == [], response
        assert main.read_bytes() == SOURCE.encode()
        change(client, uri, SOURCE, 2)
        expect(client.sync(), uri, 2, [])
        change(client, uri, BAD_VALUES, 3)
        expect(client.sync(), uri, 3, [diagnostic(BAD_VALUES, "==", MATCH)])

        change(client, uri, BAD_CASE, 4)
        change(client, uri, BAD_VALUES, 5)
        change(client, uri, MATCHED, 6)
        revisions = expect(client.sync(), uri, 6, [])
        versions = [note["version"] for note in revisions]
        assert versions == sorted(versions) and set(versions) <= {4, 5, 6}, revisions
        change(client, uri, BAD_CASE, 5)
        assert not client.sync(), "An older edit published new diagnostics"
        client.notify("textDocument/didClose", {"textDocument": {"uri": uri}})
        assert all(not note["diagnostics"] for note in client.sync())

        notes = select(client, "src/other.void", "error")
        expect(notes, other_uri, None, [diagnostic(BAD_NEGATED, "!=", NEGATED_CASE)])
        assert all(not note["diagnostics"] for note in notes if note["uri"] == uri), notes
        notes = select(client, "src/main.void", "ready")
        expect(notes, other_uri, None, [])
        assert all(not note["diagnostics"] for note in notes), notes
        client.shutdown()

print("Enum guidance preserved UTF-16 spans, current revisions and project-entry ownership")
