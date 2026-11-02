"""Compile and execute the published HTTP examples against local independent peers."""
import argparse
import json
import os
from pathlib import Path
import ssl
import select
import subprocess
import tempfile
import threading
import time
import fixture

ROOT = Path(__file__).resolve().parents[3]

class Handler(fixture.Handler):
    def do_GET(self):
        if self.path.startswith('/json'):
            self.reply(b'{"message":"Example JSON complete"}', headers=[('Content-Type', 'application/json')])
        elif self.path.startswith('/broken-json'):
            self.reply(b'{bad', headers=[('Content-Type', 'application/json')])
        else:
            super().do_GET()

class CancellationHandler(fixture.Handler):
    def do_GET(self):
        peer = self.server
        peer.events.append((self.path, 'received', time.monotonic() - peer.started))
        if self.path == '/slow':
            self.send_response(200); self.send_header('Content-Length', '8'); self.end_headers(); self.wfile.flush()
            peer.pending.set()
            # No automatic completion and no timed sleep posing as readiness.
            # Observe native close before fixture teardown, not just host exit 1.
            while not peer.stopping.is_set():
                if not select.select([self.connection], [], [], 0.1)[0]: continue
                try: data = self.connection.recv(1)
                except ConnectionResetError: data = b''
                if not data:
                    peer.events.append((self.path, 'closed', time.monotonic() - peer.started))
                    peer.closed.set(); break
                raise AssertionError('unexpected request bytes on held connection')
            self.close_connection = True
        elif self.path == '/plain':
            assert peer.pending.wait(3), 'barrier reached without the pending request'
            if peer.fail_barrier: time.sleep(0.06)
            self.reply(b'pending request observed')
            peer.events.append((self.path, 'barrier', time.monotonic() - peer.started))
        else:
            raise AssertionError(self.path)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('compiler', type=Path)
    parser.add_argument('host', type=Path)
    parser.add_argument('--evidence', type=Path, required=True)
    args = parser.parse_args()
    compiler, host = args.compiler.resolve(), args.host.resolve()
    evidence = args.evidence.resolve(); evidence.mkdir(parents=True, exist_ok=True)
    commands = []
    with tempfile.TemporaryDirectory(prefix='VOID HTTP examples ') as directory:
        work = Path(directory)
        def run(argv, target='', expected='', code=0, extra=None, error=''):
            env = os.environ.copy()
            env.update(VOID_HTTP_URL=target, VOID_HTTP_TOKEN='exact-token')
            env.pop('VOID_HTTP_CA', None)
            env.update(extra or {})
            start = time.monotonic()
            result = subprocess.run(list(map(str, argv)), cwd=work, env=env, capture_output=True, timeout=120)
            number = len(commands)
            (evidence / f'{number}.stdout').write_bytes(result.stdout)
            (evidence / f'{number}.stderr').write_bytes(result.stderr)
            commands.append(dict(argv=list(map(str, argv)), exit_code=result.returncode,
                                 seconds=time.monotonic() - start))
            (evidence / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
            assert result.returncode == code, (argv, result.stderr.decode(errors='replace'))
            assert expected in result.stdout.decode(), (argv, result.stdout)
            assert error in result.stderr.decode(), (argv, result.stderr)
            return result
        tool = fixture.certificate_tool(compiler)
        config = work / 'openssl.cnf'
        config.write_text('[req]\ndistinguished_name=dn\nx509_extensions=ext\n[dn]\nCN=localhost\n'
                          '[ext]\nsubjectAltName=IP:127.0.0.1\nbasicConstraints=critical,CA:TRUE\n')
        cert, key = work / 'cert.pem', work / 'key.pem'
        run([tool, 'req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '1',
             '-config', config, '-subj', '/CN=localhost', '-keyout', key, '-out', cert])
        plain = fixture.Server(('127.0.0.1', 0), Handler)
        secure = fixture.Server(('127.0.0.1', 0), Handler)
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER); context.load_cert_chain(cert, key)
        secure.socket = context.wrap_socket(secure.socket, server_side=True)
        for server in (plain, secure):
            threading.Thread(target=server.serve_forever, daemon=True).start()
        base = f'http://127.0.0.1:{plain.server_port}'
        try:
            sources = ROOT / 'examples/net_http'
            for path in sorted(path for path in sources.glob('*.void') if path.is_file()):
                run([compiler, 'check', path])
            cases = [('api_json', base + '/json', 'Example JSON complete'),
                     ('authenticated_post', f'https://127.0.0.1:{secure.server_port}/auth-bearer', 'HTTP 200;'),
                     ('files', base, 'Native download, upload and multipart complete')]
            for name, target, expected in cases:
                path = sources / (name + '.void')
                extra = {'VOID_HTTP_CA': str(cert)} if name == 'authenticated_post' else {}
                run([compiler, 'run', path], target, expected, extra=extra)
                binary = work / (name + ('.exe' if os.name == 'nt' else ''))
                run([compiler, 'build', path, '-o', binary])
                run([binary], target, expected, extra=extra)
            # Failed JSON is classified after a successful HTTP response.
            run([work / ('api_json.exe' if os.name == 'nt' else 'api_json')],
                base + '/broken-json', 'JSON ', code=1)
            run([host, sources / 'streaming_async.void'], base, 'Streamed 8388608 bytes')
            for fail_barrier in (False, True):
                peer = fixture.Server(('127.0.0.1', 0), CancellationHandler)
                peer.pending, peer.closed, peer.stopping = threading.Event(), threading.Event(), threading.Event()
                peer.events, peer.started, peer.fail_barrier = [], time.monotonic(), fail_barrier
                threading.Thread(target=peer.serve_forever, daemon=True).start()
                source = sources / 'cancellation.void'
                if fail_barrier:
                    # Lower only this negative control's header deadline. It
                    # must return an HTTP error before reaching pending.cancel.
                    source = work / 'cancellation-timeout.void'
                    source.write_text((sources / 'cancellation.void').read_text().replace(
                        'owner.get_async(base + "/plain")',
                        'owner.get_async(base + "/plain", http.request_options(header_timeout_ms = 20))'))
                try:
                    result = run([host, source], f'http://127.0.0.1:{peer.server_port}',
                                 code=1, error='' if fail_barrier else 'Async task cancelled')
                    if fail_barrier:
                        assert b'timeout' in result.stdout and b'headers' in result.stdout, result.stdout
                        assert not result.stderr, result.stderr
                    else:
                        assert not result.stdout, result.stdout
                    assert peer.pending.is_set() and peer.closed.wait(3), 'held request did not close before teardown'
                finally:
                    (evidence / ('cancellation-timeout-peer.json' if fail_barrier else 'cancellation-peer.json')).write_text(
                        json.dumps(peer.events, indent=2) + '\n')
                    peer.stopping.set(); peer.shutdown(); peer.server_close()
        finally:
            fixture.release.set()
            for server in (plain, secure): server.shutdown(); server.server_close()
    print(f'HTTP published examples: {len(commands)} checks passed')

if __name__ == '__main__': main()
