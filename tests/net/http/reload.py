"""Hold a real public-source HTTP read across compatible code replacement."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import fixture

ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'tests/text_encoding'))
import crash

class Handler(fixture.Handler):
    def do_GET(self):
        if self.path=='/held':
            self.server.count+=1;self.server.seen.set()
            self.send_response(200);self.send_header('Content-Length','2');self.end_headers();self.wfile.flush()
            if self.server.release.wait(60):self.wfile.write(b'ok')
        elif self.path=='/seen':
            if self.server.seen.wait(60):self.reply(b'ok')
        elif self.path=='/release':self.server.release.set();self.reply(b'ok')

SOURCE='''use net/http

async func entry(value: i32) -> i32:
    if http.open_client() is Ok(var owner):
        with owner:
            if owner.start(http.request(target = "@BASE@/held")) is Ok(var exchange):
                with exchange:
                    var first = exchange.read_bytes_async()
                    if (await first) is Ok(var block):
                        if (await first) is Ok(var again):
                            if block.bytes.count > 0 and block.bytes.count == again.bytes.count:
                                return value + 1
    return -1
'''

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary',type=Path);parser.add_argument('--evidence',type=Path,required=True)
    args=parser.parse_args();binary=args.binary.resolve();args.evidence.mkdir(parents=True,exist_ok=True)
    server=fixture.Server(('127.0.0.1',0),Handler);server.daemon_threads=True
    server.seen=threading.Event();server.release=threading.Event();server.count=0
    threading.Thread(target=server.serve_forever,daemon=True).start()
    with tempfile.TemporaryDirectory(prefix='VOID HTTP reload ') as directory:
        work=Path(directory);base=f'http://127.0.0.1:{server.server_port}'
        path=work/'main.void';source=SOURCE.replace('@BASE@',base);path.write_text(source)
        (args.evidence/'original.void').write_text(source)
        try:
            environment=os.environ.copy();environment['VOID_CRASH_EVIDENCE']=str(ROOT/'docs/net/http/evidence/crashes')
            result=crash.run([str(binary),str(path),base,str(work/'cache')],cwd=work,capture_output=True,timeout=180,
                env=environment,expected_returncode=0,source_inputs=[path,ROOT/'stdlib'],
                provenance_inputs=[Path(__file__),Path(__file__).with_suffix('.cpp'),ROOT/'CMakeLists.txt',ROOT/'src/runtime',ROOT/'include'])
            (args.evidence/'native.stdout').write_bytes(result.stdout);(args.evidence/'native.stderr').write_bytes(result.stderr)
            (args.evidence/'final.void').write_bytes(path.read_bytes())
            (args.evidence/'commands.json').write_text(json.dumps(dict(binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                exit_code=result.returncode,held_requests=server.count),indent=2)+'\n')
            assert result.returncode==0,result.stderr.decode(errors='replace')
            assert server.count==2,server.count
        finally:server.release.set();server.shutdown();server.server_close()
    print('Public HTTP held-request reload passed')

if __name__=='__main__':main()
