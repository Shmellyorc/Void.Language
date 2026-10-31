"""Reproduce DNS fixture port-pair collision/cleanup without changing the driver."""
import errno
import json
from pathlib import Path
import socketserver
import sys
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / 'tests/net/http'))
from dns_fixture import DNS

rows = []
for label, conflicts, failure in [('retry', 1, errno.EADDRINUSE),
                                  ('bounded', 32, errno.EADDRINUSE),
                                  ('other-error', 1, errno.EACCES)]:
    calls, closed = [], []
    real_bind = socketserver.ThreadingTCPServer.server_bind
    real_close = socketserver.ThreadingUDPServer.server_close
    def bind(server):
        calls.append(server)
        if len(calls) <= conflicts:
            raise OSError(failure, 'controlled port-pair probe')
        return real_bind(server)
    def close(server):
        real_close(server)
        closed.append(server)
        assert server.socket.fileno() == -1
    with patch.object(socketserver.ThreadingTCPServer, 'server_bind', bind), \
         patch.object(socketserver.ThreadingUDPServer, 'server_close', close):
        if label == 'retry':
            dns = DNS()
            try:
                assert len(calls) == 2 and len(closed) == 1
                assert dns.udp.server_address == dns.tcp.server_address
            finally:
                dns.close()
        else:
            try:
                DNS()
            except OSError as error:
                assert error.errno == failure
            else:
                raise AssertionError('expected startup failure')
            assert len(calls) == conflicts and len(closed) == conflicts
    rows.append({'case': label, 'tcp_attempts': len(calls),
                 'udp_sockets_closed': len(closed), 'passed': True})
print(json.dumps(rows, indent=2))
