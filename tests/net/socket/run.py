"""Qualify the socket provider and compiled source against local peers."""
from pathlib import Path
from queue import Queue
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import os
import re
import socket
import struct
import subprocess
import sys
import tempfile
from threading import Thread

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(HERE.parent / "http"))
from dns_fixture import DNS

compiler, native, runner, async_binary = (Path(value).resolve() for value in sys.argv[1:])


class SocketDNS(DNS):
    def answer(self, query, transport):
        response = super().answer(query, transport)
        for name in ("fallback.test", "nodata.test"):
            question = DNS.name(name)
            if query[12:12 + len(question)] != question:
                continue
            end = 12 + len(question) + 4
            addresses = ["127.0.0.2", "127.0.0.1"] if name == "fallback.test" else []
            records = b"".join(b"\xc0\x0c" + struct.pack("!HHIH", 1, 1, 60, 4) + socket.inet_aton(address)
                               for address in addresses)
            return query[:2] + struct.pack("!HHHHH", 0x8180, 1, len(addresses), 0, 0) + query[12:end] + records
        return response


def run(arguments, work, timeout=90):
    result = subprocess.run(list(map(str, arguments)), cwd=work, capture_output=True, timeout=timeout)
    assert result.returncode == 0, (arguments, result.returncode, result.stdout, result.stderr)
    assert not any(error in result.stderr for error in (b"AddressSanitizer", b"LeakSanitizer", b"runtime error:")), result.stderr
    return result


def linked(source, destination, work, flags=()):
    run([compiler, "build", source, *flags, "-o", destination], work)
    return destination


class HTTPPeer(BaseHTTPRequestHandler):
    def do_GET(self):
        content = b"shared provider"
        self.send_response(200)
        self.send_header("Content-Length", str(len(content)))
        self.end_headers()
        self.wfile.write(content)

    def log_message(self, *args):
        pass


def echo_examples(work, extension):
    examples = ROOT / "examples/net_sockets"
    server_image = linked(examples / "tcp_echo_server.void", work / ("server" + extension), work)
    client_image = linked(examples / "tcp_echo_client.void", work / ("client" + extension), work)
    with subprocess.Popen([server_image], cwd=work, stdout=subprocess.PIPE, stderr=subprocess.PIPE) as server:
        line = Queue(maxsize=1)
        reader = Thread(target=lambda: line.put(server.stdout.readline()))
        reader.start()
        try:
            opening = line.get(timeout=5)
            match = re.fullmatch(rb"Listening (\d+)\r?\n", opening)
            assert match, opening
            result = run([client_image, match[1].decode("ascii")], work, timeout=8)
            assert b"Echo verified" in result.stdout
            output, error = server.communicate(timeout=8)
            assert server.returncode == 0 and not error, (server.returncode, output, error)
        finally:
            if server.poll() is None:
                server.kill()
            reader.join(timeout=5)
    assert b"Datagram verified" in run([compiler, "run", examples / "udp_loopback.void", "--no-cache"], work).stdout
    assert b"[::1]:7777" in run([compiler, "run", examples / "addresses.void", "--no-cache"], work).stdout


with tempfile.TemporaryDirectory(prefix="VOID network 雪 ") as folder:
    work = Path(folder)
    dns = SocketDNS()
    try:
        result = run([native, dns.endpoint], work, timeout=25)
        assert b"descriptor and shutdown controls passed" in result.stdout, result.stdout
        queries = dns.counts()
        assert queries["good.test", 1, "udp"] and queries["good.test", 28, "udp"], queries
        assert queries["fanout.test", 1, "tcp"], queries
    finally:
        dns.close()
    extension = ".exe" if os.name == "nt" else ""
    source = HERE / "source_sync.void"
    for index, flags in enumerate((["--no-opt"], [])):
        run([compiler, "run", source, *flags, "--no-cache"], work)
        application = linked(source, work / (f"socket fixture {index}" + extension), work, flags)
        run([application], work)
    with ThreadingHTTPServer(("127.0.0.1", 0), HTTPPeer) as http:
        thread = Thread(target=http.serve_forever)
        thread.start()
        try:
            (work / "http-peer.txt").write_text(f"http://127.0.0.1:{http.server_port}/", encoding="ascii")
            run([runner, "--async-one-worker", HERE / "source_async.void"], work)
            run([async_binary], work)
        finally:
            http.shutdown()
            thread.join()
    echo_examples(work, extension)
print("TCP/UDP/DNS loopback, one-worker async, JIT and linked AOT passed")
