"""Independent counting/hash peers for bounded public native bulk IO, including >4 GiB offsets."""
import argparse
import hashlib
import http.server
import json
import os
from pathlib import Path
import subprocess
import tempfile
import threading
import time

ROOT = Path(__file__).resolve().parents[3]
ZERO = bytes(65536)
events = []

class Server(http.server.ThreadingHTTPServer):
    daemon_threads = True

class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'
    def log_message(self, *args): pass
    def do_GET(self):
        self.send_response(200); self.send_header('Content-Length', str(self.server.download)); self.end_headers()
        remaining = self.server.download
        try:
            while remaining:
                size = min(remaining, len(ZERO)); self.wfile.write(ZERO[:size]); remaining -= size
        except (BrokenPipeError, ConnectionResetError): return
        events.append({'download': self.server.download})
    def do_POST(self):
        self.connection.settimeout(120)
        declared = int(self.headers['Content-Length'])
        assert declared == self.server.upload
        remaining = declared; digest = hashlib.sha256()
        while remaining:
            block = self.rfile.read(min(remaining, len(ZERO)))
            assert block, 'truncated upload'
            digest.update(block); remaining -= len(block)
        value = digest.hexdigest()
        assert value == self.server.expected, (declared, value)
        events.append({'uploaded': declared, 'sha256': value})
        encoded = value.encode()
        self.send_response(200); self.send_header('Content-Length', str(len(encoded))); self.end_headers()
        self.wfile.write(encoded)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('compiler', type=Path)
    parser.add_argument('runner', type=Path)
    parser.add_argument('--evidence', type=Path, required=True)
    parser.add_argument('--upload-bytes', type=int, default=67108865)
    parser.add_argument('--download-bytes', type=int, default=67108864)
    args = parser.parse_args()
    assert args.upload_bytes > 0 and args.download_bytes > 0
    evidence = args.evidence.resolve(); evidence.mkdir(parents=True, exist_ok=True)
    compiler, runner = args.compiler.resolve(), args.runner.resolve()
    digest = hashlib.sha256()
    remaining = args.upload_bytes - 1
    while remaining:
        size = min(remaining, len(ZERO)); digest.update(ZERO[:size]); remaining -= size
    digest.update(b'Z'); expected = digest.hexdigest()
    server = Server(('127.0.0.1', 0), Handler)
    server.upload, server.download, server.expected = args.upload_bytes, args.download_bytes, expected
    threading.Thread(target=server.serve_forever, daemon=True).start()
    commands = []
    with tempfile.TemporaryDirectory(prefix='VOID HTTP bulk ') as directory:
        work = Path(directory); original = work / 'sparse.bin'; destination = work / 'download.bin'
        with original.open('wb') as output:
            output.seek(args.upload_bytes - 1); output.write(b'Z')
        source = (ROOT / 'tests/net/http/bulk.void').read_text()
        for key, value in {'BASE': f'http://127.0.0.1:{server.server_port}', 'SOURCE': original,
                           'DESTINATION': destination, 'UPLOAD': args.upload_bytes,
                           'DOWNLOAD': args.download_bytes, 'HASH': expected}.items():
            source = source.replace('@' + key + '@', str(value).replace('\\', '\\\\'))
        path = work / 'bulk.void'; path.write_text(source); (evidence / 'bulk.void').write_text(source)
        def run(argv, expected_stdout=None):
            start = time.monotonic(); cpu_before = os.times().children_user + os.times().children_system
            number = len(commands); samples = []
            with (evidence / f'{number}.stdout').open('w') as out, (evidence / f'{number}.stderr').open('w') as err:
                with subprocess.Popen(list(map(str, argv)), cwd=work, stdout=out, stderr=err) as child:
                    while child.poll() is None:
                        try:
                            for line in Path(f'/proc/{child.pid}/status').read_text().splitlines():
                                if line.startswith('VmRSS:'): samples.append([time.monotonic() - start, int(line.split()[1])])
                        except FileNotFoundError: pass
                        if time.monotonic() - start > 300: child.kill(); child.wait(); raise RuntimeError('bulk deadline')
                        time.sleep(0.02)
            commands.append({'argv': list(map(str, argv)), 'exit_code': child.returncode,
                             'seconds': time.monotonic() - start,
                             'child_cpu_seconds': os.times().children_user + os.times().children_system - cpu_before,
                             'rss_kib': samples})
            (evidence / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
            assert child.returncode == 0, (evidence / f'{number}.stderr').read_text()
            if expected_stdout is not None: assert (evidence / f'{number}.stdout').read_text() == expected_stdout
        try:
            run([compiler, 'check', path])
            # Use the compiler's established async adapter and independently link the verified native closure.
            manifest = (compiler.parent / 'void-native.txt').read_text().splitlines()
            assert manifest[0] == 'VOID native 23.1.4 v2'
            obj, binary = work / 'bulk.o', work / ('bulk.exe' if os.name == 'nt' else 'bulk')
            run([runner, '--emit-async', path, obj, 'optimized'])
            flags = ['--driver-mode=g++', *manifest[5:]]
            if manifest[2]: flags += ['--ld-path=' + manifest[2]]
            run([manifest[1], obj, *manifest[3:5], *flags, '-o', binary])
            (evidence / 'artifacts.json').write_text(json.dumps({'object_bytes': obj.stat().st_size,
                'executable_bytes': binary.stat().st_size}, indent=2)+'\n')
            run([binary, '0'], '0\n')
            assert len(events) == 2 and destination.stat().st_size == args.download_bytes
            with destination.open('rb') as output:
                for offset in (0, args.download_bytes // 2, args.download_bytes - 1):
                    output.seek(offset); assert output.read(1) == b'\0'
            (evidence / 'peer.json').write_text(json.dumps(events, indent=2) + '\n')
            print(f'Public bulk: download {args.download_bytes}, upload {args.upload_bytes}, SHA-256 {expected}')
        finally:
            server.shutdown(); server.server_close()

if __name__ == '__main__': main()
