"""Disposable Windows in-memory P12 TLS study. No OS trust/identity writes."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import csv
import functools
import hashlib
import http.server
import importlib.util
import json
from pathlib import Path
import secrets
import shutil
import ssl
import subprocess
import sys
import tempfile
import threading
import urllib.request
import zipfile

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
OUT=ROOT/'.toolchains/native-awslc'
OPENSSL=Path('C:/Program Files/Git/usr/bin/openssl.exe')

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def dependencies():
    for pin in json.loads((ROOT/'tests/net/http/python-versions.json').read_text()):
        path=OUT/'downloads'/(pin['name']+'-'+pin['version']+'.whl')
        if not path.exists():
            # The VM's Python trust setup rejected this download host. Use the
            # OS curl with ordinary Schannel verification, never insecure TLS.
            subprocess.run(['C:/Windows/System32/curl.exe','-q','--noproxy','*','--fail','--location','--silent','--show-error',
                '--max-time','90','--output',str(path),pin['url']],check=True)
        assert sha(path)==pin['sha256']
        with zipfile.ZipFile(path) as bundle:
            for item in bundle.infolist():assert (OUT/'python'/item.filename).resolve().is_relative_to((OUT/'python').resolve())
            bundle.extractall(OUT/'python')

def inventory():
    path=HERE.parent/'native-schannel/collision-study.py'
    spec=importlib.util.spec_from_file_location('prior_collision_study',path)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module.inventory('build')

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--profile',choices=['Debug','Release'],required=True)
    args=parser.parse_args();dependencies()
    from peer import Peer
    executable=OUT/args.profile/'curl/probe.exe'
    before=inventory();cases=[];fixture_rows=[];row_lock=threading.Lock()
    progress=OUT/(args.profile+'-progress.json')
    if progress.exists():shutil.copyfile(progress,OUT/('earlier-'+progress.stem+'-'+sha(progress)[:12]+'.json'))
    final=OUT/(args.profile+'-result.json')
    if final.exists():shutil.copyfile(final,OUT/('earlier-'+final.stem+'-'+sha(final)[:12]+'.json'))
    for algorithm in ('RSA','ECDSA'):
        directory=Path(tempfile.mkdtemp(prefix='fixture-',dir=OUT))
        sid=list(csv.reader(subprocess.check_output(['whoami','/user','/fo','csv','/nh'],text=True).splitlines()))[0][1]
        subprocess.run(['icacls',str(directory),'/inheritance:r','/grant:r',f'*{sid}:(OI)(CI)F','*S-1-5-18:(OI)(CI)F'],check=True,stdout=subprocess.DEVNULL)
        password=secrets.token_hex(24);http_requests=[]
        class Handler(http.server.SimpleHTTPRequestHandler):
            def log_message(self,*args):pass
            def do_GET(self):http_requests.append(self.path);super().do_GET()
        crl_server=http.server.ThreadingHTTPServer(('127.0.0.1',0),functools.partial(Handler,directory=str(directory)))
        threading.Thread(target=crl_server.serve_forever,daemon=True).start()
        try:
            def openssl(*argv,stdin=None):
                p=subprocess.run([str(OPENSSL),*argv],cwd=directory,input=stdin.encode('ascii') if stdin is not None else None,capture_output=True)
                if p.returncode:raise RuntimeError('disposable fixture command failed: '+argv[0])
            openssl('req','-x509','-newkey','rsa:2048','-noenc','-keyout','ca.key','-out','ca.pem','-days','2','-subj','/CN=VOID AWS-LC disposable root',
                '-addext','basicConstraints=critical,CA:TRUE','-addext','keyUsage=critical,keyCertSign,cRLSign')
            key=['-newkey','rsa:2048'] if algorithm=='RSA' else ['-newkey','ec','-pkeyopt','ec_paramgen_curve:P-256']
            openssl('req','-new',*key,'-noenc','-keyout','client.key','-out','client.csr','-subj','/CN=VOID disposable client')
            (directory/'client.ext').write_text('basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature\nextendedKeyUsage=clientAuth\n')
            openssl('x509','-req','-in','client.csr','-CA','ca.pem','-CAkey','ca.key','-CAcreateserial','-days','2','-extfile','client.ext','-out','client.pem')
            openssl('req','-new','-newkey','rsa:2048','-noenc','-keyout','server.key','-out','server.csr','-subj','/CN=localhost')
            (directory/'server.ext').write_text('basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\nsubjectAltName=DNS:localhost\n'
                +f'crlDistributionPoints=URI:http://127.0.0.1:{crl_server.server_port}/revocation.crl\n')
            openssl('x509','-req','-in','server.csr','-CA','ca.pem','-CAkey','ca.key','-CAcreateserial','-days','2','-extfile','server.ext','-out','server.pem')
            openssl('req','-x509','-newkey','rsa:2048','-noenc','-keyout','wrong-ca.key','-out','wrong-ca.pem','-days','2','-subj','/CN=VOID wrong root')
            # Default OpenSSL 3 AES/PBES2 P12, unlike the older Schannel fixture.
            openssl('pkcs12','-export','-inkey','client.key','-in','client.pem','-certfile','ca.pem','-out','client.p12','-passout','stdin',stdin=password+'\n')
            openssl('pkcs12','-in','client.p12','-noout','-passin','stdin',stdin=password+'\n')
            (directory/'index.txt').write_text('');(directory/'crlnumber').write_text('01\n')
            (directory/'ca.cnf').write_text('[ca]\ndefault_ca=local\n[local]\ndatabase=index.txt\nprivate_key=ca.key\ncertificate=ca.pem\ndefault_md=sha256\ndefault_crl_days=1\ncrlnumber=crlnumber\n')
            def crl():
                openssl('ca','-gencrl','-config','ca.cnf','-out','revocation.pem')
                openssl('crl','-in','revocation.pem','-outform','DER','-out','revocation.crl')
            crl()
            def run(label,peer,tls='1.3',protocol='h1',ca='ca.pem',identity=True,wrong_password=False,native=False,revocation='-',proxy=None,proxy_identity=False,host='localhost',proxy_host='localhost',expect=True):
                start=len(peer.rows);proxy_start=len(proxy.rows) if proxy else 0
                command=[str(executable),f'https://{host}:{peer.port}/',str(directory/ca) if ca!='-' else '-',
                    str(directory/'client.p12') if identity else '-',tls,protocol,'native' if native else 'explicit',
                    str(directory/revocation) if revocation not in ('-','ocsp') else revocation,
                    f'https://{proxy_host}:{proxy.port}' if proxy else '-','proxy-identity' if proxy_identity else '-']
                p=subprocess.run(command,input=password+('wrong' if wrong_password else '')+'\n',capture_output=True,text=True,timeout=30)
                row={'case':label,'algorithm':algorithm,'tls':tls,'protocol':protocol,'expected_success':expect,'exit':p.returncode,
                    'client':[json.loads(line) for line in p.stdout.splitlines()], 'peer':peer.rows[start:],
                    'proxy':proxy.rows[proxy_start:] if proxy else [],'error':p.stderr.strip().replace(str(directory),'<fixture>')}
                with row_lock:
                    cases.append(row)
                    progress.write_text(json.dumps({'profile':args.profile,'cases':cases,'inventory_before':before,
                        'probe_sha256':sha(executable),'probe_source_sha256':sha(HERE/'probe.cpp')},indent=2)+'\n')
                if expect:
                    assert p.returncode==0,(label,p.stderr)
                    assert any(r.get('exact_der_match') for r in row['peer']),label
                    assert [r['new_connections'] for r in row['client'] if 'new_connections' in r]==[1,0],label
                    assert any(r.get('tls')=='TLSv'+tls and r.get('alpn')==('h2' if protocol=='h2' else 'http/1.1') for r in row['peer']),label
                else:assert p.returncode!=0,label
                print(args.profile,algorithm,label,tls,protocol,'recorded',flush=True)
            for tls,version in [('1.2',ssl.TLSVersion.TLSv1_2),('1.3',ssl.TLSVersion.TLSv1_3)]:
                for protocol in ('h1','h2'):
                    peer=Peer(directory,version,protocol)
                    try:
                        run('in_memory_p12',peer,tls,protocol)
                        if protocol=='h1':
                            run('anonymous',peer,tls,protocol,identity=False,expect=False)
                            run('wrong_password',peer,tls,protocol,wrong_password=True,expect=False)
                            run('wrong_root',peer,tls,protocol,ca='wrong-ca.pem',expect=False)
                            run('wrong_hostname',peer,tls,protocol,host='127.0.0.1',expect=False)
                    finally:peer.close()
            if algorithm=='RSA':
                peer=Peer(directory,ssl.TLSVersion.TLSv1_3)
                try:
                    run('explicit_crl_good',peer,revocation='revocation.pem')
                    openssl('ca','-config','ca.cnf','-revoke','server.pem');crl()
                    run('revoked_default_no_online_check',peer)
                    run('explicit_crl_revoked',peer,revocation='revocation.pem',expect=False)
                    run('ocsp_staple_required_missing',peer,revocation='ocsp',expect=False)
                    run('native_plus_explicit',peer,native=True)
                    run('native_only_untrusted_fixture',peer,native=True,ca='-',expect=False)
                    run('native_plus_wrong_explicit_root',peer,native=True,ca='wrong-ca.pem',expect=False)
                    proxy=Peer(directory,ssl.TLSVersion.TLSv1_3,proxy=True,require_client=False)
                    try:run('https_proxy_verified',peer,proxy=proxy)
                    finally:proxy.close()
                    proxy=Peer(directory,ssl.TLSVersion.TLSv1_3,proxy=True)
                    try:
                        run('https_proxy_identity_absent',peer,proxy=proxy,expect=False)
                        run('https_proxy_p12_identity',peer,proxy=proxy,proxy_identity=True)
                        run('https_proxy_wrong_root',peer,proxy=proxy,proxy_identity=True,ca='wrong-ca.pem',expect=False)
                        run('https_proxy_wrong_hostname',peer,proxy=proxy,proxy_identity=True,proxy_host='127.0.0.1',expect=False)
                    finally:proxy.close()
                    with ThreadPoolExecutor(max_workers=2) as pool:
                        results=[pool.submit(run,'independent_process_owner',peer) for _ in range(2)]
                        for result in results:result.result()
                finally:peer.close()
            fixture_rows.append({'algorithm':algorithm,'p12':'default AES/PBES2','cdp_http_requests':len(http_requests)})
        finally:
            crl_server.shutdown();crl_server.server_close()
            assert directory.parent.resolve()==OUT.resolve();shutil.rmtree(directory)
    after=inventory();assert before==after,'OS credential inventory changed'
    report={'schema':1,'profile':args.profile,'cases':cases,'fixtures':fixture_rows,'inventory_before':before,'inventory_after':after,
        'probe_sha256':sha(executable),'source_sha256':{name:sha(HERE/name) for name in ('CMakeLists.txt','build.py','probe.cpp','peer.py','run.py')},
        'fixture_directories_removed':True}
    (OUT/(args.profile+'-result.json')).write_text(json.dumps(report,indent=2)+'\n')
    print(len(cases),'cases; unchanged OS inventories; fixture directories removed')

if __name__=='__main__':main()
