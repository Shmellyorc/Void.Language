"""Compiler-owned networking navigation and UTF-16 positions."""
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools"))
from lsp_client import Client

source = '''use net/address
use net/tcp
use net/udp

func inspect(subject: tcp.stream, receiver: udp.socket):
    var addresses = ["雪😀😀", address.format_ip(address.loopback_v6())]
    var read = subject.read_bytes(timeout_ms = 5)
    var packet = receiver.receive_from(max_count = 8)
'''


def position(line, token):
    text = source.splitlines()[line]
    return {"line": line, "character": len(text[:text.index(token)].encode("utf-16-le")) // 2}


with tempfile.TemporaryDirectory(prefix="VOID socket LSP ") as directory:
    entry = Path(directory) / "main.void"
    entry.write_text(source, encoding="utf-8")
    document = {"uri": entry.as_uri()}
    with Client(Path(sys.argv[1]).resolve(), entry) as client:
        client.request("initialize", {"capabilities": {}})
        client.notify("initialized", {})
        client.sync()
        client.notify("textDocument/didOpen", {"textDocument": document | {"languageId": "void", "version": 1, "text": source}})
        notifications = client.sync()
        assert all(not notification["diagnostics"] for notification in notifications), notifications
        for line, token, module in ((5, "format_ip", "address"), (6, "read_bytes", "tcp"),
                                    (7, "receive_from", "udp")):
            params = {"textDocument": document, "position": position(line, token)}
            hover = client.request("textDocument/hover", params)[0]["result"]
            assert hover and token in str(hover), (params, hover)
            definition = client.request("textDocument/definition", params)[0]["result"]
            assert f"/stdlib/net/{module}.void" in str(definition).replace("\\", "/"), definition
        client.shutdown()
print("Networking API hover and definition passed with UTF-16 positions")
