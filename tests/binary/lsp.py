"""Compiler-owned import, documentation, completion and signature help contract."""
from pathlib import Path
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from lsp_client import Client


server = Path(sys.argv[1]).resolve()
checks = 0


def check(value, message):
    global checks
    assert value, message
    checks += 1


def point(text, needle, delta=0):
    prefix = text[:text.index(needle) + delta]
    return {"line": prefix.count("\n"), "character": len(prefix.rsplit("\n", 1)[-1])}


with tempfile.TemporaryDirectory(prefix="VOID binary LSP ") as directory:
    entry = Path(directory) / "main.void"
    base = """use encoding/binary

func main():
    var bytes = binary.encode<i32>(123)
    var number = binary.decode<i32>(bytes, binary.byte_order.big)
"""
    entry.write_text(base, encoding="utf-8")

    with Client(server, entry) as client:
        client.request("initialize", {"capabilities": {"textDocument": {
            "signatureHelp": {"signatureInformation": {"documentationFormat": ["plaintext"]}}}}})
        client.notify("initialized", {})
        initial = client.sync()
        text = base
        version = 0

        def replace(updated):
            global version, text
            version += 1
            text = updated
            if version == 1:
                client.notify("textDocument/didOpen", {"textDocument": {
                    "uri": entry.as_uri(), "languageId": "void", "version": version, "text": text}})
            else:
                client.notify("textDocument/didChange", {"textDocument": {
                    "uri": entry.as_uri(), "version": version}, "contentChanges": [{"text": text}]})
            return client.sync()

        def query(method, needle, delta=0, **extra):
            response = client.request("textDocument/" + method, {
                "textDocument": {"uri": entry.as_uri()},
                "position": point(text, needle, delta), **extra})[0]
            check("error" not in response, response)
            return response["result"]

        notes = initial + replace(base)
        check(all(not note["diagnostics"] for note in notes), notes)
        for name, description in (("encode", "fresh owned byte array"),
                                  ("decode", "without modifying or retaining")):
            hover = query("hover", f"binary.{name}", 8)["contents"]["value"]
            check(description in hover and "Returns:" in hover, hover)
            check("@param" not in hover and "@return" not in hover, hover)
            definition = query("definition", f"binary.{name}", 8)
            check(definition and "encoding/binary.void" in str(definition), definition)

        replace(base[:base.index("    var bytes")] + "    var bytes = binary.")
        response = query("completion", "binary.", len("binary."))
        items = response["items"] if isinstance(response, dict) else response
        by_name = {item["label"]: item for item in items}
        check({"encode", "decode", "byte_order", "error"} <= by_name.keys(), by_name)
        check(not any(name.startswith("_binary") for name in by_name), by_name)
        for name in ("encode", "decode"):
            check("Returns:" in by_name[name]["documentation"], by_name[name])

        replace("use encoding/")
        response = query("completion", "encoding/", len("encoding/"))
        imports = response["items"] if isinstance(response, dict) else response
        check(any("binary" in item["label"] for item in imports), imports)

        pending = base[:base.index("    var bytes")]
        for call, active, expected in (
            ("binary.encode<i32>(", 0, "Numeric value"),
            ("binary.encode<i32>(123, ", 1, "Byte order"),
            ("binary.decode<i32>(", 0, "Complete representation"),
            ("binary.decode<i32>([1, 0, 0, 0], order = ", 1, "Byte order"),
        ):
            replace(pending + "    var pending = " + call)
            for context in ({}, {"context": {"triggerKind": 1}},
                            {"context": {"triggerKind": 2, "triggerCharacter": ","}}):
                signature = query("signatureHelp", call, len(call), **context)
                check(signature and signature["activeParameter"] == active, (call, signature))
                shown = signature["signatures"][0]
                check(expected in shown["parameters"][active]["documentation"]["value"], shown)
                check("documentation" not in shown, shown)
                check(all("documentation" not in parameter for index, parameter in
                          enumerate(shown["parameters"]) if index != active), shown)

        replace(base)
        hover = query("hover", "binary.byte_order.big", len("binary.byte_order.") + 1)
        check("most significant byte first" in str(hover), hover)
        check(all(not note["diagnostics"] for note in replace(base)), "final diagnostics were not empty")
        client.shutdown()

print(f"Binary LSP: {checks} import, completion, navigation, hover and active-parameter checks passed")
