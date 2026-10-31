"""URL's compiler-owned API docs, navigation and public completion inventory."""
from pathlib import Path
import os
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
os.environ["VOID_CRASH_EVIDENCE"] = str(ROOT / "docs/net/http/evidence/crashes")
sys.path.insert(0, str(ROOT / "tests/os"))
import support as h
from crash import install
install()
sys.path.insert(0, str(ROOT / "tools"))
from lsp_client import Client

server = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="VOID URL LSP ") as directory:
    entry = Path(directory) / "main.void"
    text = 'use url as weburl\nfunc main():\n    var parsed = weburl.parse("https://example.com/")\n'
    entry.write_text(text, encoding="utf-8")
    with Client(server, entry) as client:
        client.request("initialize", {"capabilities": {}})
        client.notify("initialized", {})
        client.sync()
        client.notify("textDocument/didOpen", {"textDocument": {"uri": entry.as_uri(), "languageId": "void", "version": 1, "text": text}})
        notes = client.sync()
        assert all(not note["diagnostics"] for note in notes), notes
        position = {"line": 2, "character": text.splitlines()[2].index("parse(") + 1}
        params = {"textDocument": {"uri": entry.as_uri()}, "position": position}
        hover = client.request("textDocument/hover", params)[0]
        assert "explicit HTTP(S)" in str(hover), hover
        definition = client.request("textDocument/definition", params)[0]
        assert "stdlib/url.void" in str(definition), definition
        denied = client.request("textDocument/rename", params | {"newName": "changed"})[0]
        assert "read-only" in str(denied), denied
        incomplete = "use url as weburl\nfunc main():\n    var output = weburl."
        client.notify("textDocument/didChange", {"textDocument": {"uri": entry.as_uri(), "version": 2}, "contentChanges": [{"text": incomplete}]})
        client.sync()
        response = client.request("textDocument/completion", {"textDocument": {"uri": entry.as_uri()},
            "position": {"line": 2, "character": len(incomplete.splitlines()[2])}})[0]["result"]
        items = response["items"] if isinstance(response, dict) else response
        names = {item["label"] for item in items}
        assert {"parse", "resolve", "normalize", "encode_component", "decode_component", "decode_bytes",
                "encode_query", "parse_query", "with_query", "address", "query_item", "error", "error_kind"} <= names, names
        assert not any(name.startswith("_") for name in names), names
        client.shutdown()
print("URL LSP import/docs/definition/rename/completion checks passed")
