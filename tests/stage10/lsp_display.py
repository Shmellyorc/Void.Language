"""Compiler-owned display metadata on valid and dirty editor snapshots."""
from pathlib import Path
import sys
import tempfile
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from lsp_client import Client

server = Path(sys.argv[1]).resolve()
source = '''const shown = string(2.125d)
func entry(input: i32) -> i32:
    var text = "Score: {input:000}"
    var value = format(input, "000")
    if value is Ok(var formatted):
        return formatted.length as i32
    return 0
'''
with tempfile.TemporaryDirectory(prefix="VOID Stage10 display editor ") as directory:
    file = Path(directory) / "main.void"
    file.write_text(source, encoding="utf-8")
    uri = file.as_uri()
    with Client(server, file) as client:
        client.request("initialize", {"capabilities": {"workspace": {"workspaceEdit": {"documentChanges": True, "failureHandling": "textOnlyTransactional"}}}})
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
        renamed = client.request("textDocument/rename", {"textDocument": {"uri": uri},
            "position": {"line": 1, "character": 14}, "newName": "score"})[0]
        assert renamed.get("result"), renamed
        hover = query("textDocument/hover", 0, 15)["result"]["contents"]["value"]
        assert "string" in hover and "invariant" in hover.lower(), hover
        hover = query("textDocument/hover", 3, 18)["result"]["contents"]["value"]
        assert "format" in hover and "spec" in hover, hover
        # The hole retains original expression spans for references and rename.
        definition = query("textDocument/definition", 2, 26)["result"]
        assert definition["range"]["start"]["line"] == 1, definition
        replace(source.replace('format(input, "000")','format(input, '))
        signature=query("textDocument/signatureHelp",3,len('    var value = format(input, '))["result"]
        assert signature and "spec" in signature["signatures"][0]["label"], signature
        diagnostics=replace(source.replace('{input:000}', '{input:0.#0}'))
        assert any("Required fraction" in d["message"] and d["range"]["start"]["line"]==2 for d in diagnostics), diagnostics
        assert not replace(source)
        assert query("textDocument/definition", 2, 26)["result"]
        hooks = '''struct record:
    private const value: i32 = 4
func (self record) string() -> string:
    return "{self.value}"
func (self record) format(spec: string) -> result<string, string>:
    return format(self.value, spec)
func entry(input: i32) -> i32:
    var item = record()
    return item.string().length as i32
'''
        assert not replace(hooks)
        definition = query("textDocument/definition", 8, 18)["result"]
        assert definition["range"]["start"]["line"] == 2, definition
        hover = query("textDocument/hover", 8, 18)["result"]["contents"]["value"]
        assert "string" in hover, hover
        replace(hooks.replace("item.string().length as i32", "item."))
        methods = completions(8, len("    return item."))
        assert "string" in methods and "format" in methods and "value" not in methods, methods
        assert not any("$display$" in name for name in methods), methods
        replace(hooks.replace("item.string().length as i32", "item.format("))
        signature = query("textDocument/signatureHelp", 8, len("    return item.format("))["result"]
        assert signature and "spec" in signature["signatures"][0]["label"], signature
        assert not replace(hooks)
        temporal = '''use time
func entry(input: i32) -> i32:
    if make_date(2024, 1, 1) is Ok(var day):
        return day.year as i32
    return input
'''
        assert not replace(temporal)
        hover = query("textDocument/hover", 2, 11)["result"]["contents"]["value"]
        assert "make_date" in hover and "i64" in hover and "date" in hover, hover
        definition = query("textDocument/definition", 2, 11)["result"]
        assert definition and definition["uri"].endswith("/time.void"), definition
        replace(temporal.replace("return day.year as i32", "return day."))
        members = completions(3, len("        return day."))
        assert "year" in members and "month" in members and "day" in members and "_year" not in members, members
        replace("use time\nfunc entry(input: i32) -> i32:\n    var value = make_date(2024, \n    return input\n")
        signature=query("textDocument/signatureHelp",2,len("    var value = make_date(2024, "))["result"]
        assert signature and "month" in signature["signatures"][0]["label"], signature
        replace(temporal.replace("make_date(2024, 1, 1)", "make_date(2024, "))
        signature=query("textDocument/signatureHelp",2,len("    if make_date(2024, "))["result"]
        assert signature and "month" in signature["signatures"][0]["label"], signature
        replace("use time\nfunc entry(input: i32) -> i32:\n    var value = make_date(2024, # pending month\n    return input\n")
        signature=query("textDocument/signatureHelp",2,len("    var value = make_date(2024, "))["result"]
        assert signature and "month" in signature["signatures"][0]["label"], signature
        diagnostics=replace(temporal.replace("return day.year as i32", 'var invalid = "{day:HH}"\n        return input'))
        assert any("does not apply" in d["message"] and d["range"]["start"]["line"]==3 for d in diagnostics), diagnostics
        assert not replace(temporal)
print("Display LSP primitive/hook/temporal hover, completion, navigation, signature, original hole spans and dirty repair passed")
