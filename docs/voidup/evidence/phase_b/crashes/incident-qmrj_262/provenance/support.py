"""Local HTTPS fixtures and lossless subprocess receipts for VOIDUP qualification."""
from pathlib import Path
import datetime, hashlib, http.server, ipaddress, json, os, ssl, sys, threading, time
from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.x509.oid import NameOID

ROOT = Path(__file__).resolve().parents[2]
EVIDENCE = ROOT / 'docs/voidup/evidence/phase_b'
sys.path.insert(0, str(ROOT / 'tests/text_encoding'))
from crash import run as preserving_run, preserve
sys.path.insert(0, str(ROOT / 'tools'))
from package_voidup_fixtures import encoded, sign, sha
os.environ['VOID_CRASH_EVIDENCE'] = str(EVIDENCE / 'crashes')

def stack():
    import resource
    resource.setrlimit(resource.RLIMIT_STACK, (1048576, 1048576))

class Runner:
    def __init__(self, name):
        self.name = name
        self.sequence = 0
        self.receipts = EVIDENCE / (name + '.jsonl')
        if self.receipts.exists():
            raise RuntimeError('retain previous evidence; choose a new run name')
    def run(self, args, *, cwd=ROOT, expected=0, input=None, env=None, timeout=600):
        command = list(map(str, args))
        self.sequence += 1
        stamp = time.monotonic()
        result = preserving_run(command, cwd=cwd, input=input, env=env,
            capture_output=True, timeout=timeout, preexec_fn=stack,
            source_inputs=(ROOT/'apps/voidup',), provenance_inputs=(Path(__file__).parent,))
        base = f'{self.name}-{self.sequence:03}'
        (EVIDENCE/(base+'.stdout')).write_bytes(result.stdout)
        (EVIDENCE/(base+'.stderr')).write_bytes(result.stderr)
        receipt = dict(command=command, cwd=str(cwd), status=result.returncode,
            expected=expected, wall_seconds=time.monotonic()-stamp, hard_stack_bytes=1048576,
            stdout=base+'.stdout', stderr=base+'.stderr',
            input_sha256=hashlib.sha256(input).hexdigest() if input is not None else None)
        with self.receipts.open('a') as output:
            output.write(json.dumps(receipt)+'\n')
        if result.returncode != expected:
            preserve(command,cwd,result,(ROOT/'apps/voidup',),(Path(__file__).parent,),failure_kind='test_assertion')
            raise AssertionError((receipt,result.stdout[-3000:],result.stderr[-6000:]))
        return result

class Service:
    def __init__(self, fixtures):
        self.fixtures = Path(fixtures)
        self.overrides = {}
        self.stall = None
        self.started = threading.Event()
        self.resume = threading.Event()
        self.requests = []
        self._tls()
        service = self
        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self,*args):
                pass
            def do_GET(self):
                service.requests.append(self.path)
                if service.stall == self.path:
                    service.started.set()
                    service.resume.wait(60)
                value = service.overrides.get(self.path)
                if value is not None:
                    status, body, headers = value
                    self.send_response(status)
                    for name, content in headers.items(): self.send_header(name,content)
                    self.send_header('Content-Length',str(len(body)))
                    self.end_headers()
                    try: self.wfile.write(body)
                    except (BrokenPipeError,ConnectionResetError,ssl.SSLError): pass
                    return
                relative = self.path.lstrip('/')
                target = (service.fixtures/'www'/relative).resolve()
                if not target.is_relative_to((service.fixtures/'www').resolve()) or not target.is_file():
                    self.send_error(404); return
                self.send_response(200)
                self.send_header('Content-Length',str(target.stat().st_size))
                self.end_headers()
                try:
                    with target.open('rb') as stream:
                        for block in iter(lambda:stream.read(262144),b''): self.wfile.write(block)
                except (BrokenPipeError,ConnectionResetError,ssl.SSLError): pass
        self.server = http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler)
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(str(self.fixtures/'tls/server.pem'),str(self.fixtures/'tls/server-key.pem'))
        self.server.socket = context.wrap_socket(self.server.socket,server_side=True)
        self.url = f'https://127.0.0.1:{self.server.server_port}/catalog.json'
        self.thread = threading.Thread(target=self.server.serve_forever,daemon=True)
    def _tls(self):
        directory = self.fixtures/'tls';directory.mkdir(mode=0o700,exist_ok=True)
        self.ca = directory/'ca.pem'
        if self.ca.exists(): return
        key = rsa.generate_private_key(public_exponent=65537,key_size=2048)
        subject = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME,'VOIDUP local engineering CA')])
        now = datetime.datetime.now(datetime.timezone.utc)
        ca = x509.CertificateBuilder().subject_name(subject).issuer_name(subject).public_key(key.public_key()).serial_number(x509.random_serial_number()).not_valid_before(now-datetime.timedelta(days=1)).not_valid_after(now+datetime.timedelta(days=30)).add_extension(x509.BasicConstraints(ca=True,path_length=0),critical=True).sign(key,hashes.SHA256())
        server_key = rsa.generate_private_key(public_exponent=65537,key_size=2048)
        cert = x509.CertificateBuilder().subject_name(x509.Name([x509.NameAttribute(NameOID.COMMON_NAME,'localhost')])).issuer_name(subject).public_key(server_key.public_key()).serial_number(x509.random_serial_number()).not_valid_before(now-datetime.timedelta(days=1)).not_valid_after(now+datetime.timedelta(days=7)).add_extension(x509.SubjectAlternativeName([x509.DNSName('localhost'),x509.IPAddress(ipaddress.ip_address('127.0.0.1'))]),critical=False).add_extension(x509.BasicConstraints(ca=False,path_length=None),critical=True).sign(key,hashes.SHA256())
        self.ca.write_bytes(ca.public_bytes(serialization.Encoding.PEM))
        (directory/'server.pem').write_bytes(cert.public_bytes(serialization.Encoding.PEM))
        private=directory/'server-key.pem'
        private.write_bytes(server_key.private_bytes(serialization.Encoding.PEM,serialization.PrivateFormat.PKCS8,serialization.NoEncryption()));private.chmod(0o600)
    def catalog(self, document, signers=(1,2)):
        raw = encoded(document)
        self.overrides['/catalog.json']=(200,raw,{})
        self.overrides['/catalog.json.sig']=(200,sign(raw,self.fixtures/'keys',signers),{})
    def __enter__(self):
        self.thread.start();return self
    def __exit__(self,*args):
        self.resume.set();self.server.shutdown();self.server.server_close();self.thread.join()
