"""Exercise compiler-owned memory stream documentation over the actual LSP pipe."""
from pathlib import Path
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from lsp_client import Client


server = Path(sys.argv[1]).resolve()
checks = 0


def check(valid, message):
    global checks
    assert valid, message
    checks += 1


def point(text, needle, delta=0):
    prefix = text[:text.index(needle) + delta]
    return {"line": prefix.count("\n"), "character": len(prefix.rsplit("\n", 1)[-1])}


with tempfile.TemporaryDirectory(prefix="VOID memory LSP ") as directory:
    entry = Path(directory) / "main.void"
    (entry.parent / "api.void").write_text('''struct packet:
    value: i32

## An unused generic method whose dependent body must stay unchecked during completion.
## @param value Input used only if the caller specializes the method.
## @return The specialized value.
func (self packet) echo<t>(value: t) -> t:
    return unknown_until_specialized(value)

private func (self packet) hidden<t>(value: t) -> t:
    return value
''', encoding="utf-8")
    (entry.parent / "wrapper.void").write_text('''use "./api.void" as api

## Construct a packet without reexporting its module's methods.
## @return A packet owned by the directly imported API module.
func make() -> api.packet:
    return api.packet(value = 1)
''', encoding="utf-8")
    base = '''use io/memory
use encoding/binary

func main():
    var buffer = memory.stream([1, 2, 3, 4], binary.byte_order.big)
    var number = buffer.read<i32>()
    var sent = buffer.write<i64>(-1)
    var position = buffer.position
    var exact = buffer.read_exact(4)
    var text = buffer.read_pascal_string(1024)
'''
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
            result = client.request("textDocument/" + method, {
                "textDocument": {"uri": entry.as_uri()},
                "position": point(text, needle, delta), **extra})[0]
            check("error" not in result, result)
            return result["result"]

        notes = initial + replace(base)
        check(all(not note["diagnostics"] for note in notes), notes)
        for needle, delta, expected in (
            ("buffer.read<i32>", 8, "encoding/binary"),
            ("buffer.write<i64>", 8, "encoding/binary"),
            ("buffer.read_exact", 8, "short buffer"),
            ("buffer.read_pascal_string", 8, "u32-byte-length-prefixed"),
            ("buffer.position", 8, "Absolute byte position"),
        ):
            shown = query("hover", needle, delta)["contents"]["value"]
            check(expected in shown and "@param" not in shown and "@return" not in shown, shown)
            definition = query("definition", needle, delta)
            check(definition and "io/memory.void" in str(definition), definition)

        header = base[:base.index("    var number")]
        replace(header + "    var selected = buffer.")
        result = query("completion", "buffer.", len("buffer."))
        items = result["items"] if isinstance(result, dict) else result
        names = {item["label"]: item for item in items}
        public = {"position", "length", "remaining", "eof", "order", "seek", "to_bytes",
                  "read_bytes", "read_exact", "write_bytes", "read", "write", "read_string",
                  "write_string", "read_pascal_string", "write_pascal_string"}
        check(public <= names.keys(), names)
        check(not any(name.startswith("_") for name in names), names)
        check("Returns:" in names["write_bytes"]["documentation"], names["write_bytes"])

        replace("use io/")
        result = query("completion", "io/", len("io/"))
        items = result["items"] if isinstance(result, dict) else result
        check(any("memory" in item["label"] for item in items), items)

        calls = [
            ("buffer.seek(", 0, "New absolute byte position"),
            ("buffer.read_bytes(", 0, "Maximum bytes to copy"),
            ("buffer.read_exact(", 0, "Required byte count"),
            ("buffer.write_bytes(", 0, "Readonly or mutable input bytes"),
            ("buffer.write<i32>(", 0, "Numeric value"),
            ("buffer.read_string(", 0, "UTF-8 bytes to consume"),
            ("buffer.write_string(", 0, "Strict Unicode text"),
            ("buffer.read_pascal_string(", 0, "Maximum payload bytes"),
            ("buffer.write_pascal_string(", 0, "Text whose UTF-8 byte length"),
        ]
        for call, active, expected in calls:
            replace(header + "    var pending = " + call)
            for context in ({}, {"context": {"triggerKind": 1}},
                            {"context": {"triggerKind": 2, "triggerCharacter": "("}}):
                signature = query("signatureHelp", call, len(call), **context)
                check(signature and signature["activeParameter"] == active, (call, signature))
                shown = signature["signatures"][0]
                check(expected in shown["parameters"][active]["documentation"]["value"], shown)
                check("documentation" not in shown, shown)

        for call, active, expected in (
            ("memory.stream(", 0, "Initial data"),
            ("memory.stream([], order = ", 1, "Numeric and Pascal-prefix byte order"),
        ):
            replace(base[:base.index("    var buffer")] + "    var pending = " + call)
            signature = query("signatureHelp", call, len(call))
            check(signature and signature["activeParameter"] == active, signature)
            shown = signature["signatures"][0]
            check(expected in shown["parameters"][active]["documentation"]["value"], shown)
            check(all("documentation" not in value for index, value in enumerate(shown["parameters"])
                      if index != active), shown)

        replace(header + "    var pending = buffer.read<i32>(")
        check(query("signatureHelp", "buffer.read<i32>(", len("buffer.read<i32>(")) is None,
              "parameterless generic read should not invent active-parameter documentation")

        for prefix, binding, visible in (
            ('use "./api.void" as api\n', 'var item = api.packet(value = 1)', True),
            ('use "./api.void" as api\n', 'const item = api.packet(value = 1)', False),
            ('use "./wrapper.void" as wrapper\n', 'var item = wrapper.make()', False),
        ):
            replace(prefix + '\nfunc main():\n    ' + binding + '\n    var selected = item.')
            result = query("completion", "item.", len("item."))
            items = result["items"] if isinstance(result, dict) else result
            names = {item["label"]: item for item in items}
            check(("echo" in names) == visible, names)
            check("hidden" not in names, names)
            if visible:
                check("Returns: The specialized value." in names["echo"]["documentation"], names)

        check(all(not note["diagnostics"] for note in replace(base)), "repaired source has diagnostics")
        client.shutdown()

print(f"Memory LSP: {checks} completion, navigation, hover, constructor/method signature and documentation checks passed")
