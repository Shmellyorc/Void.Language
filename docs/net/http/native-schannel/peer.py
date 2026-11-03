"""Loopback mTLS peer. Only public identity/status enters receipts."""
import hashlib
import socket
import ssl
import threading
import subprocess
import json

class Peer:
    def __init__(self, directory, version):
        self.rows = []
        self.expected = ssl.PEM_cert_to_DER_cert((directory/'client.pem').read_text())
        self.context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        self.context.minimum_version = self.context.maximum_version = version
        self.context.load_cert_chain(directory/'server.pem', directory/'server.key')
        self.context.load_verify_locations(directory/'ca.pem')
        self.context.verify_mode = ssl.CERT_REQUIRED
        self.socket = socket.socket()
        self.socket.bind(('127.0.0.1', 0)); self.socket.listen(8); self.socket.settimeout(.2)
        self.port = self.socket.getsockname()[1]
        self.done = threading.Event()
        self.thread = threading.Thread(target=self.run, daemon=True); self.thread.start()
    def run(self):
        while not self.done.is_set():
            try: conn, _ = self.socket.accept()
            except TimeoutError: continue
            except OSError: break
            conn.settimeout(3)
            try:
                with self.context.wrap_socket(conn, server_side=True) as secure:
                    der = secure.getpeercert(binary_form=True)
                    self.rows.append({'event':'authenticated', 'tls':secure.version(),
                                      'sha256':hashlib.sha256(der).hexdigest(),
                                      'exact_der_match':der == self.expected})
                    pending = b''
                    while not self.done.is_set():
                        data = secure.recv(4096)
                        if not data: break
                        pending += data
                        if len(pending) > 16384: raise ValueError('header bound')
                        while b'\r\n\r\n' in pending:
                            _, pending = pending.split(b'\r\n\r\n', 1)
                            secure.sendall(b'HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: keep-alive\r\n\r\nOK')
                            self.rows.append({'event':'request'})
            except (OSError, ValueError) as e:
                self.rows.append({'event':'rejected', 'reason':getattr(e,'reason',type(e).__name__)})
                conn.close()
    def close(self):
        self.done.set();self.socket.close();self.thread.join(timeout=5)

def verify(directory, output, selector, build='build', password=None):
    records=[]
    for label, version in [('1.2',ssl.TLSVersion.TLSv1_2),('1.3',ssl.TLSVersion.TLSv1_3)]:
        peer=Peer(directory,version)
        try:
            cases=[('named',selector,'localhost','ca.pem','store'),
                   ('anonymous','none','localhost','ca.pem','store'),
                   ('wrong_hostname',selector,'127.0.0.1','ca.pem','store'),
                   ('wrong_root',selector,'localhost','wrong-ca.pem','store')]
            if password is not None and label=='1.2':cases.append(('nonpersistent_p12',str(directory/'client.p12'),'localhost','ca.pem','p12'))
            for name,identity,host,ca,kind in cases:
                start=len(peer.rows)
                r=subprocess.run([str(output/build/'peer-client.exe'),f'https://{host}:{peer.port}/',
                                  str(directory/ca),identity,label,kind],input=password+'\n' if kind=='p12' else None,capture_output=True,text=True,timeout=25)
                rows=[json.loads(line) for line in r.stdout.splitlines()]
                records.append({'case':name,'requested_tls':label,'exit':r.returncode,
                                'client':rows,'peer':peer.rows[start:],
                                # Errors may include only synthetic names/loopback endpoint.
                                'error':r.stderr.strip()})
        finally:peer.close()
    return records
