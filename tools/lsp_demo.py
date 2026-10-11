"""Run: python tools/lsp_demo.py build/ci-debug/voidls

Creates a disposable real multi-file project and speaks actual framed JSON-RPC.
"""
import pathlib
import sys
import tempfile
from lsp_client import Client


def demonstrate(executable):
    with tempfile.TemporaryDirectory(prefix="VOID LSP demo ") as directory:
        root = pathlib.Path(directory).resolve()  # macOS temporary roots can have a /var alias.
        main = 'use "/worker.void" as worker\nfunc entry(x: i32) -> i32:\n    return worker.calculate(x)\n'
        worker = 'func calculate(x: i32) -> i32:\n    return x + 42\n'
        (root / "main.void").write_text(main, encoding="utf-8")
        (root / "worker.void").write_text(worker, encoding="utf-8")
        uri = (root / "main.void").as_uri()
        worker_uri = (root / "worker.void").as_uri()
        query = {"textDocument": {"uri": uri}, "position": {"line": 2, "character": 18}}
        with Client(executable, root / "main.void") as client:
            client.initialize()
            print("initialized: full synchronization, UTF-16, definition and references")
            client.notify("textDocument/didOpen", {"textDocument": {"uri": uri, "languageId": "void", "version": 0, "text": main}})
            notes = client.sync()
            assert all(not n["diagnostics"] for n in notes)
            print("open: no diagnostics")
            response, _ = client.request("textDocument/definition", query, "definition")
            assert response["result"]["uri"] == worker_uri, response
            print("definition: worker.void:1:6")
            response, _ = client.request("textDocument/references", {**query, "context": {"includeDeclaration": True}})
            assert len(response["result"]) == 2
            print("references: declaration and one source call")
            client.notify("textDocument/didOpen", {"textDocument": {"uri": worker_uri, "languageId": "void", "version": -1,
                "text": worker.replace("x + 42", "missing")}})
            notes = client.sync()
            errors = [d for n in notes for d in n["diagnostics"]]
            assert any(d["code"] == "VOID2002" for d in errors)
            print("unsaved edit: VOID2002 at worker.void:2:12")
            response, _ = client.request("textDocument/definition", query)
            assert response["result"] is None
            print("invalid revision: navigation returns null")
            client.notify("textDocument/didChange", {"textDocument": {"uri": worker_uri, "version": 0}, "contentChanges": [{"text": worker}]})
            notes = client.sync()
            assert all(not n["diagnostics"] for n in notes)
            response, _ = client.request("textDocument/definition", query)
            assert response["result"]["uri"] == worker_uri
            print("repair: old diagnostics cleared; fresh navigation restored")
            client.shutdown()
            print("shutdown and exit: clean protocol streams")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: python tools/lsp_demo.py <voidls executable>")
    demonstrate(pathlib.Path(sys.argv[1]).resolve())
