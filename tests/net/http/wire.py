"""Independent literal HTTP/1 wire peer; never uses the production HTTP parser."""
import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
try:
    import resource
except ImportError:
    resource = None
import socketserver
import subprocess
import threading
import time
import zlib

ROOT = Path(__file__).resolve().parents[3]

class Peer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True

class Handler(socketserver.BaseRequestHandler):
    def handle(self):
        self.request.settimeout(5)
        try:
            data = b''
            while b'\r\n\r\n' not in data:
                chunk = self.request.recv(4096)
                if not chunk: return
                data += chunk
                if len(data) > 65536: return
            header = data.split(b'\r\n\r\n')[0]
            method, path, _ = header.split(b'\r\n')[0].decode('ascii').split(' ')
            def fixed(body, fields=b'', status=b'200 OK'):
                return b'HTTP/1.1 ' + status + b'\r\nContent-Length: ' + str(len(body)).encode() + b'\r\nConnection: close\r\n' + fields + b'\r\n' + body
            if path == '/chunked': output = b'HTTP/1.1 200 OK\r\nConnection: close\r\nTransfer-Encoding: chunked\r\nTrailer: X-End\r\n\r\n5\r\nhello\r\n0\r\nX-End: done\r\n\r\n'
            elif path == '/large-head-trailer': output = b'HTTP/1.1 200 OK\r\nConnection: close\r\nX-Pad: ' + b'p' * 60000 + b'\r\nTransfer-Encoding: chunked\r\n\r\n1\r\nx\r\n0\r\n' + (b'X-End: ' + b't' * 2000 + b'\r\n') * 4 + b'\r\n'
            elif path == '/large-trailer-field': output = b'HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n0\r\nX-End: ' + b't' * 4096 + b'\r\n\r\n'
            elif path == '/http10': output = b'HTTP/1.0 200 OK\r\n\r\nold'
            elif path == '/conflicting-length': output = b'HTTP/1.1 200 OK\r\nContent-Length: 3\r\nContent-Length: 4\r\n\r\nabc'
            elif path == '/te-cl': output = b'HTTP/1.1 200 OK\r\nContent-Length: 3\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nabc\r\n0\r\n\r\n'
            elif path == '/switching': output = b'HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n\r\n'
            elif path == '/interim-overflow': output = b'HTTP/1.1 100 Continue\r\n\r\n' * 17 + fixed(b'final')
            elif path == '/forbidden-trailer': output = b'HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nabc\r\n0\r\nContent-Length: 3\r\n\r\n'
            elif path == '/large-field': output = fixed(b'', b'X-Pad: ' + b'p' * 66000 + b'\r\n')
            elif path == '/field-overflow': output = fixed(b'', b'X-One: p\r\n' * 129)
            elif path == '/trailer-overflow': output = b'HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n' + b'X-One: t\r\n' * 33 + b'\r\n'
            elif path == '/partial-length': output = b'HTTP/1.1 200 OK\r\nContent-Length: 8\r\n\r\nabc'
            elif path == '/partial-chunk': output = b'HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n4\r\nabc'
            elif path == '/gzip-crc':
                encoded = bytearray(gzip.compress(b'clear')); encoded[-8] ^= 1
                output = fixed(encoded, b'Content-Encoding: gzip\r\n')
            elif path == '/gzip-junk': output = fixed(gzip.compress(b'clear') + b'junk', b'Content-Encoding: gzip\r\n')
            elif path == '/gzip-empty': output = fixed(gzip.compress(b''), b'Content-Encoding: gzip\r\n')
            elif path == '/raw-deflate': output = fixed(zlib.compress(b'clear', wbits=-15), b'Content-Encoding: deflate\r\n')
            elif path == '/unknown-encoding': output = fixed(b'opaque', b'Content-Encoding: br\r\n')
            elif path == '/stacked-encoding': output = fixed(b'opaque', b'Content-Encoding: gzip, deflate\r\n')
            elif path == '/stall-body':
                self.request.sendall(b'HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\na'); time.sleep(.1); return
            elif path == '/loop': output = fixed(b'', b'Location: /loop\r\n', b'302 Found')
            elif path == '/cross': output = fixed(b'', f'Location: http://127.0.0.1:{self.server.other_port}/echo-header\r\n'.encode(), b'302 Found')
            elif path == '/echo-header': output = fixed(next((line.split(b':', 1)[1].strip() for line in header.split(b'\r\n')[1:] if line.lower().startswith(b'x-secret:')), b''))
            elif path == '/early-303': output = fixed(b'', b'Location: /final\r\n', b'303 See Other')
            elif path == '/final': output = fixed(b'get' if method == 'GET' else b'wrong-method')
            else: output = fixed(b'unknown', status=b'404 Not Found')
            self.request.sendall(output)
        except (OSError, TimeoutError): pass

def main():
    parser = argparse.ArgumentParser(); parser.add_argument('binary', type=Path); parser.add_argument('--evidence', type=Path, required=True)
    args = parser.parse_args(); args.binary = args.binary.resolve(); args.evidence.mkdir(parents=True, exist_ok=True)
    servers = [Peer(('127.0.0.1', 0), Handler), Peer(('127.0.0.1', 0), Handler)]
    for index, peer in enumerate(servers):
        peer.other_port = servers[1-index].server_address[1]
        threading.Thread(target=peer.serve_forever, daemon=True).start()
    try:
        argv = [str(args.binary), f'http://127.0.0.1:{servers[0].server_address[1]}']
        def stack(): resource.setrlimit(resource.RLIMIT_STACK, (1048576, 1048576))
        with (args.evidence / 'native.stdout').open('w') as out, (args.evidence / 'native.stderr').open('w') as err:
            result = subprocess.run(argv, stdout=out, stderr=err, preexec_fn=stack if resource else None, timeout=120)
        (args.evidence / 'commands.json').write_text(json.dumps({'argv': argv, 'exit_code': result.returncode,
            'stack_bytes': [1048576,1048576] if resource else None, 'binary_sha256': hashlib.sha256(args.binary.read_bytes()).hexdigest(),
            'source_sha256': {name: hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in
                ['src/runtime/http.cpp', 'tests/net/http/wire.cpp', 'tests/net/http/wire.py']},
            'sanitizers': {key: os.environ.get(key, '') for key in ['ASAN_OPTIONS','UBSAN_OPTIONS']}},indent=2)+'\n')
        assert result.returncode == 0, (args.evidence / 'native.stderr').read_text()
        print((args.evidence / 'native.stdout').read_text(), end='')
    finally:
        for peer in servers: peer.shutdown(); peer.server_close()

if __name__ == '__main__': main()
