"""Independent gates for timeout phases, producer pauses and Digest replay timing."""
import argparse
import json
from pathlib import Path
import socket
import socketserver
import ssl
import subprocess
import tempfile
import threading
import fixture

producer_seen=threading.Event()
digest_seen=threading.Event()
digest_release=threading.Event()
stall_release=threading.Event()
early_release=threading.Event()

class Stall(socketserver.BaseRequestHandler):
    def handle(self):
        self.request.settimeout(5)
        try: self.request.recv(4096);stall_release.wait(5)
        except OSError: pass

class Handler(fixture.Handler):
    def setup(self):
        super().setup()
        self.connection.setsockopt(socket.IPPROTO_TCP,socket.TCP_NODELAY,1)
    def route(self):
        self.connection.settimeout(5)
        if self.path=='/early-stop':
            assert early_release.wait(3)
            self.reply(b'',303,[('Location','/plain')]);self.close_connection=True;return
        if self.path=='/early-release':
            early_release.set();self.reply(b'ok');return
        if self.path=='/producer-pause':
            assert self.rfile.read(1024)==b'p'*1024;producer_seen.set()
            assert self.rfile.read(1024)==b'p'*1024
            self.reply(b'ok');return
        if self.path=='/producer-seen':
            assert producer_seen.wait(3);self.reply(b'ok');return
        if self.path=='/write-stall':
            self.connection.setsockopt(socket.SOL_SOCKET,socket.SO_RCVBUF,4096)
            stall_release.wait(3);self.close_connection=True;return
        if self.path=='/digest-seen':
            assert digest_seen.wait(3);self.reply(b'ok');return
        if self.path=='/digest-release':
            digest_release.set();self.reply(b'ok');return
        if self.path=='/digest-slow':
            valid=fixture.valid_digest(self.headers.get('Authorization',''),'user','password',self.command,
                                       self.path,'fixture','fixture-nonce')
            if not valid:
                self.read_body();self.reply(b'',401,[('WWW-Authenticate','Digest realm="fixture", nonce="fixture-nonce", algorithm=MD5, qop="auth"')]);return
            remaining=int(self.headers['Content-Length'])
            block=self.rfile.read(min(1024,remaining));remaining-=len(block);digest_seen.set()
            assert digest_release.wait(3)
            while remaining:
                block=self.rfile.read(min(65536,remaining))
                if not block:return
                remaining-=len(block)
            self.reply(b'ok');return
        super().route()
    do_GET = route
    do_POST = route

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary',type=Path);parser.add_argument('--evidence',required=True,type=Path)
    args=parser.parse_args();binary=args.binary.resolve();args.evidence.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='VOID timeout ') as directory:
        work=Path(directory);tool=fixture.certificate_tool(binary)
        config=work/'openssl.cnf';config.write_text('[req]\ndistinguished_name=dn\nx509_extensions=ext\n[dn]\nCN=localhost\n[ext]\nsubjectAltName=IP:127.0.0.1\nbasicConstraints=critical,CA:TRUE\n')
        cert,key=work/'cert.pem',work/'key.pem'
        subprocess.run([str(tool),'req','-x509','-newkey','rsa:2048','-nodes','-days','1','-config',str(config),
                        '-subj','/CN=localhost','-keyout',str(key),'-out',str(cert)],capture_output=True,check=True)
        plain=fixture.Server(('127.0.0.1',0),Handler);secure=fixture.Server(('127.0.0.1',0),Handler)
        context=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);context.load_cert_chain(cert,key)
        secure.socket=context.wrap_socket(secure.socket,server_side=True)
        stalled=socketserver.ThreadingTCPServer(('127.0.0.1',0),Stall);stalled.daemon_threads=True
        for peer in [plain,secure,stalled]:threading.Thread(target=peer.serve_forever,daemon=True).start()
        argv=[str(binary),f'http://127.0.0.1:{plain.server_port}',f'https://127.0.0.1:{stalled.server_address[1]}',
              f'http://127.0.0.1:{stalled.server_address[1]}',f'https://127.0.0.1:{secure.server_port}',str(cert),str(work/'input.bin')]
        try:
            try: result=subprocess.run(argv,capture_output=True,timeout=40)
            except subprocess.TimeoutExpired as failure:
                (args.evidence/'native.stdout').write_bytes(failure.stdout or b'')
                (args.evidence/'native.stderr').write_bytes(failure.stderr or b'')
                (args.evidence/'commands.json').write_text(json.dumps({'argv':argv,'timeout':True},indent=2)+'\n')
                raise
            (args.evidence/'native.stdout').write_bytes(result.stdout);(args.evidence/'native.stderr').write_bytes(result.stderr)
            (args.evidence/'commands.json').write_text(json.dumps({'argv':argv,'exit_code':result.returncode},indent=2)+'\n')
            assert result.returncode==0,result.stderr.decode()
            rows={row['case']:row for row in map(json.loads,result.stdout.splitlines())}
            assert rows['tls']['kind']=='timeout' and rows['tls']['phase']=='tls' and not rows['tls']['unknown'],rows
            assert rows['proxy']['kind']=='timeout' and rows['proxy']['phase']=='proxy' and not rows['proxy']['unknown'],rows
            assert rows['producer-resume']['kind']=='success',rows
            assert rows['digest-upload']['kind']=='success',rows
            assert rows['write']['kind']=='timeout' and rows['write']['phase']=='upload' and rows['write']['unknown'],rows
            assert rows['file-drain']['kind']=='timeout' and rows['file-drain']['phase']=='file' and rows['file-drain']['unknown'],rows
        finally:
            stall_release.set();digest_release.set()
            for peer in [plain,secure,stalled]:peer.shutdown();peer.server_close()
    print('TLS/proxy phases, resumed producers, blocked writes and Digest header deadlines passed')

if __name__=='__main__':main()
