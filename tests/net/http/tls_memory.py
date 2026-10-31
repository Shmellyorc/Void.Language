"""Large unique trust corpus and oversized peer chain using independent TLS peers."""
import argparse
import hashlib
import http.server
import json
from pathlib import Path
import ssl
import subprocess
import tempfile
import threading
import time
import fixture

ROOT = Path(__file__).resolve().parents[3]

class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args): pass
    def do_GET(self):
        self.server.requests += 1
        self.send_response(200); self.send_header('Content-Length', '2')
        self.send_header('Connection', 'close'); self.end_headers(); self.wfile.write(b'ok')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('--evidence', type=Path, required=True)
    args = parser.parse_args(); binary = args.binary.resolve()
    evidence = args.evidence.resolve(); evidence.mkdir(parents=True, exist_ok=True)
    tool = fixture.certificate_tool(binary)
    with tempfile.TemporaryDirectory(prefix='VOID TLS memory ') as directory:
        work = Path(directory); cert, key = work / 'cert.pem', work / 'key.pem'
        cnf = work / 'openssl.cnf'
        cnf.write_text('[req]\ndistinguished_name=dn\nx509_extensions=ext\n[dn]\nCN=localhost\n'
                       '[ext]\nsubjectAltName=IP:127.0.0.1\nbasicConstraints=critical,CA:TRUE\n')
        commands = []
        def run(argv, name, timeout=120):
            start = time.monotonic()
            try:
                result = subprocess.run(list(map(str, argv)), capture_output=True, timeout=timeout, cwd=work)
            except subprocess.TimeoutExpired as failure:
                (evidence / (name + '.stdout')).write_bytes(failure.stdout or b'')
                (evidence / (name + '.stderr')).write_bytes(failure.stderr or b'')
                commands.append(dict(argv=list(map(str, argv)), timeout=True, seconds=time.monotonic()-start))
                (evidence / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
                raise
            if not name.startswith('root-') or result.returncode:
                (evidence / (name + '.stdout')).write_bytes(result.stdout)
                (evidence / (name + '.stderr')).write_bytes(result.stderr)
            commands.append(dict(argv=list(map(str, argv)), exit_code=result.returncode, seconds=time.monotonic()-start))
            if not name.startswith('root-') or result.returncode:
                (evidence / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
            assert result.returncode == 0, (name, result.stderr.decode(errors='replace'))
            return result.stdout
        run([tool, 'req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '1', '-config', cnf,
             '-subj', '/CN=localhost', '-keyout', key, '-out', cert], 'certificate')
        roots = work / 'roots.pem'
        # Generate unique, signed roots in one instrumented process. Repeated
        # crypto-process startup/self-tests dominated the former CLI-per-root fixture.
        generated = json.loads(run([binary, '--make-roots', cert, key, roots], 'make-roots'))
        corpus = cert.read_bytes() + roots.read_bytes()
        # Retain the actual server root while keeping the combined byte budget.
        if len(corpus) > 1048576:
            corpus = corpus[:corpus.rfind(b'-----BEGIN CERTIFICATE-----')]
            generated['roots'] -= 1
        count = generated['roots'] + 1
        roots.write_bytes(corpus)
        (evidence / 'roots.pem').write_bytes(corpus)
        chain = work / 'large-chain.pem'; chain.write_bytes((work/'leaf.pem').read_bytes()+cert.read_bytes()*200)
        peers = []
        for index, path in enumerate((work/'leaf.pem', chain, work/'expired.pem', work/'future.pem', work/'leaf.pem')):
            server = fixture.Server(('127.0.0.1', 0), Handler); server.requests = 0
            context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER); context.load_cert_chain(path, key)
            if index == 4:
                context.minimum_version = ssl.TLSVersion.TLSv1
                context.maximum_version = ssl.TLSVersion.TLSv1_1
                context.set_ciphers('ALL:@SECLEVEL=0')
            server.socket = context.wrap_socket(server.socket, server_side=True)
            threading.Thread(target=server.serve_forever, daemon=True).start(); peers.append(server)
        try:
            run([binary, f'https://127.0.0.1:{peers[0].server_port}/',
                 f'https://127.0.0.1:{peers[1].server_port}/', roots,
                 *[f'https://127.0.0.1:{peer.server_port}/' for peer in peers[2:]]], 'probe', timeout=1200)
            assert peers[0].requests == 256 and all(peer.requests == 0 for peer in peers[1:]), [peer.requests for peer in peers]
            (evidence / 'corpus.json').write_text(json.dumps({'pem_bytes': len(corpus), 'unique_roots': count,
                'peer_chain_pem_bytes': chain.stat().st_size, 'successful_requests': peers[0].requests,
                'oversized_chain_http_requests': peers[1].requests}, indent=2) + '\n')
        finally:
            for server in peers: server.shutdown(); server.server_close()
    print('TLS provider corpus/flight/retained-memory checks passed')

if __name__ == '__main__': main()
