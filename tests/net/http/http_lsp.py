"""HTTP's compiler-owned API docs, navigation and public completion inventory."""
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
with tempfile.TemporaryDirectory(prefix="VOID HTTP LSP ") as directory:
    entry = Path(directory) / "main.void"
    text = 'use net/http as webhttp\nfunc main():\n    var parsed = webhttp.open_client()\n'
    entry.write_text(text, encoding="utf-8")
    with Client(server, entry) as client:
        client.request("initialize", {"capabilities": {}})
        client.notify("initialized", {})
        client.sync()
        client.notify("textDocument/didOpen", {"textDocument": {"uri": entry.as_uri(), "languageId": "void", "version": 1, "text": text}})
        notes = client.sync()
        assert all(not note["diagnostics"] for note in notes), notes
        position = {"line": 2, "character": text.splitlines()[2].index("open_client(") + 1}
        params = {"textDocument": {"uri": entry.as_uri()}, "position": position}
        hover = client.request("textDocument/hover", params)[0]
        assert "Acquire a client locally" in str(hover), hover
        definition = client.request("textDocument/definition", params)[0]
        assert "stdlib/net/http.void" in str(definition), definition
        denied = client.request("textDocument/rename", params | {"newName": "changed"})[0]
        assert "read-only" in str(denied), denied
        incomplete = "use net/http as webhttp\nfunc main():\n    var output = webhttp."
        client.notify("textDocument/didChange", {"textDocument": {"uri": entry.as_uri(), "version": 2}, "contentChanges": [{"text": incomplete}]})
        client.sync()
        response = client.request("textDocument/completion", {"textDocument": {"uri": entry.as_uri()},
            "position": {"line": 2, "character": len(incomplete.splitlines()[2])}})[0]["result"]
        items = response["items"] if isinstance(response, dict) else response
        names = {item["label"] for item in items}
        assert {"open_client", "get", "get_async", "field", "header_values", "capabilities",
                "client", "transfer", "request", "response", "response_info", "transfer_info", "chunk",
                "header", "part", "part_data", "body", "tls_options", "proxy_options", "client_options",
                "request_options", "error", "error_kind", "failure_phase", "authentication", "trust_source",
                "client_identity", "protocol", "http_version", "redirect_policy", "ip_family",
                "content_decoding", "proxy_mode"} <= names, names
        assert not any(name.startswith("_") for name in names), names
        version = 2
        def changed(value):
            global version
            version += 1
            client.notify("textDocument/didChange", {"textDocument": {"uri": entry.as_uri(), "version": version},
                "contentChanges": [{"text": value}]})
            return client.sync()
        prefix = "use net/http as webhttp\nuse io\n"
        for owner_type, expected in {
            "client": {"send", "send_async", "get", "get_async", "start", "download", "download_async",
                       "upload", "upload_async", "clear_cookies", "close", "close_async"},
            "transfer": {"response", "response_async", "read_bytes", "read_bytes_async", "write_bytes",
                         "write_bytes_async", "finish_upload", "finish", "finish_async", "close", "close_async"},
            "response": {"text", "require_success", "info", "bytes", "trailers"},
            "client_options": {"tls", "proxy", "cookies", "max_active", "max_connections", "max_per_origin",
                               "max_http2_streams", "max_idle", "idle_timeout_ms", "connection_lifetime_ms",
                               "dns_cache_ms", "user_agent"},
        }.items():
            value = prefix + f"func inspect(subject: webhttp.{owner_type}):\n    var output = subject."
            changed(value)
            response = client.request("textDocument/completion", {"textDocument": {"uri": entry.as_uri()},
                "position": {"line": 3, "character": len(value.splitlines()[3])}})[0]["result"]
            items = response["items"] if isinstance(response, dict) else response
            labels = {item["label"] for item in items}
            assert expected <= labels, (owner_type, labels)
            assert not any(label.startswith("_") for label in labels), (owner_type, labels)
        calls = ["owner.send(submission)", "owner.send_async(submission)", "owner.get(\"https://example.com/\")",
            "owner.get_async(\"https://example.com/\")", "owner.start(submission)",
            "owner.download(submission, source_file)", "owner.download_async(submission, source_file)",
            "owner.upload(submission, source_file)", "owner.upload_async(submission, source_file)",
            "owner.clear_cookies()", "owner.close()", "owner.close_async()", "exchange.response()",
            "exchange.response_async()", "exchange.read_bytes(7)", "exchange.read_bytes_async(7)",
            'exchange.write_bytes("x".to_utf8())', 'exchange.write_bytes_async("x".to_utf8())', "exchange.finish_upload()",
            "exchange.finish()", "exchange.finish_async()", "exchange.close()", "exchange.close_async()",
            "reply.text()", "reply.require_success()"]
        value = prefix + "async func inspect(owner: webhttp.client, exchange: webhttp.transfer, reply: webhttp.response, submission: webhttp.request, source_file: io.file):\n"
        value += "".join(f"    var step_{index} = {call}\n" for index, call in enumerate(calls))
        notes = changed(value)
        assert all(not note["diagnostics"] for note in notes), notes
        for index, call in enumerate(calls):
            line = value.splitlines()[index + 3]
            member = call.split(".", 1)[1].split("(", 1)[0]
            point = {"line": index + 3, "character": line.index("." + member) + 2}
            params = {"textDocument": {"uri": entry.as_uri()}, "position": point}
            hover = client.request("textDocument/hover", params)[0]["result"]
            assert hover and "@return" not in str(hover) and member in str(hover), (call, hover)
            definition = client.request("textDocument/definition", params)[0]["result"]
            assert "stdlib/net/http.void" in str(definition), (call, definition)
        value = prefix + "func inspect(exchange: webhttp.transfer):\n    var pending = exchange.read_bytes("
        changed(value)
        signature = client.request("textDocument/signatureHelp", {"textDocument": {"uri": entry.as_uri()},
            "position": {"line": 3, "character": len(value.splitlines()[3])},
            "context": {"triggerKind": 1}})[0]["result"]
        assert signature and "max_count" in str(signature) and signature["activeParameter"] == 0, signature
        client.shutdown()
print("HTTP LSP: module/member inventory, 25 documented methods, navigation and active parameter passed")
