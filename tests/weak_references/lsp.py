"""Exercise weak-reference editor behavior through the real language server."""
import os
from pathlib import Path
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from lsp_client import Client
from test_output import output_directory

SOURCE = """struct item:
    value: i32

func entry(mode: i32) -> i32:
    var target = item(value = mode)
    var reference: weak<item> = weak(target)
    if reference.lock() is Ok(var promoted):
        return promoted.value
    return 0
"""


def limits():
    import resource

    resource.setrlimit(resource.RLIMIT_STACK, (1048576, 1048576))
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


with tempfile.TemporaryDirectory(prefix="lsp-weak-", dir=output_directory()) as directory:
    path = Path(directory) / "main.void"
    path.write_text(SOURCE)
    with Client(Path(sys.argv[1]).resolve(), path, preexec_fn=limits if os.name == "posix" else None) as client:
        client.request("initialize", {
            "initializationOptions": {"voidInteractivePresentation": True},
            "capabilities": {"textDocument": {"publishDiagnostics": {"versionSupport": True}}},
        })
        client.notify("initialized", {})
        version = 0

        def update(source):
            global version
            version += 1
            document = {"uri": path.as_uri(), "version": version}
            if version == 1:
                document.update(languageId="void", text=source)
                client.notify("textDocument/didOpen", {"textDocument": document})
            else:
                client.notify("textDocument/didChange", {
                    "textDocument": document,
                    "contentChanges": [{"text": source}],
                })
            return [d for note in client.sync() if note.get("version") == version
                    for d in note["diagnostics"] if d.get("severity") == 1]

        def query(method, prefix, source=SOURCE):
            before = source[:source.index(prefix) + len(prefix)]
            response, _ = client.request("textDocument/" + method, {
                "textDocument": {"uri": path.as_uri()},
                "position": {"line": before.count("\n"), "character": len(before.rsplit("\n", 1)[-1])},
            })
            assert "error" not in response, response
            return response["result"]

        assert not update(SOURCE)
        creation = query("hover", "= we")
        assert "non-owning" in str(creation), creation
        promotion = query("hover", "reference.lo")
        assert "strong reference" in str(promotion) and "item?" in str(promotion), promotion
        incomplete = SOURCE.replace("reference.lock()", "reference.")
        update(incomplete)
        completion = query("completion", "reference.", incomplete)
        items = completion["items"] if isinstance(completion, dict) else completion
        assert any(item["label"] == "lock" for item in items), completion

        invalid = {
            SOURCE.replace("weak(target)", "weak(mode)"): "non-null concrete managed object",
            SOURCE.replace("weak<item>", "weak<i32>"): "non-null concrete managed target",
            SOURCE.replace("struct item:", "struct other:\n    value: i32\n\nstruct item:").replace(
                "reference: weak<item>", "reference: weak<other>"
            ): "weak<",
            SOURCE.replace("var target", "const target").replace(
                "        return promoted.value", "        promoted.value = 7\n        return promoted.value"
            ): "readonly",
        }
        for source, message in invalid.items():
            diagnostics = update(source)
            assert any(message in d["message"] for d in diagnostics), diagnostics
        assert not update(SOURCE)
        client.shutdown()
    assert client.process.returncode == 0 and not client.logs, client.logs
