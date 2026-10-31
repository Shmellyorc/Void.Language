"""Compile and execute the published HTTP examples against local independent peers."""
import argparse
import json
import os
from pathlib import Path
import ssl
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
            run([host, sources / 'cancellation.void'], base, code=1, error='Async task cancelled')
        finally:
            fixture.release.set()
            for server in (plain, secure): server.shutdown(); server.server_close()
    print(f'HTTP published examples: {len(commands)} checks passed')

if __name__ == '__main__': main()
