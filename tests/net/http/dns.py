"""Run the actual c-ares transport against loopback UDP/TCP DNS and HTTP peers."""
import argparse
import hashlib
import json
import os
from pathlib import Path
try:
    import resource
except ImportError:
    resource = None
import socket
import subprocess
import tempfile
import threading
import time
import fixture
from dns_fixture import DNS

ROOT = Path(__file__).resolve().parents[3]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('binary', type=Path)
    parser.add_argument('--evidence', type=Path, required=True)
    args = parser.parse_args(); args.binary = args.binary.resolve()
    args.evidence.mkdir(parents=True, exist_ok=True)
    class Closing(fixture.Handler):
        def reply(self, body, status=200, headers=()):
            super().reply(body, status, [*headers, ('Connection', 'close')])
            self.close_connection = True
    class IPv6(fixture.Server): address_family = socket.AF_INET6
    dns = DNS(); servers = [fixture.Server(('127.0.0.1', 0), Closing)]
    try:
        servers.append(IPv6(('::1', 0), Closing))
        for server in servers: threading.Thread(target=server.serve_forever, daemon=True).start()
        argv = [str(args.binary), str(servers[0].server_port), dns.endpoint, str(servers[1].server_port)]
        def stack(): resource.setrlimit(resource.RLIMIT_STACK, (1048576, 1048576))
        started = time.monotonic()
        with (args.evidence / 'native.stdout').open('w') as out, (args.evidence / 'native.stderr').open('w') as err:
            try:
                result = subprocess.run(argv, stdout=out, stderr=err, preexec_fn=stack if resource else None, timeout=120)
                status, timed_out = result.returncode, False
            except subprocess.TimeoutExpired: status, timed_out = None, True
        report = {'argv': argv, 'exit_code': status, 'timeout': timed_out, 'seconds': time.monotonic() - started,
            'stack_bytes': [1048576, 1048576] if resource else None, 'binary_sha256': hashlib.sha256(args.binary.read_bytes()).hexdigest(),
            'source_sha256': {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in
                ['src/runtime/http.cpp', 'include/void/detail/http_hooks.hpp', 'tests/net/http/dns.cpp', 'tests/net/http/dns_fixture.py']},
            'sanitizers': {name: os.environ.get(name, '') for name in ['ASAN_OPTIONS', 'UBSAN_OPTIONS']}, 'dns_events': dns.events}
        (args.evidence / 'commands.json').write_text(json.dumps(report, indent=2) + '\n')
        assert status == 0 and not timed_out, (status, (args.evidence / 'native.stderr').read_text())
        counts = dns.counts()
        assert counts['warm.void.test', 1, 'udp'] == 2, ('cache reuse/finite eviction', counts)
        assert counts['nx.void.test', 1, 'udp'] == 1, ('negative cache', counts)
        assert counts['tcp.void.test', 1, 'udp'] and counts['tcp.void.test', 1, 'tcp'], ('TCP fallback', counts)
        assert counts['xn--bcher-kva.void.test', 1, 'udp'], ('actual canonical IDN query', counts)
        assert counts['v6.void.test', 28, 'udp'], ('actual AAAA lookup', counts)
        print((args.evidence / 'native.stdout').read_text(), end='')
    finally:
        dns.close()
        for server in servers: server.shutdown(); server.server_close()

if __name__ == '__main__': main()
