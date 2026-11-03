"""Bounded loopback h1/h2 and HTTPS CONNECT peers; public identity receipts."""
import hashlib
import select
import socket
import ssl
import threading
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0,str(ROOT/'.toolchains/native-awslc/python'))
import h2.config
import h2.connection
import h2.events

class Peer:
    def __init__(self,directory,version,protocol='h1',proxy=False,require_client=True):
        self.rows=[];self.proxy=proxy;self.done=threading.Event();self.workers=[]
        self.expected=ssl.PEM_cert_to_DER_cert((directory/'client.pem').read_text())
        self.context=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        self.context.minimum_version=self.context.maximum_version=version
        self.context.load_cert_chain(directory/'server.pem',directory/'server.key')
        self.context.set_alpn_protocols(['h2' if protocol=='h2' else 'http/1.1'])
        self.context.load_verify_locations(directory/'ca.pem')
        self.context.verify_mode=ssl.CERT_REQUIRED if require_client else ssl.CERT_NONE
        self.socket=socket.socket();self.socket.bind(('127.0.0.1',0));self.socket.listen(16);self.socket.settimeout(.2)
        self.port=self.socket.getsockname()[1]
        self.thread=threading.Thread(target=self.run,daemon=True);self.thread.start()
    def run(self):
        while not self.done.is_set():
            try: conn,_=self.socket.accept()
            except TimeoutError: continue
            except OSError: break
            worker=threading.Thread(target=self.serve,args=(conn,),daemon=True);self.workers.append(worker);worker.start()
    def serve(self,conn):
        conn.settimeout(4)
        try:
            with self.context.wrap_socket(conn,server_side=True) as secure:
                der=secure.getpeercert(binary_form=True)
                self.rows.append({'event':'authenticated' if der else 'server_only','tls':secure.version(),
                    'alpn':secure.selected_alpn_protocol(),'client_der_sha256':hashlib.sha256(der).hexdigest() if der else None,
                    'exact_der_match':der==self.expected})
                if self.proxy: self.tunnel(secure)
                elif secure.selected_alpn_protocol()=='h2': self.http2(secure)
                else: self.http1(secure)
        except (OSError,ValueError) as e:
            self.rows.append({'event':'rejected','reason':getattr(e,'reason',type(e).__name__)})
            conn.close()
    def http1(self,secure):
        pending=b''
        while not self.done.is_set():
            data=secure.recv(4096)
            if not data:return
            pending+=data
            if len(pending)>16384:raise ValueError('header bound')
            while b'\r\n\r\n' in pending:
                _,pending=pending.split(b'\r\n\r\n',1)
                secure.sendall(b'HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: keep-alive\r\n\r\nOK')
                self.rows.append({'event':'request','protocol':'http/1.1'})
    def http2(self,secure):
        connection=h2.connection.H2Connection(config=h2.config.H2Configuration(client_side=False))
        connection.initiate_connection();secure.sendall(connection.data_to_send())
        while not self.done.is_set():
            data=secure.recv(16384)
            if not data:return
            for event in connection.receive_data(data):
                if isinstance(event,h2.events.RequestReceived):
                    connection.send_headers(event.stream_id,[(b':status',b'200'),(b'content-length',b'2')])
                    connection.send_data(event.stream_id,b'OK',end_stream=True)
                    self.rows.append({'event':'request','protocol':'h2'})
                elif isinstance(event,h2.events.DataReceived):connection.acknowledge_received_data(event.flow_controlled_length,event.stream_id)
            secure.sendall(connection.data_to_send())
    def tunnel(self,secure):
        pending=b''
        while b'\r\n\r\n' not in pending:
            data=secure.recv(4096)
            if not data:return
            pending+=data
            if len(pending)>16384:raise ValueError('proxy header bound')
        header,extra=pending.split(b'\r\n\r\n',1)
        method,target,_=header.split(b'\r\n',1)[0].split(b' ')
        host,port=target.decode('ascii').rsplit(':',1)
        if method!=b'CONNECT' or host not in ('localhost','127.0.0.1'):raise ValueError('loopback boundary')
        with socket.create_connection(('127.0.0.1',int(port)),timeout=4) as remote:
            secure.sendall(b'HTTP/1.1 200 Connection established\r\n\r\n');self.rows.append({'event':'connect'})
            if extra:remote.sendall(extra)
            while not self.done.is_set():
                ready=[secure] if secure.pending() else select.select([secure,remote],[],[],4)[0]
                if not ready:return
                for source in ready:
                    data=source.recv(16384)
                    if not data:return
                    (remote if source is secure else secure).sendall(data)
    def close(self):
        self.done.set();self.socket.close();self.thread.join(timeout=2)
        for worker in self.workers:worker.join(timeout=5)
