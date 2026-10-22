"""Compiler-owned field policy on valid and dirty editor snapshots."""
from pathlib import Path
import sys
import tempfile
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from lsp_client import Client

server = Path(sys.argv[1]).resolve()
source = """struct box:
    private secret: i32 = 7
    const id: i32
func (self box) read() -> i32:
    return self.secret
func entry(input: i32) -> i32:
    var b = box()
    return b.id
"""
with tempfile.TemporaryDirectory(prefix="VOID Stage10 field editor ") as directory:
    file = Path(directory) / "main.void"
    file.write_text(source, encoding="utf-8")
    uri = file.as_uri()
    with Client(server, file) as client:
        client.request("initialize", {"capabilities": {}})
        client.notify("initialized", {})
        client.sync()
        version = 0

        def replace(text):
            global version
            version += 1
            if version == 1:
                client.notify("textDocument/didOpen", {"textDocument": {
                    "uri": uri, "languageId": "void", "version": version, "text": text}})
            else:
                client.notify("textDocument/didChange", {
                    "textDocument": {"uri": uri, "version": version},
                    "contentChanges": [{"text": text}]})
            return [d for n in client.sync() for d in n["diagnostics"]]

        def query(method, line, column):
            return client.request(method, {"textDocument": {"uri": uri},
                                           "position": {"line": line, "character": column}})[0]

        def completions(line, column):
            response = query("textDocument/completion", line, column)
            assert "error" not in response, response
            items = response["result"]
            return {item["label"]: item for item in (items.get("items", []) if isinstance(items, dict) else items)}

        assert not replace(source)
        hover = query("textDocument/hover", 7, 14)["result"]["contents"]["value"]
        assert "const id" in hover and "declared by" in hover, hover
        assert query("textDocument/definition", 7, 14)["result"]["range"]["start"]["line"] == 2
        replace(source.replace("return b.id", "return b."))
        outside = completions(7, len("    return b."))
        assert "id" in outside and "secret" not in outside, outside
        assert "const" in outside["id"]["detail"], outside
        replace(source.replace("return self.secret", "return self."))
        inside = completions(4, len("    return self."))
        assert "secret" in inside and "id" in inside, inside
        assert replace(source.replace("return b.id", "return b.secret"))
        assert query("textDocument/hover", 7, 15).get("result") is None
        assert replace(source.replace("return b.id", "b.id = 2\n    return 0"))
        assert not replace(source)
        assert query("textDocument/definition", 7, 14)["result"]
print("Field LSP privacy, const hover/completion, dirty rejection and repair passed")
