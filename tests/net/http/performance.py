"""Opt-in Release measurements; no throughput threshold or external service."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import socket
import socketserver
import ssl
import statistics
import subprocess
import sys
import tempfile
import threading
import time
import fixture

ROOT=Path(__file__).resolve().parents[3]

class Server(fixture.Server):
    daemon_threads=True
    def get_request(self):
        connection,address=super().get_request();self.accepted+=1
        return connection,address

class Handler(fixture.Handler):
    def setup(self):
        super().setup();self.connection.setsockopt(socket.IPPROTO_TCP,socket.TCP_NODELAY,1)
    def do_POST(self):self.server.requests+=1;self.reply(self.read_body(),headers=[('Content-Type','application/json')])
    def do_GET(self):
        self.server.requests+=1
        if self.path=='/paused':
            self.send_response(200);self.send_header('Content-Length','536870912');self.end_headers();self.wfile.flush()
            self.server.release.wait(2);self.close_connection=True
        else:super().do_GET()

class H2(fixture.H2Handler):
    def handle(self):
        self.server.accepted+=1
        self.request.setsockopt(socket.IPPROTO_TCP,socket.TCP_NODELAY,1)
        return super().handle()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('compiler',type=Path);parser.add_argument('binary',type=Path)
    parser.add_argument('--evidence',required=True,type=Path);args=parser.parse_args()
    evidence=args.evidence.resolve();evidence.mkdir(parents=True,exist_ok=True)
    compiler,binary=args.compiler.resolve(),args.binary.resolve();commands=[]
    def run(argv,label,cwd):
        start=time.monotonic();cpu=os.times().children_user+os.times().children_system
        result=subprocess.run(list(map(str,argv)),cwd=cwd,capture_output=True,timeout=180)
        (evidence/(label+'.stdout')).write_bytes(result.stdout);(evidence/(label+'.stderr')).write_bytes(result.stderr)
        commands.append(dict(argv=list(map(str,argv)),label=label,exit_code=result.returncode,seconds=time.monotonic()-start,
            child_cpu_seconds=os.times().children_user+os.times().children_system-cpu))
        (evidence/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
        assert result.returncode==0,(label,result.stderr.decode(errors='replace'))
        return result.stdout
    with tempfile.TemporaryDirectory(prefix='VOID HTTP measurements ') as directory:
        work=Path(directory);tool=fixture.certificate_tool(compiler)
        config=work/'openssl.cnf';config.write_text('[req]\ndistinguished_name=dn\nx509_extensions=ext\n[dn]\nCN=localhost\n[ext]\nsubjectAltName=IP:127.0.0.1\nbasicConstraints=critical,CA:TRUE\n')
        cert,key=work/'cert.pem',work/'key.pem'
        run([tool,'req','-x509','-newkey','rsa:2048','-nodes','-days','1','-config',config,'-subj','/CN=localhost',
             '-keyout',key,'-out',cert],'certificate',work)
        plain=Server(('127.0.0.1',0),Handler);secure=Server(('127.0.0.1',0),Handler)
        context=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);context.load_cert_chain(cert,key)
        secure.socket=context.wrap_socket(secure.socket,server_side=True)
        sys.path.insert(0,str(ROOT/'.toolchains/http/test-python/site-packages'))
        import h2
        assert h2.__version__=='4.4.1'
        multiplex=socketserver.ThreadingTCPServer(('127.0.0.1',0),H2);multiplex.daemon_threads=True
        multiplex.context=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);multiplex.context.load_cert_chain(cert,key)
        multiplex.context.set_alpn_protocols(['h2'])
        for server in [plain,secure,multiplex]:
            server.accepted=0;server.requests=0;server.release=threading.Event()
            threading.Thread(target=server.serve_forever,daemon=True).start()
        try:
            rows=[json.loads(line) for line in run([binary,f'http://127.0.0.1:{plain.server_port}',
                f'https://127.0.0.1:{secure.server_port}',f'https://127.0.0.1:{multiplex.server_address[1]}',cert],'network',work).splitlines()]
            # A cold connection, 136 small requests sharing it, then 16 partial
            # responses explicitly closed. HTTP/1 closes need a fresh socket;
            # HTTP/2 closes reset only their stream and retain the connection.
            assert plain.requests==secure.requests==153,(plain.requests,secure.requests)
            assert plain.accepted==secure.accepted==16 and multiplex.accepted==1,(plain.accepted,secure.accepted,multiplex.accepted)
            sizes={};summaries={}
            for name,text in [('plain','print("7")\nreturn 0\n'),('imported','use net/http\nprint("7")\nreturn 0\n')]:
                source=work/(name+'.void');source.write_text(text);(evidence/source.name).write_text(text)
                for mode,flags in [('normal',['--no-opt']),('optimized',[])]:
                    for index in range(3):
                        run([compiler,'check',source],f'{name}-{mode}-check-{index}',work)
                        assert run([compiler,'run',source,'--no-cache',*flags],f'{name}-{mode}-jit-{index}',work).strip()==b'7'
                        output=work/(name+'-'+mode+('.exe' if os.name=='nt' else ''))
                        run([compiler,'build',source,'-o',output,*flags],f'{name}-{mode}-aot-{index}',work)
                        assert run([output],f'{name}-{mode}-run-{index}',work).strip()==b'7'
                        sizes[name+'-'+mode]=output.stat().st_size
                    for operation in ['check','jit','aot','run']:
                        values=[row['seconds'] for row in commands if row['label'].startswith(f'{name}-{mode}-{operation}-')]
                        summaries[f'{name}-{mode}-{operation}']=dict(median_seconds=statistics.median(values),minimum=min(values),maximum=max(values))
            report=dict(host=platform.platform(),machine=platform.machine(),stack_bytes=1048576,
                compiler_sha256=hashlib.sha256(compiler.read_bytes()).hexdigest(),network=rows,
                connections=dict(h1=plain.accepted,h1_tls=secure.accepted,h2_tls=multiplex.accepted),
                compilation=summaries,executable_bytes=sizes,
                scope='Local Release samples under concurrent validation load; no baseline compiler or native Windows/macOS comparison')
            (evidence/'measurements.json').write_text(json.dumps(report,indent=2)+'\n')
        finally:
            for server in [plain,secure,multiplex]:server.release.set();server.shutdown();server.server_close()
    print('HTTP Release latency, pool reuse, partial-response close and unused-import measurements recorded')

if __name__=='__main__':main()
