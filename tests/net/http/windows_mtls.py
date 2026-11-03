"""Real Windows public API/JIT/AOT, exact peer identity and credential ownership."""
import argparse
import contextlib
import hashlib
import json
import os
from pathlib import Path
import secrets
import ssl
import subprocess
import tempfile
import threading
import time

import fixture
from windows_identity import WindowsIdentities

ROOT = Path(__file__).resolve().parents[3]


class Peer(fixture.Server):
    def get_request(self):
        try:
            stream, address = super().get_request()
        except ssl.SSLError as error:
            self.events.append({'event': 'handshake_rejected', 'reason': error.reason})
            raise
        certificate = stream.getpeercert(binary_form=True)
        exact = certificate == self.expected
        self.events.append({'event': 'authenticated' if exact else 'identity_rejected',
                            'exact_der_match': exact, 'tls': stream.version(),
                            'client_sha256': hashlib.sha256(certificate).hexdigest() if certificate else None})
        if not exact:
            stream.close()
            raise ssl.SSLError('wrong test client DER')
        return stream, address


class Handler(fixture.Handler):
    def do_GET(self):
        self.server.events.append({'event': 'request', 'path': self.path, 'connection': self.client_address[1]})
        if self.path == '/hold':
            self.send_response(200); self.send_header('Content-Length', '20'); self.end_headers()
            self.wfile.write(b'first'); self.wfile.flush()
            self.server.release.wait(30)
        else:
            self.reply(b'authenticated')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('compiler', type=Path); parser.add_argument('runner', type=Path)
    parser.add_argument('helper', type=Path); parser.add_argument('--evidence', type=Path, required=True)
    args = parser.parse_args()
    if os.name != 'nt':
        raise RuntimeError('Native Windows execution is required; no skipped acceptance')
    args.evidence.mkdir(parents=True, exist_ok=True)
    tool = fixture.certificate_tool(args.compiler)
    commands, peers = [], []
    environment = os.environ.copy()
    for name in list(environment):
        if name.lower() in ('http_proxy', 'https_proxy', 'all_proxy', 'no_proxy', 'sslkeylogfile'):
            environment.pop(name)

    def save():
        (args.evidence / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
        (args.evidence / 'peers.json').write_text(json.dumps([p.events for p in peers], indent=2) + '\n')

    def run(argv, cwd, expected=None, requests=False):
        index = len(commands); before = [len(p.events) for p in peers]; start = time.monotonic()
        result = subprocess.run(list(map(str, argv)), cwd=cwd, env=environment,
                                capture_output=True, timeout=150)
        (args.evidence / (str(index) + '.stdout')).write_bytes(result.stdout)
        (args.evidence / (str(index) + '.stderr')).write_bytes(result.stderr)
        events = [p.events[n:] for p, n in zip(peers, before)]
        commands.append({'argv': list(map(str, argv)), 'exit': result.returncode,
                         'seconds': round(time.monotonic() - start, 3), 'peer_events': events})
        save()
        assert result.returncode == 0, (argv, result.stderr.decode(errors='replace'))
        if expected is not None:
            assert result.stdout.decode().replace('\r\n', '\n') == expected
        if requests:
            for rows in events:
                first = [r for r in rows if r.get('path') == '/first']
                reuse = [r for r in rows if r.get('path') == '/reuse']
                assert first and len(first) == len(reuse)
                assert [r['connection'] for r in first] == [r['connection'] for r in reuse], 'connection reuse lost'
                assert any(r.get('exact_der_match') and r.get('tls') in ('TLSv1.2', 'TLSv1.3') for r in rows)
            assert any(r['event'] == 'identity_rejected' for r in events[0]), 'wrong identity not exercised'
            assert any(r['event'] == 'handshake_rejected' for r in events[0]), 'anonymous not rejected'
            assert not any(r.get('path', '').startswith(('/wrong-', '/anonymous', '/must-not-')) for rows in events for r in rows)

    with tempfile.TemporaryDirectory(prefix='VOID Windows store acceptance ') as temporary:
        work = Path(temporary)
        with WindowsIdentities(args.helper, work, tool, args.evidence) as credentials:
            # Match VOID's native-link security contract without changing the
            # ambient VM temp directory or weakening its ownership/ACL check.
            environment['TEMP'] = environment['TMP'] = str(work)
            config = '[req]\ndistinguished_name=dn\nx509_extensions=ext\n[dn]\n[ext]\nsubjectAltName=DNS:localhost\nbasicConstraints=critical,CA:TRUE\n'
            (work / 'openssl.cnf').write_text(config)
            credentials.tool('req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '1', '-config', 'openssl.cnf',
                             '-keyout', 'key.pem', '-out', 'cert.pem', '-subj', '/CN=localhost')
            credentials.tool('req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '1', '-config', 'openssl.cnf',
                             '-keyout', 'wrong.key', '-out', 'wrong.pem', '-subj', '/CN=unrelated')
            credentials.tool('pkcs12', '-export', '-in', 'cert.pem', '-inkey', 'key.pem',
                             '-out', 'identity.p12', '-passout', 'pass:integration')
            rsa = credentials.provision('rsa', 'RSA'); ec = credentials.provision('ecdsa', 'ECDSA')
            unusable = credentials.provision('public-only', 'RSA', public_only=True)
            try:
                for item in (rsa, ec):
                    peer = Peer(('127.0.0.1', 0), Handler); peers.append(peer)
                    peer.events = []; peer.release = threading.Event(); peer.expected = item['certificate'].read_bytes()
                    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER); context.minimum_version = ssl.TLSVersion.TLSv1_2
                    context.load_cert_chain(work / 'cert.pem', work / 'key.pem'); context.load_verify_locations(work / 'cert.pem')
                    context.verify_mode = ssl.CERT_REQUIRED; peer.socket = context.wrap_socket(peer.socket, server_side=True)
                    threading.Thread(target=peer.serve_forever, daemon=True).start()
                values = {'CA': str(work / 'cert.pem'), 'WRONG_CA': str(work / 'wrong.pem'), 'P12': str(work / 'identity.p12'),
                          'RSA_REF': rsa['reference'], 'ECDSA': ec['reference'], 'UNUSABLE': unusable['reference'],
                          'MISSING_STORE': 'CurrentUser\\VOID-HTTP-MISSING-' + secrets.token_hex(32).upper() + '\\' + '0' * 40,
                          'MISSING_CERT': rsa['reference'][:-40] + '0' * 40,
                          'RSA': 'https://localhost:' + str(peers[0].server_port), 'EC': 'https://localhost:' + str(peers[1].server_port),
                          'WRONG_HOST': 'https://127.0.0.1:' + str(peers[0].server_port)}
                template = (ROOT / 'tests/net/http/windows_store.void').read_text()
                for key, value in values.items(): template = template.replace('@' + key + '@', value.replace('\\', '\\\\'))
                sync_cancel = '''with cancellation.start(http.request(target = "@RSA@/hold"))? as exchange:
            var info = exchange.response()?
            var prefix = exchange.read_bytes(1)?
            if info.status != 200 or prefix.bytes.count != 1:
                return Ok(false)
            var closed = exchange.close()?
            var again = exchange.close()?'''.replace('@RSA@', values['RSA'])
                async_cancel = '''var pending = cancellation.get_async("@RSA@/hold")
        var barrier = (await cancellation.get_async("@RSA@/barrier"))?
        var closed = (await cancellation.close_async())?
        if (await pending) is Err(var failure):
            if not (failure.kind is http.error_kind.closed):
                return Ok(false)
        else:
            return Ok(false)'''.replace('@RSA@', values['RSA'])
                source = template.replace('@ASYNC@', '').replace('@AWAIT@', '').replace('@SUFFIX@', '').replace('@CANCELLATION@', sync_cancel)
                top = 'if suite() is Ok(var passed):\n    print(passed)\nelse:\n    print(false)\nreturn 0\n'
                path = work / 'store.void'; path.write_text(source.replace('@ENTRY@', top))
                (args.evidence / path.name).write_text(path.read_text())
                run([args.compiler, 'check', path], work)
                for label, flags in [('normal', ['--no-opt']), ('O2', [])]:
                    run([args.compiler, 'run', path, *flags], work, 'true\n', requests=True)
                    binary = work / ('store-' + label + '.exe')
                    run([args.compiler, 'build', path, '-o', binary, *flags], work)
                    run([binary], work, 'true\n', requests=True)
                entry = 'func entry(input: i32) -> i32:\n    if suite() is Ok(var passed):\n        if passed:\n            return input\n    return 1\n'
                path = work / 'store-gc.void'; path.write_text(source.replace('@ENTRY@', entry))
                run([args.runner, '--tls', path], work, requests=True)
                source = template.replace('@ASYNC@', 'async ').replace('@AWAIT@', 'await ').replace('@SUFFIX@', '_async').replace('@CANCELLATION@', async_cancel)
                entry = 'async func entry(input: i32) -> i32:\n    if (await suite()) is Ok(var passed):\n        if passed:\n            return input\n    return 1\n'
                path = work / 'store-async.void'; path.write_text(source.replace('@ENTRY@', entry))
                (args.evidence / path.name).write_text(path.read_text())
                run([args.compiler, 'check', path], work)
                run([args.runner, '--tls-async', path], work, requests=True)
                manifest = (args.compiler.parent / 'void-native.txt').read_text().splitlines()
                flags = ['--driver-mode=g++', *manifest[5:]]
                if manifest[2]: flags += ['--ld-path=' + manifest[2]]
                for mode in ('normal', 'optimized'):
                    obj = work / ('store-async-' + mode + '.o'); binary = obj.with_suffix('.exe')
                    run([args.runner, '--emit-async', path, obj, mode], work)
                    run([manifest[1], obj, *manifest[3:5], *flags, '-o', binary], work)
                    run([binary, '7'], work, '7\n', requests=True)
            finally:
                for peer in peers: peer.release.set(); peer.shutdown(); peer.server_close()
                save()
    print('Windows public store identity: RSA/ECDSA, negatives, reuse, cancellation, GC/shutdown and normal/O2 JIT/AOT passed')


if __name__ == '__main__': main()
