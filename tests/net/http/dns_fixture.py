"""Loopback DNS over UDP/TCP for the actual pinned c-ares driver tests."""
import collections
import errno
import socket
import socketserver
import struct
import threading

class DNS:
    def __init__(self):
        self.events = []
        self.lock = threading.Lock()
        owner = self
        class UDP(socketserver.ThreadingUDPServer):
            allow_reuse_address = True
            daemon_threads = True
        class TCP(socketserver.ThreadingTCPServer):
            allow_reuse_address = True
            daemon_threads = True
        class Datagram(socketserver.BaseRequestHandler):
            def handle(self):
                data, stream = self.request
                response = owner.answer(data, 'udp')
                if response is not None: stream.sendto(response, self.client_address)
        class Connection(socketserver.BaseRequestHandler):
            def handle(self):
                self.request.settimeout(5)
                def exact(count):
                    output = b''
                    while len(output) < count:
                        block = self.request.recv(count - len(output))
                        if not block: return None
                        output += block
                    return output
                try:
                    while True:
                        prefix = exact(2)
                        if prefix is None: return
                        data = exact(struct.unpack('!H', prefix)[0])
                        if data is None: return
                        response = owner.answer(data, 'tcp')
                        if response is not None: self.request.sendall(struct.pack('!H', len(response)) + response)
                except (OSError, TimeoutError):
                    pass
        # An ephemeral UDP port may already belong to an unrelated TCP socket.
        # Acquire both before serving, releasing the first on a failed pair.
        for attempt in range(32):
            udp = UDP(('127.0.0.1', 0), Datagram)
            try:
                tcp = TCP(udp.server_address, Connection)
            except OSError as error:
                udp.server_close()
                if error.errno != errno.EADDRINUSE or attempt == 31:
                    raise
            else:
                self.udp, self.tcp = udp, tcp
                break
        for server in [self.udp, self.tcp]:
            threading.Thread(target=server.serve_forever, daemon=True).start()

    @property
    def endpoint(self): return '127.0.0.1:' + str(self.udp.server_address[1])

    @staticmethod
    def name(value):
        return b''.join(bytes([len(label)]) + label.encode('ascii') for label in value.split('.')) + b'\0'

    def answer(self, data, transport):
        if len(data) < 12 or struct.unpack('!H', data[4:6])[0] != 1: return None
        offset = 12; labels = []
        while offset < len(data):
            count = data[offset]; offset += 1
            if not count: break
            if count > 63 or offset + count > len(data): return None
            labels.append(data[offset:offset + count].decode('ascii').lower()); offset += count
        if offset + 4 > len(data): return None
        name = '.'.join(labels)
        kind, category = struct.unpack('!HH', data[offset:offset + 4]); offset += 4
        question = data[12:offset]
        with self.lock: self.events.append({'name': name, 'type': kind, 'transport': transport})
        if name.startswith('stall.'): return None
        flags, answers = 0x8180, []
        if name.startswith('nx.') or category != 1: flags = 0x8183
        elif transport == 'udp' and name.startswith(('tcp.', 'fanout.')): flags |= 0x0200
        elif kind in [1, 28]:
            count = 300 if name.startswith('fanout.') else 1
            for index in range(count):
                if kind == 1:
                    address = socket.inet_pton(socket.AF_INET, '127.0.0.1' if index == 0 else f'127.1.{index // 256}.{index % 256}')
                else:
                    address = socket.inet_pton(socket.AF_INET6, '::1' if index == 0 else f'2001:db8::{index:x}')
                answers.append(b'\xc0\x0c' + struct.pack('!HHIH', kind, 1, 60, len(address)) + address)
        return data[:2] + struct.pack('!HHHHH', flags, 1, len(answers), 0, 0) + question + b''.join(answers)

    def counts(self):
        with self.lock: return collections.Counter((event['name'], event['type'], event['transport']) for event in self.events)

    def close(self):
        for server in [self.udp, self.tcp]: server.shutdown(); server.server_close()
