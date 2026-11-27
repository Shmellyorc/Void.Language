"""Generator hover, completion and diagnostics at the language-server boundary."""
from pathlib import Path
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from lsp_client import Client
from test_output import output_directory

SOURCE = """generator func values() -> i32:
    yield 7

func main():
    var state = values()
    if state.next() is Some(var item):
        print(item)
    state.close()
"""

with tempfile.TemporaryDirectory(prefix="lsp-generator-", dir=output_directory()) as scratch:
    path = Path(scratch) / "main.void"
    path.write_text(SOURCE)
    with Client(Path(sys.argv[1]).resolve(), path) as client:
        client.request("initialize", {
            "initializationOptions": {"voidInteractivePresentation": True},
            "capabilities": {"textDocument": {"publishDiagnostics": {"versionSupport": True}}},
        })
        client.notify("initialized", {})

        def document(version):
            return {"uri": path.as_uri(), "version": version}

        def query(kind, text, prefix):
            before = text[:text.index(prefix) + len(prefix)]
            response, _ = client.request("textDocument/" + kind, {
                "textDocument": {"uri": path.as_uri()},
                "position": {"line": before.count("\n"), "character": len(before.rsplit("\n", 1)[-1])},
            })
            assert "error" not in response, response
            return response["result"]

        client.notify("textDocument/didOpen", {
            "textDocument": {**document(1), "languageId": "void", "text": SOURCE},
        })
        notes = client.sync()
        assert not any(d["severity"] == 1 for note in notes for d in note["diagnostics"]), notes
        assert "generator<i32>" in str(query("hover", SOURCE, "var st"))
        assert "until the next yield" in str(query("hover", SOURCE, "state.ne"))
        assert "reached cleanup" in str(query("hover", SOURCE, "state.cl"))
        assert "i32" in str(query("hover", SOURCE, "var it"))

        incomplete = SOURCE.replace("state.next()", "state.")
        client.notify("textDocument/didChange", {
            "textDocument": document(2), "contentChanges": [{"text": incomplete}],
        })
        client.sync()
        completion = query("completion", incomplete, "state.")
        items = completion["items"] if isinstance(completion, dict) else completion
        assert {"next", "close"} <= {item["label"] for item in items}, completion

        invalid = SOURCE.replace("yield 7", "yield true")
        client.notify("textDocument/didChange", {
            "textDocument": document(3), "contentChanges": [{"text": invalid}],
        })
        notes = client.sync()
        assert any("expected i32" in d["message"].lower() for note in notes for d in note["diagnostics"]), notes
        client.shutdown()
    assert client.process.returncode == 0 and not client.logs, client.logs
