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
## @param spec The custom format specification.
func (self record) format(spec: string) -> result<string, string>:
    return format(self.value, spec)
func entry(input: i32) -> i32:
    var item = record()
    return item.string().length as i32
'''
        assert not replace(hooks)
        definition = query("textDocument/definition", 9, 18)["result"]
        assert definition["range"]["start"]["line"] == 2, definition
        hover = query("textDocument/hover", 9, 18)["result"]["contents"]["value"]
        assert "string" in hover, hover
        replace(hooks.replace("item.string().length as i32", "item."))
        methods = completions(9, len("    return item."))
        assert "string" in methods and "format" in methods and "value" not in methods, methods
        assert not any("$display$" in name for name in methods), methods
        replace(hooks.replace("item.string().length as i32", "item.format("))
        signature = query("textDocument/signatureHelp", 9, len("    return item.format("))["result"]
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
        client.shutdown()
print("Display LSP primitive/hook/temporal hover, completion, navigation, signature, original hole spans and dirty repair passed")

# Console and composite intelligence uses the same compiler snapshots as execution.
import json
import os
checks = 0
records = []
def check(value, why):
    global checks
    assert value, why
    checks += 1

with tempfile.TemporaryDirectory(prefix="VOID composite tooling é😀 ") as directory:
    file = Path(directory) / "main.void"
    foreign = Path(directory) / "payload.void"
    foreign.write_text('''value struct box<t>:
    private secret: i32
    const value: t
## Message payload documentation.
enum message:
    ## Empty message.
    empty
    ## Data message in declared payload order.
    ## @param item The contained box.
    ## @param note The accompanying note.
    data(item: box<[i32]>, note: string)
''')
    source = '''use console
use "payload.void" as p
struct record:
    private secret: i32
func (self record) string() -> string:
    print("EDITOR MUST NEVER EXECUTE HOOKS")
    return "shown"
func inspect(const view: result<[i32], string>) -> string:
    if view is Ok(var captured):
        return "{captured}"
    return "error"
func entry(score: i32) -> i32:
    var fixed: [i32, 2] = [score, 2]
    var items = [score]
    var table: {string: [i32]} = {"é😀": items}
    var chosen = p.message.data(p.box<[i32]>(value = items), "é😀")
    var outcome: result<p.message, string> = Ok(chosen)
    var absent: record? = null
    var default_text = string(outcome)
    var aligned = format(chosen, "<12")
    var text = """é😀 {{brace}}:
        {string(table)} {chosen:} {outcome:}
    """
    print(fixed, items, table, chosen, outcome, absent, record(), end = "", flush = true)
    var written = console.stdout.write("é😀\\0")
    var received = console.stdin.read_line(timeout_ms = 0)
    if outcome is Ok(var selected):
        if selected is p.message . data(var boxed, var note):
            return boxed.value.count as i32
    return score
'''
    file.write_bytes(source.encode())
    uri = file.as_uri()
    with Client(server, file) as client:
        client.request("initialize", {"capabilities": {"textDocument": {
            "publishDiagnostics": {"versionSupport": True},
            "documentSymbol": {"hierarchicalDocumentSymbolSupport": True}},
            "workspace": {"workspaceEdit": {"documentChanges": True, "failureHandling": "textOnlyTransactional"}}}})
        client.notify("initialized", {}); client.sync()
        version = 0
        current = source
        def replace(text):
            global version, current
            current = text; version += 1
            params = {"textDocument": {"uri": uri, "version": version}}
            if version == 1:
                params["textDocument"].update(languageId="void", text=text)
            else:
                params["contentChanges"] = [{"text": text}]
            client.notify("textDocument/didOpen" if version == 1 else "textDocument/didChange", params)
            notes = client.sync()
            records.append({"version": version, "source": text, "diagnostics": notes})
            for n in notes:
                if n["uri"] == uri: check(n.get("version") == version, n)
            return [d for n in notes for d in n["diagnostics"]]
        def point(needle, delta=0):
            at = current.index(needle) + delta
            return {"line": current.count("\n", 0, at), "character": len(current[current.rfind("\n", 0, at)+1:at].encode("utf-16-le"))//2}
        def request(method, needle=None, delta=0, target=uri, **extra):
            params = {"textDocument": {"uri": target}, **extra}
            if needle is not None: params["position"] = point(needle, delta)
            response = client.request("textDocument/"+method, params)[0]
            records.append({"version": version, "method": method, "params": params, "response": response})
            return response
        def query(method, needle=None, delta=0, **extra):
            response = request(method, needle, delta, **extra)
            check("error" not in response, response)
            return response.get("result")
        def hover(needle, delta=0):
            out = query("hover", needle, delta); check(out, (needle, out))
            return out["contents"]["value"]
        def completion(needle):
            out = query("completion", needle, len(needle))
            return {item["label"]: item for item in (out.get("items", []) if isinstance(out, dict) else out)}
        def dirty_expression(expr):
            return replace(source.replace("    return score", "    var pending = "+expr+"\n    return score"))
        diagnostics = replace(source)
        check(not diagnostics, ("composite source", diagnostics))
        for needle, delta, line in [("p.message.data(p.box",10,10), ("message . data(var",10,10), ("boxed.value",7,2)]:
            definition = query("definition", needle, delta)
            check(definition and definition["uri"] == foreign.as_uri() and definition["range"]["start"]["line"] == line, definition)
        text = hover("p.message.data(p.box", 10)
        check("item:" in text and "note: string" in text and "Data message" in text, text)
        refs = query("references", "p.message.data(p.box", 10, context={"includeDeclaration": True})
        check(len(refs) == 3 and any(r["range"]["start"] == point("message . data(var", 10) for r in refs), refs)
        text = hover("Ok(chosen)", 1)
        check("Ok(value:" in text and "message" in text and "Ok(null)" in text, text)
        check(query("definition", "Ok(chosen)", 1) is None, "fabricated source for builtin Ok")
        check(len(query("references", "Ok(chosen)", 1, context={"includeDeclaration": False})) == 2, "result pattern references")
        check("i32" in hover("return boxed", 8) and "box<" in hover("return boxed", 8), "generic bound payload")
        check(query("definition", "return boxed", 8)["range"]["start"] == point("var boxed", 4), "pattern capture definition")
        check("*values" in hover("print(fixed", 1) and "$value" not in hover("print(fixed", 1) and "file: console.output" in hover("print(fixed", 1), "print public hover")
        for needle, offset, expect in [("stdout.write",8,"text: string"), ("stdin.read_line",7,"timeout_ms: i64")]:
            text = hover(needle, offset); check(expect in text and "Effects:" in text, text)
            definition = query("definition", needle, offset)
            check(definition and definition["uri"].endswith("/console.void"), definition)
            check("error" in request("rename", needle, offset, newName="renamed"), "installed method rename allowed")
            check("error" in request("formatting", target=definition["uri"], options={"tabSize":4,"insertSpaces":True}) or query("formatting", target=definition["uri"], options={"tabSize":4,"insertSpaces":True}) == [], "installed formatting allowed")
        hidden = client.request("workspace/symbol", {"query":"_reply"})[0]
        records.append({"method":"workspace/symbol", "response":hidden})
        check(hidden.get("result")==[], hidden)
        symbols = query("documentSymbol")
        check(all("$display$" not in str(s) and "$collection" not in str(s) for s in symbols), symbols)
        for expr, required, forbidden in [
            ("console.", {"stdin","stdout","stderr","color","text_read","key","terminal_session"}, {"_reply","_error","_console_write"}),
            ("console.stdout.", {"write","write_bytes","flush","capabilities","size","cursor_position","set_cursor","set_color","reset_color","clear","set_title","bell"}, {"_id","_reply","_stream"}),
            ("console.stderr.", {"write","flush","size"}, {"_id"}),
            ("console.stdin.", {"read_line","read_bytes","read_text","read_char","read_key","key_available","begin_mode","capabilities"}, {"_id"}),
            ("console.color.", {"black","red","white"}, {"_id"}),
            ("p.message.", {"empty","data"}, {"$tag","$data$item"}),
            ("items.", {"count","get","push","clear"}, set()),
            ("fixed.", {"count","get"}, {"push","clear","remove_at"}),
            ("table.", {"get","contains","count","clear"}, {"push"}),
            ("absent.", set(), {"push","get","count"})]:
            dirty_expression(expr); items = completion("var pending = "+expr)
            check(required <= items.keys() and not forbidden & items.keys(), (expr,items))
            check(all("$display$" not in name and not name.startswith("_console") for name in items), items)
        replace(source+'func session_help(session: console.terminal_session) -> i32:\n    var pending = session.\n    return 0\n')
        items = completion("var pending = session.")
        check("close" in items and "_token" not in items and "deinit" not in items, items)
        # Deep readonly follows the same pattern-bound authority used by execution.
        readonly = source.replace('return "{captured}"', 'return captured.')
        replace(readonly); items = completion("return captured.")
        check({"count","get"} <= items.keys() and not {"push","clear","insert","pop"} & items.keys(), items)
        # Signature help recovers incomplete ordinary calls without executing them.
        for expr, parameter, active in [("console.stdout.set_cursor(","column: i64",0), ("console.stdout.set_cursor(2, ","row: i64",1), ("console.stdin.read_line(","timeout_ms: i64",0), ("print(","*values",0), ("print(items, ","*values",0), ("print(items, flush = ","flush: bool",4), ("p.message.data(","item:",0), ("p.message.data(p.box<[i32]>(value = items), ","note: string",1)]:
            dirty_expression(expr)
            sig = query("signatureHelp", "var pending = "+expr, len("var pending = "+expr))
            check(sig and parameter in sig["signatures"][0]["label"] and sig["activeParameter"] == active, (expr,sig))
        replace(source.replace("Ok(chosen)", "Ok("))
        sig = query("signatureHelp", "= Ok(", len("= Ok("))
        check(sig and "value:" in sig["signatures"][0]["label"] and "message" in sig["signatures"][0]["label"], sig)
        for prefix, variant in [("", "Ok"), ("Err", "Err")]:
            replace(source.replace("    return score", "    var pending: result<p.message, string> = "+prefix+"\n    return score"))
            items = completion("var pending: result<p.message, string> = "+prefix)
            check(variant in items and "message" in items[variant]["detail"], items)
        for value in ["fixed","items","table","chosen","outcome","absent"]:
            text = source.replace('format(chosen, "<12")', f'format({value}, "")')
            check(not replace(text), value)
            items = completion(f'format({value}, "')
            check(set(items) == {"<12",">12","^12"}, (value,items))
        for value in ["chosen","outcome"]:
            check(not replace(source.replace("\n", "\r\n")), "CRLF source")
            items = completion("{"+value+":")
            check(set(items) == {"<12",">12","^12"}, items)
            check(query("definition", "{string(table)}", len("{string("))["range"]["start"] == point("var table",4), "Unicode multiline nested hole navigation")
        for value in ["fixed","items","table","chosen","outcome"]:
            bad = source.replace('format(chosen, "<12")', f'format({value}, "000")')
            diagnostics = replace(bad)
            check(any("format" in d["message"].lower() or "pattern" in d["message"].lower() for d in diagnostics), diagnostics)
            check(query("hover", "print(fixed", 1) is None, "stale invalid hover")
            check(query("formatting", options={"tabSize":4,"insertSpaces":True}) == [], "invalid format edited")
            check(not replace(source), "format repair")
        bad = source.replace("{chosen:}","{chosen:000}")
        diagnostics = replace(bad)
        check(any(d["range"]["start"]["line"] == point("chosen:000")["line"] for d in diagnostics), diagnostics)
        client.notify("textDocument/didChange", {"textDocument":{"uri":uri,"version":version-1},"contentChanges":[{"text":source}]})
        check(client.sync() == [], "stale version republished")
        check(query("hover","print(fixed",1) is None, "stale notification repaired source")
        check(not replace(source), "final dirty repair")
        pretty = query("formatting", options={"tabSize":4,"insertSpaces":True})
        if pretty: check(not replace(pretty[0]["newText"]), "formatted composite")
        check(query("formatting", options={"tabSize":4,"insertSpaces":True}) == [], "composite idempotence")
        file.write_text("func entry(score: i32) -> i32:\n    return missing_disk\n")
        client.request("void/refreshProject",{}); client.sync()
        check(query("definition","print(fixed",1) is None, "builtin print fabricated definition")
        check("*values" in hover("print(fixed",1), "refresh lost overlay")
        client.notify("textDocument/didClose",{"textDocument":{"uri":uri}})
        notes=client.sync(); check(any("missing_disk" in d["message"] for n in notes for d in n["diagnostics"]), notes)
        version+=1; current=source
        client.notify("textDocument/didOpen",{"textDocument":{"uri":uri,"languageId":"void","version":version,"text":source}})
        check(not [d for n in client.sync() for d in n["diagnostics"]], "close/reopen repair")
        check(query("definition","message . data(var",10)["uri"]==foreign.as_uri(), "reopen source target")
        clocks = "use time\nfunc entry(score: i32) -> i32:\n    var clock = now_utc()\n    return score\n"
        check(not replace(clocks), "clock metadata analysis")
        check("now_utc" in hover("now_utc()", 2), "clock metadata")
        check(query("definition", "now_utc()", 2)["uri"].endswith("/time.void"), "clock source navigation")
        client.shutdown()
        check(b"EDITOR MUST NEVER EXECUTE HOOKS" not in client.logs, "editor executed source hook")
if os.environ.get("VOID_STAGE10_TOOLING_RECORD"):
    Path(os.environ["VOID_STAGE10_TOOLING_RECORD"]).write_text(json.dumps({"checks":checks,"records":records},ensure_ascii=False,indent=2)+"\n")
print(f"{checks} console/composite LSP checks passed")
