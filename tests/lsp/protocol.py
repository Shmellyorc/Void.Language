"""Portable process-level LSP acceptance, using actual compiler source projects."""
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "tools"))
from lsp_client import Client, frame


def check_error(response, code):
    assert response["error"]["code"] == code, response


def diagnostics(notes):
    return {n["uri"]: n for n in notes}


def source_position(text, needle, start=0):
    offset = text.index(needle, start)
    before = text[:offset]
    line = before.count("\n")
    tail = before.rsplit("\n", 1)[-1]
    return {"line": line, "character": len(tail.encode("utf-16-le")) // 2}


def project(root):
    root.mkdir(parents=True, exist_ok=True)
    main = '# é😀 "quoted" \\ braces {}[]\r\nuse "/worker space.void" as worker\r\nuse "/values.void" as values\r\nconst root_bonus = 2\r\nfunc entry(x: i32) -> i32:\r\n    return worker.calculate(x) + values.base\r\nfunc extra(x: i32) -> i32:\r\n    return worker.calculate(x)\r\n'
    worker = 'use "/values.void" as values\r\nfunc calculate(x: i32) -> i32:\r\n    const stable = x + root_bonus + values.base\r\n    return stable + 1\r\n'
    values = 'const base: i32 = 40\r\n'
    for name, text in [("main.void", main), ("worker space.void", worker), ("values.void", values)]:
        (root / name).write_bytes(text.encode("utf-8"))
    return main, worker, values


def editor(executable, root):
    main, worker, values = project(root)
    entry = root / "main.void"
    uri, wuri, vuri = entry.as_uri(), (root / "worker space.void").as_uri(), (root / "values.void").as_uri()
    q = {"textDocument": {"uri": uri}, "position": source_position(main, "calculate")}
    with Client(executable, entry) as c:
        initial = c.initialize()
        assert set(diagnostics(initial)) == {uri, wuri, vuri} and all(not n["diagnostics"] for n in initial)
        def opened(url, text, version=0):
            c.notify("textDocument/didOpen", {"textDocument": {"uri": url, "languageId": "void", "version": version, "text": text}})
            return diagnostics(c.sync())
        def changed(url, text, version):
            c.notify("textDocument/didChange", {"textDocument": {"uri": url, "version": version}, "contentChanges": [{"text": text}]})
            return diagnostics(c.sync())
        assert opened(uri, main, -5)[uri]["version"] == -5
        response, _ = c.request("textDocument/definition", q, 'id " \\ é😀\n')
        assert response["result"]["uri"] == wuri, response
        assert response["result"]["range"] == {"start": {"line": 1, "character": 5}, "end": {"line": 1, "character": 14}}
        response, _ = c.request("textDocument/references", {**q, "context": {"includeDeclaration": False}}, 0)
        assert len(response["result"]) == 2 and all(x["uri"] == uri for x in response["result"])
        response, _ = c.request("textDocument/references", {**q, "context": {"includeDeclaration": True}}, -2147483648)
        assert len(response["result"]) == 3
        # Query at a declaration uses compiler spans, not a text scanner.
        response, _ = c.request("textDocument/references", {"textDocument": {"uri": wuri},
            "position": source_position(worker, "calculate"), "context": {"includeDeclaration": False}}, 2147483647)
        assert len(response["result"]) == 2
        for url, text, symbol, expected in [(wuri, worker, "root_bonus", uri), (wuri, worker, "base", vuri),
                                          (wuri, worker, "stable + 1", wuri)]:
            response, _ = c.request("textDocument/definition", {"textDocument": {"uri": url}, "position": source_position(text, symbol)})
            assert response["result"]["uri"] == expected, response
        for point in [source_position(main, "x: i32"), {"line": 0, "character": 2}, {"line": 0, "character": 3}, {"line": 5, "character": 10000}]:
            response, _ = c.request("textDocument/definition", {"textDocument": {"uri": uri}, "position": point})
            assert response["result"] is None, response
        # Emoji begins after '# é' (three UTF16 units), surrogate interior is four.
        check_error(c.request("textDocument/definition", {"textDocument": {"uri": uri}, "position": {"line": 0, "character": 4}})[0], -32602)
        for point in [{"line": 999, "character": 0}, {"line": -1, "character": 0}, {"line": 1, "character": 0.5}]:
            check_error(c.request("textDocument/definition", {"textDocument": {"uri": uri}, "position": point})[0], -32602)
        check_error(c.request("textDocument/references", q)[0], -32602)
        # Multiple imported unsaved documents. Invalid source invalidates all queries.
        bad_worker = worker.replace("return stable + 1", "return missing")
        notes = opened(wuri, bad_worker, -10)
        d = notes[wuri]["diagnostics"][0]
        assert notes[wuri]["version"] == -10 and d["code"] == "VOID2002" and d["severity"] == 1 and d["source"] == "VOID"
        assert d["range"] == {"start": {"line": 3, "character": 11}, "end": {"line": 3, "character": 18}}, d
        assert c.request("textDocument/definition", q)[0]["result"] is None
        assert c.request("textDocument/references", {**q, "context": {"includeDeclaration": True}})[0]["result"] == []
        # Duplicate/stale notifications have no response and no diagnostic republish.
        assert changed(wuri, worker, -10) == {}
        assert changed(wuri, worker, -11) == {}
        assert c.request("textDocument/definition", q)[0]["result"] is None
        opened(vuri, values.replace("40", "41"), 7)
        notes = changed(wuri, worker.replace("stable + 1", "stable + 9"), -9)
        assert notes[wuri]["version"] == -9 and not notes[wuri]["diagnostics"]
        assert c.request("textDocument/definition", q)[0]["result"]["uri"] == wuri
        # Duplicate open, incremental patch and out-of-root URI never mutate source.
        assert opened(wuri, bad_worker, 200) == {}
        c.notify("textDocument/didChange", {"textDocument": {"uri": wuri, "version": -8}, "contentChanges": [{"range": {"start": {"line": 0, "character": 0}, "end": {"line": 0, "character": 1}}, "text": "bad"}]})
        assert c.sync() == []
        outside = root.parent / "outside.void"
        check_error(c.request("textDocument/definition", {**q, "textDocument": {"uri": outside.as_uri()}})[0], -32602)
        # Duplicate local declaration carries a related source location.
        duplicate = worker.replace("    return stable + 1", "    const stable = x\r\n    return stable + 1")
        notes = changed(wuri, duplicate, -8)
        related = [d for d in notes[wuri]["diagnostics"] if d["code"] == "VOID2005"]
        assert related and related[0]["relatedInformation"][0]["location"]["uri"] == wuri, notes
        changed(wuri, worker, -7)
        # Unicode import path is rejected by the existing portable VFS policy,
        # but its diagnostic range counts surrogate units, not UTF8 bytes.
        unicode_import = 'use "/é😀.void"\r\nfunc entry(x: i32) -> i32:\r\n    return x\r\n'
        notes = changed(uri, unicode_import, -4)
        diagnostic = [d for d in notes[uri]["diagnostics"] if d["code"] == "VOID3001"][0]
        assert diagnostic["range"]["start"] == {"line": 0, "character": 4}
        assert diagnostic["range"]["end"] == {"line": 0, "character": 15}, diagnostic
        # New unsaved modules, moved diagnostics, graph removal and clearing.
        virtual_uri = (root / "new" / "virtual.void").as_uri()
        graph = 'use "/new/virtual.void" as v\r\nfunc entry(x: i32) -> i32:\r\n    return v.calculate(x)\r\n'
        changed(uri, graph, -3)
        notes = opened(virtual_uri, 'func calculate(x: i32) -> i32:\r\n    return missing\r\n', 0)
        assert notes[virtual_uri]["diagnostics"][0]["code"] == "VOID2002"
        changed(virtual_uri, 'func calculate(x: i32) -> i32:\r\n    return x\r\n', 1)
        virtual_query = {"textDocument": {"uri": uri}, "position": source_position(graph, "calculate")}
        assert c.request("textDocument/definition", virtual_query)[0]["result"]["uri"] == virtual_uri
        c.notify("textDocument/didClose", {"textDocument": {"uri": virtual_uri}})
        notes = diagnostics(c.sync())
        assert any(d["code"] == "VOID3002" for d in notes[uri]["diagnostics"])
        assert c.request("textDocument/definition", virtual_query)[0]["result"] is None
        opened(virtual_uri, 'func calculate(x: i32) -> i32:\r\n    return x\r\n', 0)
        # Failed graph with virtual error, followed by removing that import clears it.
        changed(virtual_uri, 'func calculate(x: i32) -> i32:\r\n    return missing\r\n', 2)
        notes = changed(uri, main, -2)
        assert not notes[virtual_uri]["diagnostics"]
        assert c.request("textDocument/definition", q)[0]["result"]["uri"] == wuri
        c.notify("textDocument/didClose", {"textDocument": {"uri": virtual_uri}})
        c.sync()
        c.notify("textDocument/didClose", {"textDocument": {"uri": wuri}})
        notes = diagnostics(c.sync())
        assert "version" not in notes[wuri] and not notes[wuri]["diagnostics"]
        # LSP versions reset on reopen; internal session revisions keep increasing.
        notes = opened(wuri, bad_worker, -2147483648)
        assert notes[wuri]["version"] == -2147483648 and notes[wuri]["diagnostics"]
        changed(wuri, worker, -2147483647)
        # Mutable locals are not fabricated symbols.
        mutable = main.replace("return worker.calculate(x) + values.base", "var y = x\r\n    return y")
        changed(uri, mutable, -1)
        response, _ = c.request("textDocument/definition", {"textDocument": {"uri": uri}, "position": source_position(mutable, "return y") | {"character": 11}})
        assert response["result"] is None
        changed(uri, main, 0)
        # Self/forward cycles retain real compiler references and terminate queries.
        cycle = 'use "/main.void" as m\r\nfunc calculate(x: i32) -> i32:\r\n    if x < 0:\r\n        return m.entry(x)\r\n    return later(x)\r\nfunc later(x: i32) -> i32:\r\n    return x\r\n'
        changed(wuri, cycle, -2147483646)
        response, _ = c.request("textDocument/definition", {"textDocument": {"uri": wuri}, "position": source_position(cycle, "later(x)")})
        assert response["result"]["range"]["start"]["line"] == 5
        changed(wuri, 'func calculate(x: i32) -> i32', -2147483645)
        notes = diagnostics(c.sync())
        # Re-publish via a fresh version; the parser's missing-colon EOF span is empty.
        notes = changed(wuri, 'func calculate(x: i32) -> i32', -2147483644)
        assert any(d["range"]["start"] == d["range"]["end"] for d in notes[wuri]["diagnostics"])
        changed(wuri, cycle.replace("\r\n", "\r"), -2147483643)
        direct = main.replace(' as worker', '').replace('worker.calculate', 'calculate')
        changed(uri, direct, 1)
        direct_query = {"textDocument": {"uri": uri}, "position": source_position(direct, "calculate")}
        assert c.request("textDocument/definition", direct_query)[0]["result"]["uri"] == wuri
        # Multiple complete replacement events, and a zero-event logical revision.
        c.notify("textDocument/didChange", {"textDocument": {"uri": uri, "version": 2}, "contentChanges": [{"text": "invalid"}, {"text": direct}]})
        assert all(not n["diagnostics"] for n in c.sync())
        c.notify("textDocument/didChange", {"textDocument": {"uri": uri, "version": 3}, "contentChanges": []})
        assert all(not n["diagnostics"] for n in c.sync())
        c.shutdown()
    print("editor: CRLF/UTF16, cross-file symbols, overlays, versions, invalid/repair, graph changes and cycles passed")


def transport(executable, root):
    entry = root / "main.void"
    with Client(executable, entry) as c:
        check_error(c.request("textDocument/definition", {})[0], -32002)
        c.notify("textDocument/didOpen", {})  # Pre-initialize notification must not reply.
        request = {"jsonrpc": "2.0", "id": "split", "method": "initialize", "params": {"capabilities": {}}}
        c.write(frame(request), fragmented=True)
        assert c.response("split")[0]["result"]["capabilities"]["positionEncoding"] == "utf-16"
        c.notify("initialized", {})
        c.sync()
        check_error(c.request("exit")[0], -32600)
        c.notify("unknown/notification", {"text": 'escaped " \\ 😀 {}['})
        # Multiple messages in a single write, with exact numeric/string identities.
        c.write(frame({"jsonrpc": "2.0", "id": 11, "method": "unknown/a"}) + frame({"jsonrpc": "2.0", "id": "11", "method": "unknown/b"}))
        check_error(c.response(11)[0], -32601)
        check_error(c.response("11")[0], -32601)
        for body in [b'{bad json', b'', b'"scalar"', b'[]', b'{"jsonrpc":"1.0","method":"bad","id":1}',
                     b'{"jsonrpc":"2.0","id":true,"method":"bad"}', b'{"jsonrpc":"2.0","id":9223372036854775808.0,"method":"bad"}',
                     b'{"jsonrpc":"2.0","id":1.5,"method":"bad"}', b'{"jsonrpc":"2.0","id":null,"method":"bad"}',
                     b'{"jsonrpc":"2.0","method":3}', b'{"jsonrpc":"2.0","method":"bad","id":{}}', b'[' * 129 + b']' * 129,
                     b'{"jsonrpc":"2.0","method":"bad","id":"\xff"}']:
            c.write(b"Content-Length: " + str(len(body)).encode() + b"\r\n\r\n" + body)
            response = c.receive()
            assert response["error"]["code"] in [-32700, -32600], response
        # LLVM JSON normalizes an unpaired escaped surrogate to U+FFFD.
        body = b'{"jsonrpc":"2.0","method":"bad","id":"\\ud800"}'
        c.write(b"Content-Length: " + str(len(body)).encode() + b"\r\n\r\n" + body)
        check_error(c.response("�")[0], -32601)
        # Valid bracket/quote strings are not mistaken for excessive nesting.
        check_error(c.request("unknown/escaped", {"text": '"' + '{[\\' * 160 + '😀'}, "escape")[0], -32601)
        c.write(frame({"jsonrpc": "2.0", "id": None, "error": {"code": -32700, "message": "client"}}))
        assert c.sync() == []
        body = b'{"jsonrpc":"2.0","id":"headers","method":"unknown/a"}'
        c.write(b'content-length: ' + str(len(body)).encode() + b'\r\nContent-Type: application/vscode-jsonrpc; charset=utf8\r\nX-Protocol1: test\r\n\r\n' + body)
        check_error(c.response("headers")[0], -32601)
        # Invalid URI UTF-8 is a parameter error; the server must remain usable.
        check_error(c.request("textDocument/definition", {"textDocument": {"uri": "file:///a%FF.void"}, "position": {"line": 0, "character": 0}})[0], -32602)
        if entry.drive:
            other = "Z" if entry.drive[0].upper() != "Z" else "Y"
            wrong_drive = entry.as_uri().replace(entry.drive[0] + ":", other + ":", 1)
            check_error(c.request("textDocument/definition", {"textDocument": {"uri": wrong_drive}, "position": {"line": 0, "character": 0}})[0], -32602)
        check_error(c.request("initialize", {"capabilities": {}})[0], -32600)
        c.notify("textDocument/didChange", {"textDocument": {"uri": entry.as_uri(), "version": 1}, "contentChanges": [{"text": ""}]})
        assert c.sync() == []  # Closed document rejection is notification-only.
        response, _ = c.request("shutdown")
        assert response["result"] is None
        check_error(c.request("unknown/after-shutdown")[0], -32600)
        c.notify("exit")
        c.finish()
    invalid_frames = [b"Content-Length: -1\r\n\r\n", b"Content-Length: +1\r\n\r\n", b"Content-Length: 8388609\r\n\r\n",
                      b"Content-Length: 999999999999999999999\r\n\r\n", b"Content-Length: 1\r\nContent-Length: 1\r\n\r\n0",
                      b"Content-Length: one\r\n\r\n", b"X-Header: x\r\n\r\n", b"Content-Length: 1\n\n0",
                      b"Content-Length: 9\r\n\r\n{}", b"Content-Length: 2\r", b"X-Header: " + b"a" * 8192,
                      b"Content-Length: 2\r\nContent-Type: application/vscode-jsonrpc; charset=utf-16\r\n\r\n{}"]
    for data in invalid_frames:
        with Client(executable, entry) as c:
            c.write(data)
            c.process.stdin.close()
            response = c.receive()
            check_error(response, -32700)
            c.finish(expected=2)
    # Exit without shutdown and unexpected/clean boundary EOF are distinct.
    with Client(executable, entry) as c:
        c.notify("exit")
        c.finish(expected=1)
    with Client(executable, entry) as c:
        c.finish(expected=1)
    with Client(executable, entry) as c:
        c.initialize()
        assert c.request("shutdown")[0]["result"] is None
        c.finish(expected=0)
    with Client(executable, root / "unavailable.void") as c:
        notes = c.initialize()
        assert any(d["code"] == "VOID3002" for n in notes for d in n["diagnostics"])
        c.shutdown()
    print("transport: fragmented/coalesced frames, IDs, malformed messages, bounded framing, lifecycle and EOF passed")


def value_structs(executable, root):
    root.mkdir(parents=True, exist_ok=True)
    entry = root / "main.void"
    text = "value struct point:\n    x: i32 = 4\nfunc entry(value: i32) -> i32:\n    var p: point = point(x = value)\n    p.x += 1\n    return p.x\n"
    entry.write_bytes(text.encode("utf-8"))
    uri = entry.as_uri()
    with Client(executable, entry) as c:
        assert all(not n["diagnostics"] for n in c.initialize())
        for needle, expected_line, expected_column in [("point =", 0, 13), ("point(x", 0, 13),
                                                       ("x = value", 1, 4), ("x +=", 1, 4), ("x\n", 1, 4)]:
            response, _ = c.request("textDocument/definition", {"textDocument": {"uri": uri}, "position": source_position(text, needle)})
            assert response["result"]["uri"] == uri, response
            assert response["result"]["range"]["start"] == {"line": expected_line, "character": expected_column}, response
        response, _ = c.request("textDocument/references", {"textDocument": {"uri": uri},
            "position": {"line": 1, "character": 4}, "context": {"includeDeclaration": False}})
        assert len(response["result"]) == 3, response
        bad = text.replace("var p: point", "const p: point")
        c.notify("textDocument/didOpen", {"textDocument": {"uri": uri, "languageId": "void", "version": 1, "text": bad}})
        notes = diagnostics(c.sync())
        assert len(notes[uri]["diagnostics"]) == 1, notes
        assert "readonly" in notes[uri]["diagnostics"][0]["message"], notes
        assert c.request("textDocument/definition", {"textDocument": {"uri": uri}, "position": source_position(text,"x +=")})[0]["result"] is None
        c.notify("textDocument/didChange", {"textDocument": {"uri": uri, "version": 2}, "contentChanges": [{"text": text}]})
        assert not diagnostics(c.sync())[uri]["diagnostics"]
        c.shutdown()
    print("value structs: real LSP type/constructor/field navigation, references, readonly diagnostics and revision recovery passed")


def receiver_methods(executable, root):
    root.mkdir(parents=True, exist_ok=True)
    entry, worker = root / "main.void", root / "geometry.void"
    geometry = "value struct point:\n    x: i32\nfunc (p point) move(amount: i32):\n    p.x += amount\nfunc (p point) read() -> i32:\n    return p.x\n"
    text = 'use "./geometry.void" as geometry\nfunc entry(value: i32) -> i32:\n    var p = geometry.point()\n    p.move(value)\n    const copy = p\n    return copy.read()\n'
    entry.write_bytes(text.encode("utf-8")); worker.write_bytes(geometry.encode("utf-8"))
    uri, wuri = entry.as_uri(), worker.as_uri()
    query = {"textDocument": {"uri": uri}, "position": source_position(text, "move(value)")}
    with Client(executable, entry) as c:
        assert all(not n["diagnostics"] for n in c.initialize())
        response, _ = c.request("textDocument/definition", query)
        assert response["result"]["uri"] == wuri and response["result"]["range"]["start"] == {"line": 2, "character": 15}, response
        receiver = {"textDocument": {"uri": wuri}, "position": source_position(geometry, "point) move")}
        response, _ = c.request("textDocument/definition", receiver)
        assert response["result"]["range"]["start"] == {"line": 0, "character": 13}, response
        response, _ = c.request("textDocument/references", {**query, "context": {"includeDeclaration": False}})
        assert len(response["result"]) == 1 and response["result"][0]["uri"] == uri, response
        bad = text.replace("var p", "const p")
        c.notify("textDocument/didOpen", {"textDocument": {"uri": uri, "languageId": "void", "version": 1, "text": bad}})
        notes = diagnostics(c.sync())
        assert len(notes[uri]["diagnostics"]) == 1 and notes[uri]["diagnostics"][0]["code"] == "VOID2007", notes
        assert notes[uri]["diagnostics"][0]["range"]["start"] == {"line": 3, "character": 6}, notes
        assert c.request("textDocument/definition", query)[0]["result"] is None
        pure = geometry.replace("p.x += amount", "return")
        c.notify("textDocument/didOpen", {"textDocument": {"uri": wuri, "languageId": "void", "version": 1, "text": pure}})
        assert all(not n["diagnostics"] for n in c.sync())
        assert c.request("textDocument/definition", query)[0]["result"]["uri"] == wuri
        c.notify("textDocument/didChange", {"textDocument": {"uri": wuri, "version": 2}, "contentChanges": [{"text": geometry}]})
        assert diagnostics(c.sync())[uri]["diagnostics"][0]["code"] == "VOID2007"
        c.notify("textDocument/didChange", {"textDocument": {"uri": uri, "version": 2}, "contentChanges": [{"text": text}]})
        assert all(not n["diagnostics"] for n in c.sync())
        c.shutdown()
    print("receiver methods: imported native LSP navigation, references, effect changes and revision recovery passed")


def initialization(executable, root):
    root.mkdir(parents=True, exist_ok=True)
    entry, worker = root / "main.void", root / "types.void"
    types = "value struct point:\n    x: i32\nfunc (p point) init(x: i32 = 4):\n    p.x = x\n"
    text = 'use "./types.void" as types\nfunc entry(value: i32) -> i32:\n    const p = types.point(x = value)\n    return p.x\n'
    entry.write_bytes(text.encode("utf-8")); worker.write_bytes(types.encode("utf-8"))
    uri, wuri = entry.as_uri(), worker.as_uri()
    query = {"textDocument": {"uri": uri}, "position": source_position(text, "point(x")}
    with Client(executable, entry) as c:
        assert all(not n["diagnostics"] for n in c.initialize())
        response, _ = c.request("textDocument/definition", query)
        assert response["result"]["uri"] == wuri and response["result"]["range"]["start"] == {"line": 2, "character": 15}, response
        response, _ = c.request("textDocument/references", {**query, "context": {"includeDeclaration": False}})
        assert len(response["result"]) == 1 and response["result"][0]["uri"] == uri, response
        bad = text.replace("x = value", "missing = value")
        c.notify("textDocument/didOpen", {"textDocument": {"uri": uri, "languageId": "void", "version": 1, "text": bad}})
        notes = diagnostics(c.sync())
        assert notes[uri]["diagnostics"][0]["code"] == "VOID2004", notes
        assert notes[uri]["diagnostics"][0]["range"]["start"] == {"line": 2, "character": 26}, notes
        assert c.request("textDocument/definition", query)[0]["result"] is None
        c.notify("textDocument/didChange", {"textDocument": {"uri": uri, "version": 2}, "contentChanges": [{"text": text}]})
        assert all(not n["diagnostics"] for n in c.sync())
        assert c.request("textDocument/definition", query)[0]["result"]["uri"] == wuri
        c.shutdown()
    print("initialization: imported initializer navigation/references, exact argument diagnostics and unsaved recovery passed")


def managed_structs(executable, root):
    root.mkdir(parents=True, exist_ok=True)
    entry, worker = root / "main.void", root / "types.void"
    types = "struct node:\n    value: i32\n    next: node?\nfunc (n node) read() -> i32:\n    return n.value\n"
    text = 'use "./types.void" as types\nfunc entry(value: i32) -> i32:\n    var n = types.node(value = value)\n    var alias = n\n    alias.value += 1\n    const view = n\n    return view.read()\n'
    entry.write_bytes(text.encode("utf-8")); worker.write_bytes(types.encode("utf-8"))
    uri, wuri = entry.as_uri(), worker.as_uri()
    field = {"textDocument": {"uri": uri}, "position": source_position(text, "value +=")}
    method = {"textDocument": {"uri": uri}, "position": source_position(text, "read()")}
    with Client(executable, entry) as c:
        assert all(not n["diagnostics"] for n in c.initialize())
        response, _ = c.request("textDocument/definition", field)
        assert response["result"]["uri"] == wuri and response["result"]["range"]["start"] == source_position(types, "value: i32"), response
        response, _ = c.request("textDocument/definition", method)
        assert response["result"]["uri"] == wuri and response["result"]["range"]["start"] == source_position(types, "read()"), response
        recursive = {"textDocument": {"uri": wuri}, "position": source_position(types, "node?")}
        response, _ = c.request("textDocument/definition", recursive)
        assert response["result"]["range"]["start"] == source_position(types, "node:"), response
        bad = text.replace("var n =", "const n =")
        c.notify("textDocument/didOpen", {"textDocument": {"uri": uri, "languageId": "void", "version": 1, "text": bad}})
        notes = diagnostics(c.sync())
        assert len(notes[uri]["diagnostics"]) == 1 and notes[uri]["diagnostics"][0]["code"] == "VOID2007", notes
        assert c.request("textDocument/definition", field)[0]["result"] is None
        c.notify("textDocument/didChange", {"textDocument": {"uri": uri, "version": 2}, "contentChanges": [{"text": text}]})
        assert all(not n["diagnostics"] for n in c.sync())
        assert c.request("textDocument/definition", field)[0]["result"]["uri"] == wuri
        c.shutdown()
    print("managed structs: cyclic types, imported fields/methods, readonly aliases and unsaved recovery passed")


def managed_initialization(executable, root):
    root.mkdir(parents=True, exist_ok=True)
    entry, worker = root / "main.void", root / "types.void"
    types = "struct child:\n    value: i32\nstruct parent:\n    child: child\nfunc helper(p: parent) -> i32:\n    return p.child.value\nfunc (p parent) init(c: child):\n    p.child = c\n    p.child.value += helper(p)\n"
    text = 'use "./types.void" as types\nfunc entry(value: i32) -> i32:\n    const p = types.parent(c = types.child(value = value))\n    return p.child.value\n'
    entry.write_bytes(text.encode("utf-8")); worker.write_bytes(types.encode("utf-8"))
    uri, wuri = entry.as_uri(), worker.as_uri()
    query = {"textDocument": {"uri": uri}, "position": source_position(text, "parent(c")}
    with Client(executable, entry) as c:
        assert all(not n["diagnostics"] for n in c.initialize())
        response, _ = c.request("textDocument/definition", query)
        assert response["result"]["uri"] == wuri and response["result"]["range"]["start"] == source_position(types, "init(c"), response
        response, _ = c.request("textDocument/references", {**query, "context": {"includeDeclaration": False}})
        assert len(response["result"]) == 1 and response["result"][0]["uri"] == uri, response
        helper = {"textDocument": {"uri": wuri}, "position": source_position(types, "helper(p)")}
        assert c.request("textDocument/definition", helper)[0]["result"]["range"]["start"] == source_position(types, "helper(p:"), "synthetic helper variant leaked into navigation"
        bad = types.replace("    p.child = c\n", "")
        c.notify("textDocument/didOpen", {"textDocument": {"uri": wuri, "languageId": "void", "version": 1, "text": bad}})
        notes = diagnostics(c.sync())
        assert any("initialized" in d["message"] for d in notes[wuri]["diagnostics"]), notes
        assert c.request("textDocument/definition", query)[0]["result"] is None
        assert c.request("textDocument/references", {**query, "context": {"includeDeclaration": True}})[0]["result"] == []
        c.notify("textDocument/didChange", {"textDocument": {"uri": wuri, "version": 2}, "contentChanges": [{"text": types}]})
        assert all(not n["diagnostics"] for n in c.sync())
        assert c.request("textDocument/definition", query)[0]["result"]["uri"] == wuri
        c.shutdown()
    print("managed initialization: imported constructor, private helper navigation, required-field diagnostics and unsaved repair passed")


if __name__ == "__main__":
    executable = pathlib.Path(sys.argv[1]).resolve()
    # Unicode physical root names work without changing the ASCII VFS policy.
    root = pathlib.Path(sys.argv[2]).resolve() / "project space é😀 # %"
    editor(executable, root)
    transport(executable, root)
    value_structs(executable, root / "value struct project")
    receiver_methods(executable, root / "receiver method project")

    initialization(executable, root / "initialization project")

    managed_structs(executable, root / "managed struct project")

    managed_initialization(executable, root / "managed initialization project")
