"""Loopback-only native HTTP integration fixture; no external service required."""
import argparse
import base64
import gzip
import hashlib
import http.server
import http.client
import json
import os
from pathlib import Path
import socket
import socketserver
import select
import ssl
import subprocess
import tempfile
import threading
import time
import zlib
import sys
import urllib.request
import urllib.parse
import email.parser
import email.policy

release = threading.Event()
upload_release = threading.Event()
upload_dependency = threading.Event()
socks_addresses = []
proxy_events = []
required_h2_violations = []
h2_paused_events = []
h2_retry_events = []

def dependency_prefix(binary):
    """Read the actual selected prefix, including native single-config layouts."""
    for parent in (Path(binary).resolve().parent, Path(binary).resolve().parent.parent):
        cache = parent / 'CMakeCache.txt'
        if cache.is_file():
            for line in cache.read_text().splitlines():
                if line.startswith('VOID_HTTP_DEPS_ROOT:PATH='):
                    return Path(line.split('=', 1)[1])
    root = Path(__file__).resolve().parents[3]
    policy = json.loads((root / 'third_party/http-versions.json').read_text())
    return root / '.toolchains/http/prefix' / policy['bundle'] / 'Release'

def certificate_tool(binary, explicit=None):
    """The Windows fixture tool is separate from production Schannel libraries."""
    root = Path(__file__).resolve().parents[3]
    prefix = dependency_prefix(binary)
    tool = Path(explicit).resolve() if explicit else prefix / 'bin' / ('openssl.exe' if os.name == 'nt' else 'openssl')
    if not explicit and not tool.is_file() and os.name == 'nt':
        tool = root / '.toolchains/http/test-certificates/bin/openssl.exe'
    if not tool.is_file():
        raise RuntimeError('Pinned certificate fixture tool missing; run python3 tools/setup_http_tests.py --certificates' if os.name == 'nt' else 'Pinned dependencies missing; run python3 tools/setup_http_deps.py for this build profile')
    if not explicit:
        manifest_path = tool.parent.parent / 'VOID-HTTP-DEPENDENCIES.json'
        if not manifest_path.is_file(): manifest_path = tool.parent.parent / 'VOID-HTTP-TEST-CERTIFICATES.json'
        manifest = json.loads(manifest_path.read_text())
        name = str(tool.relative_to(manifest_path.parent)).replace('\\', '/')
        assert hashlib.sha256(tool.read_bytes()).hexdigest() == manifest['files'][name]
    return tool

class RepeatedBody:
    """Large loopback payload without storing its advertised size in the fixture."""
    def __init__(self, byte, count): self.byte, self.count = byte, count
    def __len__(self): return self.count
    def __getitem__(self, item):
        start, stop, step = item.indices(self.count)
        assert step == 1
        return self.byte * (stop - start)

def valid_digest(authorization, user, password, method, uri, realm, nonce):
    if not authorization.startswith("Digest "): return False
    fields = urllib.request.parse_keqv_list(urllib.request.parse_http_list(authorization[7:]))
    md5 = lambda value: hashlib.md5(value.encode()).hexdigest()
    expected = md5(md5(user + ":" + realm + ":" + password) + ":" + nonce + ":" + fields.get("nc", "") + ":" +
        fields.get("cnonce", "") + ":auth:" + md5(method + ":" + fields.get("uri", "")))
    return fields.get("username") == user and fields.get("uri") == uri and fields.get("response") == expected

def relay(left, right):
    try:
        while True:
            ready = [stream for stream in [left, right] if isinstance(stream, ssl.SSLSocket) and stream.pending()]
            if not ready: ready, _, _ = select.select([left, right], [], [], 10)
            if not ready: return
            for source in ready:
                data = source.recv(65536)
                if not data: return
                (right if source is left else left).sendall(data)
    except (OSError, ssl.SSLError):
        pass

class Proxy(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    def log_message(self, *args): pass
    def authorize(self):
        authorization = self.headers.get("Proxy-Authorization", "")
        # curl's Digest API uses the origin-form path for forwarded requests;
        # the proxy request line itself uses absolute-form. CONNECT uses authority.
        parsed = urllib.parse.urlsplit(self.path)
        auth_target = self.path if self.command == "CONNECT" else urllib.parse.urlunsplit(("", "", parsed.path or "/", parsed.query, ""))
        if authorization.startswith("Digest "):
            fields = urllib.request.parse_keqv_list(urllib.request.parse_http_list(authorization[7:]))
            proxy_events.append({"method": self.command, "request_target": self.path, "digest_uri": fields.get("uri"),
                "valid": valid_digest(authorization, "proxy", "password", self.command, auth_target, "proxy", "proxy-nonce")})
        if not self.server.secure or authorization == "Basic " + base64.b64encode(b"proxy:password").decode() or \
            valid_digest(authorization, "proxy", "password", self.command, auth_target, "proxy", "proxy-nonce"):
            return True
        self.send_response(407); self.send_header("Proxy-Authenticate", 'Basic realm="proxy"')
        self.send_header("Proxy-Authenticate", 'Digest realm="proxy", nonce="proxy-nonce", algorithm=MD5, qop="auth"')
        self.send_header("Content-Length", "0"); self.end_headers(); return False
    def do_CONNECT(self):
        if not self.authorize(): return
        host, port = self.path.rsplit(":", 1)
        assert host == "127.0.0.1", "Fixture proxies only reach loopback"
        with socket.create_connection((host, int(port)), timeout=10) as remote:
            self.send_response(200); self.end_headers(); self.wfile.flush()
            relay(self.connection, remote)
        self.close_connection = True
    def do_GET(self):
        if not self.authorize(): return
        target = urllib.parse.urlsplit(self.path)
        assert target.scheme == "http" and target.hostname == "127.0.0.1"
        connection = http.client.HTTPConnection(target.hostname, target.port, timeout=10)
        try:
            headers = {name: value for name, value in self.headers.items() if name.lower() not in
                ["host", "connection", "proxy-connection", "proxy-authorization"]}
            connection.request("GET", urllib.parse.urlunsplit(("", "", target.path, target.query, "")), headers=headers)
            response = connection.getresponse(); body = response.read()
            self.send_response(response.status)
            for name, value in response.getheaders():
                if name.lower() not in ["transfer-encoding", "content-length", "connection"]: self.send_header(name, value)
            self.send_header("Content-Length", str(len(body))); self.end_headers(); self.wfile.write(body)
        finally: connection.close()

class Socks(socketserver.BaseRequestHandler):
    def handle(self):
        def exact(count):
            data = b""
            while len(data) < count:
                block = self.request.recv(count - len(data))
                if not block: raise ConnectionResetError()
                data += block
            return data
        try:
            version, count = exact(2); assert version == 5 and 0 in exact(count)
            self.request.sendall(b"\x05\x00")
            version, command, reserved, kind = exact(4); assert (version, command, reserved) == (5, 1, 0)
            socks_addresses.append(kind)
            if kind == 1: host = socket.inet_ntop(socket.AF_INET, exact(4))
            elif kind == 4: host = socket.inet_ntop(socket.AF_INET6, exact(16))
            else: assert kind == 3; host = exact(exact(1)[0]).decode("ascii")
            assert host in ["127.0.0.1", "::1", "localhost"]
            port = int.from_bytes(exact(2), "big")
            with socket.create_connection((host, port), timeout=10) as remote:
                self.request.sendall(b"\x05\x00\x00\x01\x7f\x00\x00\x01\x00\x00")
                relay(self.request, remote)
        except OSError: pass
class Server(http.server.ThreadingHTTPServer):
    def handle_error(self, request, client_address):
        if isinstance(sys.exc_info()[1], (BrokenPipeError, ConnectionResetError, ssl.SSLError, TimeoutError)):
            return
        super().handle_error(request, client_address)

class H2Handler(socketserver.BaseRequestHandler):
    def handle(self):
        import h2.config
        import h2.connection
        import h2.events
        with self.server.context.wrap_socket(self.request, server_side=True) as connection:
            connection.settimeout(10)
            protocol = h2.connection.H2Connection(config=h2.config.H2Configuration(client_side=False, header_encoding="utf-8"))
            protocol.initiate_connection(); connection.sendall(protocol.data_to_send())
            bodies, paths, pending = {}, {}, {}
            ended = set()
            cookie_race = []
            slow = None
            def respond(stream, body, headers=()):
                protocol.send_headers(stream, [(":status", "200"), ("content-length", str(len(body))), ("x-h2", "actual"), *headers])
                pending[stream] = [body, 0]
            def pump():
                for stream in list(pending):
                    body, position = pending[stream]
                    while position < len(body):
                        count = min(protocol.local_flow_control_window(stream), protocol.max_outbound_frame_size, len(body) - position)
                        if count <= 0: break
                        protocol.send_data(stream, body[position:position + count],
                            end_stream=position + count == len(body) and paths[stream] != "/duplex")
                        position += count
                    if paths[stream] == "/paused":
                        h2_paused_events.append({"peer_port": self.client_address[1], "stream": stream,
                            "event": "sent", "bytes": position})
                    if (not body and paths[stream] != "/duplex") or (position == len(body) and stream in ended):
                        protocol.send_data(stream, b"", end_stream=True)
                    if position == len(body): del pending[stream]
                    else: pending[stream][1] = position
            while True:
                try: data = connection.recv(65536)
                except (ssl.SSLError, ConnectionResetError, TimeoutError): return
                if not data: return
                for event in protocol.receive_data(data):
                    if isinstance(event, h2.events.RequestReceived):
                        paths[event.stream_id] = dict(event.headers)[":path"]
                        bodies[event.stream_id] = bytearray()
                        if paths[event.stream_id] == "/duplex":
                            protocol.send_headers(event.stream_id, [(":status", "200"), ("content-length", "150000")])
                        elif paths[event.stream_id] == "/early-error":
                            protocol.send_headers(event.stream_id, [(":status", "413"), ("content-length", "3")])
                            protocol.send_data(event.stream_id, b"bad", end_stream=True)
                    elif isinstance(event, h2.events.DataReceived):
                        bodies[event.stream_id] += event.data
                        protocol.acknowledge_received_data(event.flow_controlled_length, event.stream_id)
                        if paths[event.stream_id] == "/duplex":
                            queued = pending.setdefault(event.stream_id, [b"", 0])
                            queued[0] += bytes(bodies[event.stream_id])
                            bodies[event.stream_id].clear()
                    elif isinstance(event, h2.events.StreamEnded):
                        path = paths[event.stream_id]
                        if path == "/duplex":
                            ended.add(event.stream_id)
                            pending.setdefault(event.stream_id, [b"", 0])
                        elif path == "/early-error": pass
                        elif path == "/large-header":
                            protocol.send_headers(event.stream_id, [(":status", "200"), ("x-limit", "x"*70000), ("content-length", "0")], end_stream=True)
                        elif path == "/reset":
                            protocol.send_headers(event.stream_id, [(":status", "200"), ("content-length", "10")])
                            protocol.send_data(event.stream_id, b"abc")
                            protocol.reset_stream(event.stream_id, error_code=2)
                        elif path == "/goaway":
                            protocol.send_headers(event.stream_id, [(":status", "200"), ("content-length", "2")])
                            protocol.send_data(event.stream_id, b"ok", end_stream=True)
                            protocol.close_connection(error_code=0, last_stream_id=event.stream_id)
                            connection.sendall(protocol.data_to_send())
                            return
                        elif path == "/refused-post":
                            h2_retry_events.append({"peer_port": self.client_address[1], "stream": event.stream_id,
                                "bytes": len(bodies[event.stream_id]), "sha256": hashlib.sha256(bodies[event.stream_id]).hexdigest()})
                            protocol.reset_stream(event.stream_id, error_code=7)  # REFUSED_STREAM
                        elif path == "/slow":
                            protocol.send_headers(event.stream_id, [(":status", "200"), ("content-length", "8")])
                            slow = event.stream_id
                        elif path == "/release":
                            respond(event.stream_id, b"ok")
                            if slow is not None: pending[slow] = [b"released", 0]; slow = None
                        elif path == "/large": respond(event.stream_id, b"h" * 2097152)
                        elif path == "/paused": respond(event.stream_id, RepeatedBody(b"p", 536870912))
                        elif path == "/cookie-race":
                            cookie_race.append(event.stream_id)
                            if len(cookie_race) == 2:
                                # Both mutations are sent in the same TLS write.
                                # The first response must own the overflow error.
                                for index, stream in enumerate(cookie_race):
                                    respond(stream, b"ok", [("set-cookie", f"race{index}=v; Path=/")])
                                cookie_race.clear()
                        elif path == "/echo": respond(event.stream_id, bytes(bodies[event.stream_id]))
                        else: respond(event.stream_id, b"real-h2")
                    elif isinstance(event, h2.events.StreamReset): pending.pop(event.stream_id, None)
                    elif isinstance(event, h2.events.WindowUpdated) and paths.get(event.stream_id) == "/paused":
                        h2_paused_events.append({"peer_port": self.client_address[1], "stream": event.stream_id,
                            "event": "window", "delta": event.delta})
                    elif isinstance(event, h2.events.ConnectionTerminated): return
                pump()
                try: connection.sendall(protocol.data_to_send())
                except (BrokenPipeError, ConnectionResetError, ssl.SSLError): return
class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    def parse_request(self):
        if self.raw_requestline.startswith(b"PRI * HTTP/2.0") or b" /must-not-send-h1 " in self.raw_requestline:
            required_h2_violations.append(self.raw_requestline.decode("ascii", errors="replace").strip())
        return super().parse_request()
    def log_message(self, *args):
        pass
    def read_body(self):
        if self.headers.get("Transfer-Encoding", "").lower() == "chunked":
            data = bytearray()
            while True:
                count = int(self.rfile.readline(128).split(b";", 1)[0], 16)
                if not count:
                    assert self.rfile.readline(128) == b"\r\n"
                    return bytes(data)
                data += self.rfile.read(count)
                assert self.rfile.read(2) == b"\r\n"
        return self.rfile.read(int(self.headers.get("Content-Length", "0")))
    def reply(self, body, status=200, headers=()):
        self.send_response(status)
        for name, value in headers:
            self.send_header(name, value)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if self.command != "HEAD":
            try:
                self.wfile.write(body)
            except (BrokenPipeError, ConnectionResetError, ssl.SSLError):
                pass
    def route(self):
        self.connection.settimeout(15)
        if self.path == "/upload-dependency":
            # More than socket buffering: the first producer cannot finish
            # while holding the sole worker needed by the second producer.
            if not upload_dependency.wait(5):
                self.reply(b"producer-starved", status=500); self.close_connection = True; return
            body = self.read_body()
            self.reply(b"independent" if len(body) == 33554432 else b"wrong-length")
            return
        body = self.read_body()
        path = self.path.split("?", 1)[0]
        if path == "/plain":
            self.reply(b"hello\0world", headers=[("X-Order", "a"), ("X-Order", "b")])
        elif path == "/echo":
            self.reply(self.command.encode() + b":" + body)
        elif path == "/multipart":
            message = email.parser.BytesParser(policy=email.policy.default).parsebytes(
                b"Content-Type: " + self.headers["Content-Type"].encode() + b"\r\nMIME-Version: 1.0\r\n\r\n" + body)
            summary = []
            for part in message.iter_parts():
                payload = part.get_payload(decode=True)
                assert not payload or all(value == payload[0] for value in payload)
                summary.append(part.get_param("name", header="Content-Disposition") + ":" + str(len(payload)) + ":" +
                    (chr(payload[0]) if payload else "-"))
            self.reply("|".join(summary).encode())
        elif path == "/upload-gate":
            assert len(body) == 100000
            if upload_release.wait(5): self.reply(b"first")
            else: self.reply(b"producer-starved", status=500)
        elif path == "/upload-release":
            assert len(body) == 100000
            upload_release.set(); self.reply(b"second")
        elif path == "/upload-dependency-release":
            upload_dependency.set(); self.reply(b"released")
        elif path == "/headers":
            self.reply((self.path + "|" + (self.headers.get("X-Empty") or "") + "|" + (self.headers.get("X-Test") or "")).encode())
        elif path == "/proxy-isolation":
            self.reply((self.headers.get("Proxy-Authorization", "") + "|" + self.headers.get("Authorization", "")).encode())
        elif path == "/auth-basic":
            good = self.headers.get("Authorization") == "Basic " + base64.b64encode(b"user:password").decode()
            self.reply(b"authenticated" if good else b"denied", 200 if good else 401,
                [] if good else [("WWW-Authenticate", 'Basic realm="fixture"')])
        elif path == "/auth-bearer":
            good = self.headers.get("Authorization") == "Bearer exact-token"
            self.reply(b"authenticated" if good else b"denied", 200 if good else 401)
        elif path == "/auth-digest":
            authorization = self.headers.get("Authorization", "")
            good = valid_digest(authorization, "user", "password", self.command, self.path, "fixture", "fixture-nonce")
            self.reply(b"authenticated:" + body if good else b"denied", 200 if good else 401,
                [] if good else [("WWW-Authenticate", 'Digest realm="fixture", nonce="fixture-nonce", algorithm=MD5, qop="auth"')])
        elif path == "/gzip":
            self.reply(gzip.compress(b"a" * 200000) + gzip.compress(b"b" * 200000), headers=[("Content-Encoding", "gzip")])
        elif path == "/deflate":
            self.reply(zlib.compress(b"decoded" * 20000), headers=[("Content-Encoding", "deflate")])
        elif path == "/bad-gzip":
            self.reply(gzip.compress(b"prefix")[:-3], headers=[("Content-Encoding", "gzip")])
        elif path == "/redirect":
            self.reply(b"ignored intermediate", status=303, headers=[("Location", "/echo")])
        elif path == "/loop":
            self.reply(b"", status=302, headers=[("Location", "/loop")])
        elif path == "/set-cookie":
            self.reply(b"set", headers=[("Set-Cookie", "session=owned; Path=/; HttpOnly")])
        elif path == "/cookie-batch":
            start, count, size = map(int, self.path.split("?", 1)[1].split(","))
            assert 0 <= count <= 32 and 0 <= size <= 4000
            self.reply(b"set", headers=[("Set-Cookie", f"c{index}=" + "v" * size + "; Path=/")
                for index in range(start, start + count)])
        elif path == "/cookie":
            self.reply(self.headers.get("Cookie", "").encode())
        elif path == "/large":
            size = int(self.path.split("?", 1)[1]) if "?" in self.path else 8388608
            self.send_response(200); self.send_header("Content-Length", str(size)); self.end_headers()
            block = b"x" * 65536
            try:
                while size:
                    part = min(size, len(block)); self.wfile.write(block[:part]); size -= part
            except (BrokenPipeError, ConnectionResetError, ssl.SSLError):
                pass
        elif path == "/slow":
            self.send_response(200); self.send_header("Content-Length", "8"); self.end_headers(); self.wfile.flush()
            release.wait(5)
            try: self.wfile.write(b"released")
            except (BrokenPipeError, ConnectionResetError, ssl.SSLError): pass
        elif path == "/release":
            release.set(); self.reply(b"ok")
        elif path == "/delay-headers":
            time.sleep(0.2); self.reply(b"late")
        elif path == "/status":
            self.reply(b"inspectable", status=404)
        else:
            self.reply(b"missing", status=404)
    do_GET = route
    do_POST = route
    do_PUT = route
    do_PATCH = route
    do_DELETE = route
    do_OPTIONS = route
    do_HEAD = route

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", type=Path)
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--gdb", action="store_true", help="Interrupt an isolated debugger run and retain all thread stacks")
    parser.add_argument("--http2", action="store_true", help="Use pinned test-only h2 for real ALPN/multiplexing checks")
    parser.add_argument("--soak-bytes", type=int, default=134217728, help="Actual async pull byte count, at least 128 MiB")
    parser.add_argument("--certificate-tool", type=Path, help="Explicit pinned native certificate fixture tool")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    args.evidence.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="VOID HTTP native ") as temporary:
        work = Path(temporary)
        (work / "openssl.cnf").write_text("[req]\ndistinguished_name = dn\nx509_extensions = extensions\n[dn]\n"
            "[extensions]\nsubjectAltName = DNS:localhost,IP:127.0.0.1\nbasicConstraints = critical,CA:TRUE\n", encoding="ascii")
        openssl = certificate_tool(args.binary, args.certificate_tool)
        generated = subprocess.run([str(openssl), "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
            "-config", str(work / "openssl.cnf"),
            "-keyout", str(work / "key.pem"), "-out", str(work / "cert.pem"), "-subj", "/CN=localhost"], capture_output=True)
        (args.evidence / "certificate.stderr").write_bytes(generated.stderr)
        assert generated.returncode == 0
        p12 = subprocess.run([str(openssl), "pkcs12", "-export", "-in", str(work / "cert.pem"), "-inkey", str(work / "key.pem"),
            "-out", str(work / "identity.p12"), "-passout", "pass:integration"], capture_output=True)
        (args.evidence / "pkcs12.stderr").write_bytes(p12.stderr)
        assert p12.returncode == 0
        server = Server(("127.0.0.1", 0), Handler)
        secure = Server(("127.0.0.1", 0), Handler)
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(work / "cert.pem", work / "key.pem")
        secure.socket = context.wrap_socket(secure.socket, server_side=True)
        mutual = Server(("127.0.0.1", 0), Handler)
        mutual_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        mutual_context.load_cert_chain(work / "cert.pem", work / "key.pem")
        mutual_context.load_verify_locations(work / "cert.pem")
        mutual_context.verify_mode = ssl.CERT_REQUIRED
        mutual.socket = mutual_context.wrap_socket(mutual.socket, server_side=True)
        forward = Server(("127.0.0.1", 0), Proxy); forward.secure = False
        tls_proxy = Server(("127.0.0.1", 0), Proxy); tls_proxy.secure = True
        tls_proxy.socket = context.wrap_socket(tls_proxy.socket, server_side=True)
        socks = socketserver.ThreadingTCPServer(("127.0.0.1", 0), Socks); socks.daemon_threads = True
        for value in [server, secure, mutual, forward, tls_proxy, socks]:
            threading.Thread(target=value.serve_forever, daemon=True).start()
        h2_server = None
        if args.http2:
            sys.path.insert(0, str(root / ".toolchains/http/test-python/site-packages"))
            import h2
            assert h2.__version__ == "4.4.1", "Run tools/setup_http_tests.py for the pinned HTTP/2 fixture"
            h2_server = socketserver.ThreadingTCPServer(("127.0.0.1", 0), H2Handler)
            h2_server.daemon_threads = True
            h2_context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            h2_context.load_cert_chain(work / "cert.pem", work / "key.pem")
            h2_context.set_alpn_protocols(["h2"])
            h2_server.context = h2_context
            threading.Thread(target=h2_server.serve_forever, daemon=True).start()
        try:
            argv = [str(args.binary.resolve()), f"http://127.0.0.1:{server.server_port}",
                f"https://127.0.0.1:{secure.server_port}", str(work / "cert.pem")]
            argv.extend([f"https://127.0.0.1:{h2_server.server_address[1]}" if h2_server else "-",
                f"https://127.0.0.1:{mutual.server_port}", str(work / "cert.pem"), str(work / "key.pem"), str(work / "identity.p12"), str(work),
                f"http://127.0.0.1:{forward.server_port}", f"https://127.0.0.1:{tls_proxy.server_port}", str(socks.server_address[1])])
            if args.gdb:
                argv = ["timeout", "--signal=INT", "--preserve-status", "5", "gdb", "-q", "-batch",
                    "-ex", "set pagination off", "-ex", "run", "-ex", "thread apply all bt", "--args", *argv]
            started = time.monotonic()
            source_hashes = {name: hashlib.sha256((root / name).read_bytes()).hexdigest() for name in
                ["src/runtime/http.cpp", "src/runtime/http_bulk.cpp", "src/runtime/http_file.cpp", "src/runtime/http_resource.cpp", "src/runtime/http_abi.cpp", "src/runtime/native_work.cpp", "src/runtime/async.cpp", "include/void/http.hpp",
                 "tests/net/http/native.cpp", "tests/net/http/runtime.cpp", "tests/net/http/fixture.py",
                 "third_party/curl/VOID-PROVENANCE.json", "third_party/http-versions.json"]}
            try:
                import resource
                stack_limits = list(resource.getrlimit(resource.RLIMIT_STACK))
            except ImportError:
                stack_limits = None
            timed_out = False
            child_environment = os.environ.copy()
            child_environment["SSLKEYLOGFILE"] = str(work / "tls-keys.log")
            child_environment["VOID_HTTP_TEST_SOAK_BYTES"] = str(args.soak_bytes)
            for variable in ["HTTP_PROXY", "http_proxy", "HTTPS_PROXY", "https_proxy", "ALL_PROXY", "all_proxy"]:
                child_environment[variable] = "http://127.0.0.1:1"
            child_environment["NO_PROXY"] = child_environment["no_proxy"] = ""
            with (args.evidence / "native.stdout").open("wb") as out, (args.evidence / "native.stderr").open("wb") as err:
                try:
                    result = subprocess.run(argv, stdout=out, stderr=err, env=child_environment,
                        timeout=(300 if args.soak_bytes > 134217728 else 120) if not args.gdb else 15)
                    status = result.returncode
                except subprocess.TimeoutExpired:
                    timed_out = True
                    status = None
            keylog_created = (work / "tls-keys.log").exists()
            samples = [json.loads(line)["http_async_sample"] for line in (args.evidence / "native.stdout").read_text().splitlines()
                if line.startswith('{"http_async_sample":')]
            (args.evidence / "commands.json").write_text(json.dumps({"argv": argv, "exit_code": status, "timeout": timed_out, "tls_keylog_file_created": keylog_created,
                "seconds": time.monotonic() - started, "binary_sha256": hashlib.sha256(args.binary.read_bytes()).hexdigest(),
                "source_sha256": source_hashes, "soft_hard_stack_bytes": stack_limits,
                "certificate_tool": {"path": str(openssl), "sha256": hashlib.sha256(openssl.read_bytes()).hexdigest()},
                "sanitizers": {key: os.environ.get(key, "") for key in ["ASAN_OPTIONS", "UBSAN_OPTIONS"]},
                "http2_fixture": args.http2,
                "socks_address_types": sorted(set(socks_addresses)),
                "async_pull_bytes": args.soak_bytes,
                "runtime_samples": samples,
                "proxy_digest_events": proxy_events,
                "required_h2_incompatible_requests": required_h2_violations,
                "h2_paused_events": h2_paused_events,
                "h2_retry_events": h2_retry_events,
                "scope": "loopback native transport; not complete source/platform acceptance"}, indent=2) + "\n")
            assert status == 0 and not timed_out, (status, (args.evidence / "native.stderr").read_text())
            assert not keylog_created, "Inherited key logging wrote a file during the native TLS fixture"
            assert not required_h2_violations, "Required HTTP/2 sent an incompatible HTTP request without h2 negotiation"
            if args.http2:
                assert len(h2_retry_events) == 1 and h2_retry_events[0]['bytes'] == 13, ('Automatic mutating request replay', h2_retry_events)
            credits = {}
            for event in h2_paused_events:
                if event['event'] == 'window':
                    key = (event['peer_port'], event['stream'])
                    credits[key] = credits.get(key, 0) + event['delta']
            assert all(value <= 10 * 1024 * 1024 + 65536 for value in credits.values()), \
                ('Paused HTTP/2 stream received duplicated/excess window credit', credits)
            assert 3 in socks_addresses and (1 in socks_addresses or 4 in socks_addresses), "SOCKS local and remote DNS were not both exercised"
            print((args.evidence / "native.stdout").read_text(), end="")
        finally:
            server.shutdown(); secure.shutdown(); server.server_close(); secure.server_close()
            mutual.shutdown(); mutual.server_close()
            for value in [forward, tls_proxy, socks]: value.shutdown(); value.server_close()
            if h2_server: h2_server.shutdown(); h2_server.server_close()

if __name__ == "__main__":
    main()
