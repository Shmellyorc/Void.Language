"""Bounded native adapter investigation; generated credentials never become evidence."""
import argparse
import csv
import datetime
import functools
import hashlib
import http.server
import importlib.util
import json
from pathlib import Path
import secrets
import shutil
import socket
import ssl
import subprocess
import sys
import tempfile
import threading
import time

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
OUT=ROOT/'.toolchains/native-awslc-adapters'
sys.path.insert(0,str(HERE.parent/'native-awslc'))
from peer import Peer
OPENSSL=Path('C:/Program Files/Git/usr/bin/openssl.exe')

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def inventory():
    spec=importlib.util.spec_from_file_location('inventory_source',HERE.parent/'native-schannel/collision-study.py')
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module.inventory('build')

class ExactPeer(Peer):
    def serve(self,conn):
        conn.settimeout(4)
        try:
            with self.context.wrap_socket(conn,server_side=True) as secure:
                der=secure.getpeercert(binary_form=True)
                self.rows.append({'event':'authenticated' if der else 'server_only','tls':secure.version(),
                    'exact_der_match':der==self.expected,'client_der_sha256':hashlib.sha256(der).hexdigest() if der else None})
                if der and der!=self.expected:self.rows.append({'event':'wrong_identity_rejected'});return
                if self.proxy:self.tunnel(secure)
                else:self.http1(secure)
        except (OSError,ValueError) as error:
            self.rows.append({'event':'rejected','reason':getattr(error,'reason',type(error).__name__)});conn.close()

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--profile',choices=['Debug','Release'],required=True)
    parser.add_argument('--track',choices=['trust','sign','all','scope','proxy-origin'],default='all');args=parser.parse_args()
    OUT.mkdir(exist_ok=True);directory=Path(tempfile.mkdtemp(prefix='fixture-',dir=OUT))
    sid=list(csv.reader(subprocess.check_output(['whoami','/user','/fo','csv','/nh'],text=True).splitlines()))[0][1]
    subprocess.run(['icacls',str(directory),'/inheritance:r','/grant:r',f'*{sid}:(OI)(CI)F','*S-1-5-18:(OI)(CI)F'],check=True,stdout=subprocess.DEVNULL)
    before=inventory();rows=[];fixture_records=[];objects=[];requests=[];fault=None
    report_path=OUT/f'{args.profile}-{args.track}.json'
    if report_path.exists():shutil.copyfile(report_path,OUT/('earlier-'+report_path.stem+'-'+sha(report_path)[:12]+'.json'))
    class Handler(http.server.SimpleHTTPRequestHandler):
        def log_message(self,*args):pass
        def do_GET(self):
            requests.append(self.path)
            if self.path.startswith('/slow'):time.sleep(2)
            try:super().do_GET()
            except (BrokenPipeError,ConnectionResetError):pass
    web=http.server.ThreadingHTTPServer(('127.0.0.1',0),functools.partial(Handler,directory=str(directory)))
    threading.Thread(target=web.serve_forever,daemon=True).start()
    executable=OUT/args.profile/'adapter-probe.exe';fixture=OUT/args.profile/'fixture.exe'
    def save(complete=False,after=None):
        data={'schema':1,'profile':args.profile,'track':args.track,'complete':complete,'cases':rows,'fixture_records':fixture_records,
              'http_requests':requests,'inventory_before':before,'inventory_after':after,'error':fault,
              'probe_sha256':sha(executable),'fixture_sha256':sha(fixture),
              'source_sha256':{p.name:sha(p) for p in HERE.iterdir() if p.suffix in ('.hpp','.cpp','.py','.txt')}}
        report_path.write_text(json.dumps(data,indent=2)+'\n')
    def openssl(*command):
        p=subprocess.run([str(OPENSSL),*command],cwd=directory,capture_output=True)
        if p.returncode:raise RuntimeError('fixture OpenSSL failed: '+command[0]+': '+p.stderr.decode(errors='replace').replace(str(directory),'<fixture>'))
    def ca(name,issuer=None):
        openssl('req','-new','-newkey','rsa:2048','-noenc','-keyout',name+'.key','-out',name+'.csr','-subj','/CN=VOID '+name+' '+secrets.token_hex(6))
        if issuer:
            (directory/(name+'.ext')).write_text('basicConstraints=critical,CA:TRUE,pathlen:0\nkeyUsage=critical,keyCertSign,cRLSign\n'+f'crlDistributionPoints=URI:http://127.0.0.1:{web.server_port}/root.crl\n')
            openssl('x509','-req','-in',name+'.csr','-CA',issuer+'.pem','-CAkey',issuer+'.key','-CAcreateserial','-days','2','-extfile',name+'.ext','-out',name+'.pem')
        else:openssl('req','-x509','-key',name+'.key','-in',name+'.csr','-out',name+'.pem','-days','2','-addext','basicConstraints=critical,CA:TRUE','-addext','keyUsage=critical,keyCertSign,cRLSign')
        (directory/(name+'.index')).write_text('');(directory/(name+'.crlnumber')).write_text('01\n')
        (directory/(name+'.cnf')).write_text('[ca]\ndefault_ca=local\n[local]\ndatabase='+name+'.index\nprivate_key='+name+'.key\ncertificate='+name+'.pem\ndefault_md=sha256\ndefault_crl_days=1\ncrlnumber='+name+'.crlnumber\n')
    def crl(name):
        openssl('ca','-gencrl','-config',name+'.cnf','-out',name+'.crl.pem')
        openssl('crl','-in',name+'.crl.pem','-outform','DER','-out',name+'.crl')
    def issue(name,issuer='root',cdp='root.crl',aia=None,days='2',san='DNS:localhost,DNS:origin.test,DNS:proxy.test,IP:127.0.0.1'):
        openssl('req','-new','-newkey','rsa:2048','-noenc','-keyout',name+'.key','-out',name+'.csr','-subj','/CN=localhost')
        extension='basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\nsubjectAltName='+san+'\n'
        if cdp:extension+=f'crlDistributionPoints=URI:http://127.0.0.1:{web.server_port}/{cdp}\n'
        if aia:extension+=f'authorityInfoAccess=caIssuers;URI:http://127.0.0.1:{web.server_port}/{aia}\n'
        (directory/(name+'.ext')).write_text(extension)
        validity=['-days',days]
        if days in ('-1','future'):
            now=datetime.datetime.now(datetime.UTC);start,end=(-2,-1) if days=='-1' else (1,2)
            validity=['-not_before',(now+datetime.timedelta(days=start)).strftime('%Y%m%d%H%M%SZ'),'-not_after',(now+datetime.timedelta(days=end)).strftime('%Y%m%d%H%M%SZ')]
        openssl('x509','-req','-in',name+'.csr','-CA',issuer+'.pem','-CAkey',issuer+'.key','-CAcreateserial',*validity,'-extfile',name+'.ext','-out',name+'.pem')
    def peer(name='good',tls='1.3',client=False,chain=None,proxy=False):
        content=(directory/(name+'.pem')).read_bytes()
        if chain:content+=(directory/(chain+'.pem')).read_bytes()
        (directory/'server.pem').write_bytes(content);shutil.copyfile(directory/(name+'.key'),directory/'server.key')
        p=ExactPeer(directory,ssl.TLSVersion.TLSv1_3 if tls=='1.3' else ssl.TLSVersion.TLSv1_2,proxy=proxy,require_client=client)
        return p
    def run(label,p,expected,trust='windows',tls='1.3',identity='-',kind='none',host='localhost',proxy=None,proxy_host='proxy.test',mode='normal',owners=False,ca_file='root.pem'):
        start=len(p.rows);proxy_start=len(proxy.rows) if proxy else 0;routes=f'{host}:{p.port}:127.0.0.1'
        if proxy:routes+=f'|{proxy_host}:{proxy.port}:127.0.0.1'
        command=[str(executable),f'https://{host}:{p.port}/',str(directory/ca_file) if ca_file!='-' else '-',identity,kind,trust,tls,
                 f'https://{proxy_host}:{proxy.port}' if proxy else '-',mode,routes,'owners' if owners else 'normal']
        started=time.monotonic();process=subprocess.run(command,input='\n',capture_output=True,text=True,timeout=35)
        row={'case':label,'expected_success':expected,'exit':process.returncode,'seconds':round(time.monotonic()-started,3),
             'client':[json.loads(line) for line in process.stdout.splitlines()], 'peer':p.rows[start:],
             'proxy':proxy.rows[proxy_start:] if proxy else [],'error':process.stderr.strip().replace(str(directory),'<fixture>')}
        rows.append(row);save()
        assert (process.returncode==0)==expected,(label,row)
        if expected:
            requests_rows=[r for r in row['client'] if 'curl_status' in r]
            assert all(r['curl_status']==0 for r in requests_rows)
            assert sorted(r['new_connections'] for r in requests_rows)==([0,1,1] if owners else [0,1])
            if kind=='store':assert any(r.get('exact_der_match') for r in row['peer'])
        print(args.profile,label,'recorded',flush=True);return row
    def fixture_call(action,item,certificate='-',extra=None):
        command=[str(fixture),action,item['nonce'],str(item['public']),str(directory/certificate) if certificate!='-' else '-']
        if action=='create':command=[str(fixture),action,item['nonce'],extra,str(item['public']),str(item['csr'])]
        result=subprocess.run(command,capture_output=True,text=True,timeout=20)
        record={'action':action,'fixture':item['label'],'exit':result.returncode,'records':[json.loads(line) for line in result.stdout.splitlines()]}
        fixture_records.append(record);save();assert result.returncode==0,record;return record
    def provision(label,algorithm,bind_certificate=None):
        item={'label':label,'nonce':secrets.token_hex(32).upper(),'public':directory/(label+'.public'),'csr':directory/(label+'.csr.der'),'certificate':'-'};objects.append(item)
        fixture_call('create',item,extra=algorithm)
        openssl('req','-inform','DER','-in',label+'.csr.der','-verify','-noout')
        openssl('req','-inform','DER','-in',label+'.csr.der','-out',label+'.csr.pem')
        (directory/'client.ext').write_text('basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature\nextendedKeyUsage=clientAuth\n')
        openssl('x509','-req','-in',label+'.csr.pem','-CA','root.pem','-CAkey','root.key','-CAcreateserial','-days','2','-extfile','client.ext','-out',label+'.pem')
        openssl('x509','-in',label+'.pem','-outform','DER','-out',label+'.der')
        item['certificate']=bind_certificate or label+'.der';fixture_call('install-bad-association' if bind_certificate else 'install',item,item['certificate'])
        thumb=hashlib.sha1((directory/item['certificate']).read_bytes()).hexdigest().upper()
        item['reference']='CurrentUser\\VOID-ADAPTER-TEST-STORE-'+item['nonce']+'\\'+thumb
        item['before']=fixture_call('inspect',item,item['certificate']);return item
    try:
        ca('root');shutil.copyfile(directory/'root.pem',directory/'ca.pem');ca('wrong')
        # The old peer expects an identity file even for server-only trust tests.
        shutil.copyfile(directory/'root.pem',directory/'client.pem')
        issue('good');issue('revoked');issue('unknown',cdp=None);issue('offline',cdp='not-present.crl');issue('cache',cdp='cache-only.crl');issue('slow',cdp='slow.crl');issue('expired',days='-1');issue('future',days='future')
        openssl('ca','-config','root.cnf','-revoke','revoked.pem');crl('root');shutil.copyfile(directory/'root.crl',directory/'cache-only.crl');shutil.copyfile(directory/'root.crl',directory/'slow.crl')
        if args.track in ('trust','all'):
            for label,name,expected,options in [
                ('trusted_explicit','good',True,{}),('untrusted_explicit','good',False,{'ca_file':'wrong.pem'}),
                ('default_system_untrusted_generated_root','good',False,{'ca_file':'-'}),('wrong_hostname','good',False,{'host':'wrong.test'}),
                ('revoked','revoked',False,{}),('revocation_unavailable','unknown',False,{}),('revocation_endpoint_offline','offline',False,{}),
                ('revocation_cache_only_miss','cache',False,{'trust':'offline'}),('expired','expired',False,{}),('not_yet_valid','future',False,{}),
                ('ip_literal_aws_control','good',True,{'host':'127.0.0.1','trust':'aws'}),('ip_literal_native_context_missing','good',False,{'host':'127.0.0.1'}),
                ('verification_async_retry_unsupported','good',False,{'trust':'verify-retry'}),('deadline_during_native_verification','slow',False,{'mode':'deadline'})]:
                p=peer(name)
                try:run(label,p,expected,**options)
                finally:p.close()
            for name in ('intermediate_full','intermediate_aia','intermediate_missing'):
                ca(name,'root');crl(name);issue(name+'_leaf',issuer=name,cdp=name+'.crl',aia=name+'.der' if name=='intermediate_aia' else None)
                openssl('x509','-in',name+'.pem','-outform','DER','-out',name+'.der')
                p=peer(name+'_leaf',chain=name if name=='intermediate_full' else None)
                try:run(name,p,name!='intermediate_missing')
                finally:p.close()
            origin=peer('good');proxy=peer('good',proxy=True)
            try:
                run('proxy_and_origin_verified',origin,True,proxy=proxy)
                run('proxy_wrong_hostname',origin,False,proxy=proxy,proxy_host='wrong.test')
                run('origin_wrong_hostname_after_proxy',origin,False,proxy=proxy,host='wrong.test')
            finally:origin.close();proxy.close()
            origin=peer('good');proxy=peer('revoked',proxy=True)
            try:run('revoked_proxy',origin,False,proxy=proxy)
            finally:origin.close();proxy.close()
        if args.track in ('sign','all'):
            rsa=provision('rsa','RSA');ecdsa=provision('ecdsa','ECDSA');wrong=provision('wrong_identity','RSA');mismatch=provision('mismatch','RSA',bind_certificate='rsa.der')
            for item in (rsa,ecdsa):
                shutil.copyfile(directory/(item['label']+'.pem'),directory/'client.pem')
                for tls in ('1.2','1.3'):
                    p=peer('good',tls=tls,client=True)
                    try:
                        run(item['label']+'_software_store_tls'+tls,p,True,trust='aws',tls=tls,identity=item['reference'],kind='store')
                    finally:p.close()
            shutil.copyfile(directory/'rsa.pem',directory/'client.pem');p=peer('good',client=True)
            try:
                run('two_independent_store_clients',p,True,trust='aws',identity=rsa['reference'],kind='store',owners=True)
                run('missing_identity',p,False,trust='aws')
                run('missing_thumbprint',p,False,trust='aws',identity=rsa['reference'][:-40]+'0'*40,kind='store')
                run('missing_store',p,False,trust='aws',identity='CurrentUser\\VOID-ADAPTER-TEST-STORE-'+secrets.token_hex(32).upper()+'\\'+'0'*40,kind='store')
                run('wrong_identity',p,False,trust='aws',identity=wrong['reference'],kind='store')
                run('certificate_key_mismatch',p,False,trust='aws',identity=mismatch['reference'],kind='store')
                run('signing_async_retry_unsupported',p,False,trust='aws',identity=rsa['reference'],kind='store',mode='sign-retry')
            finally:p.close()
            p=peer('good',tls='1.2',client=True)
            try:run('rsa_pkcs1_sha256',p,True,trust='aws',tls='1.2',identity=rsa['reference'],kind='store',mode='pkcs1')
            finally:p.close()
            p=peer('good',client=True)
            try:
                run('rsa_pss_sha384',p,True,trust='aws',identity=rsa['reference'],kind='store',mode='pss384')
                run('rsa_pss_sha512',p,True,trust='aws',identity=rsa['reference'],kind='store',mode='pss512')
            finally:p.close()
            # Both independent signing algorithms have passed before combining
            # them with the independently established Windows verifier.
            for item in (rsa,ecdsa):
                shutil.copyfile(directory/(item['label']+'.pem'),directory/'client.pem')
                for tls in ('1.2','1.3'):
                    p=peer('good',tls=tls,client=True)
                    try:run(item['label']+'_combined_tls'+tls,p,True,tls=tls,identity=item['reference'],kind='store')
                    finally:p.close()
            for item in objects:
                after=fixture_call('inspect',item,item['certificate']);a=[r for r in item['before']['records'] if 'public_blob' in r];b=[r for r in after['records'] if 'public_blob' in r];assert a==b,'borrowed credential changed'
        if args.track=='scope':
            item=provision('scope_rsa','RSA');shutil.copyfile(directory/'scope_rsa.pem',directory/'client.pem')
            origin=peer('good',client=True);proxy=peer('good',proxy=True)
            proxy.context.verify_mode=ssl.CERT_OPTIONAL
            try:
                row=run('origin_store_identity_scope',origin,True,identity=item['reference'],kind='store',proxy=proxy)
                observed=any(r.get('exact_der_match') for r in row['proxy'])
                row['security_assertion']={'expected_proxy_client_identity':False,'observed_proxy_client_identity':observed,'passed':not observed}
                save()
                # This is an expected reproducer of a failed privacy contract,
                # never a passing security gate or a production workaround.
                assert observed,'scope-leak hypothesis not reproduced; reassess source'
                print('FAILED SECURITY GUARANTEE: origin store identity disclosed to requesting HTTPS proxy',flush=True)
            finally:origin.close();proxy.close()
            after=fixture_call('inspect',item,item['certificate'])
            assert [r for r in item['before']['records'] if 'public_blob' in r]==[r for r in after['records'] if 'public_blob' in r]
        if args.track=='proxy-origin':
            # The inherited loopback CONNECT fixture rejects wrong.test before
            # origin TLS. Preserve that original inconclusive row; exercise the
            # actual hostname policy using localhost and a wrong-SAN leaf.
            issue('wrong_origin',san='DNS:wrong.test')
            origin=peer('wrong_origin');proxy=peer('good',proxy=True)
            try:
                row=run('origin_wrong_san_after_verified_proxy',origin,False,proxy=proxy)
                policies=[r['status'] for r in row['client'] if r.get('operation')=='native_ssl_policy']
                verifications=[r for r in row['client'] if 'windows_verification' in r]
                assert policies==['0x00000000','0x800B010F'],row
                assert [r['hostname'] for r in verifications]==['proxy.test','localhost'],row
                assert [r['windows_verification'] for r in verifications]==[True,False],row
            finally:origin.close();proxy.close()
    except BaseException as error:
        fault=str(error).replace(str(directory),'<fixture>');save();raise
    finally:
        cleanup_errors=[]
        for item in reversed(objects):
            if item['public'].exists():
                try:fixture_call('cleanup',item,item['certificate'])
                except BaseException as error:cleanup_errors.append(str(error))
        # Native AIA retrieval may add this exact generated public certificate
        # to CurrentUser CA. Never delete an entry present before the study.
        ca_store_hash=hashlib.sha256(b'CA').hexdigest()
        original_ca={c for s in before['stores'] if s['store_name_sha256']==ca_store_hash for c in s['certificate_sha256']}
        for public in directory.glob('intermediate*.der'):
            expected_hash=sha(public).upper()
            if expected_hash in original_ca:continue
            process=subprocess.run([str(fixture),'purge-aia',str(public),expected_hash],capture_output=True,text=True,timeout=15)
            fixture_records.append({'action':'purge-generated-aia','certificate_sha256':expected_hash,'exit':process.returncode,
                'records':[json.loads(line) for line in process.stdout.splitlines()]})
            if process.returncode:cleanup_errors.append('generated AIA cleanup failed')
        web.shutdown();web.server_close();after=inventory()
        if cleanup_errors:fault='fixture cleanup failed: '+str(cleanup_errors)
        if after!=before:fault=(fault or '')+' OS inventory changed'
        save(complete=fault is None,after=after)
        if fault:shutil.copytree(directory,OUT/(directory.name+'-retained'),ignore=shutil.ignore_patterns('*.key','*.p12'))
        assert directory.parent.resolve()==OUT.resolve();shutil.rmtree(directory)
        if cleanup_errors or after!=before:raise RuntimeError(fault)
    print(len(rows),'native controls; owned fixture credentials removed; unrelated inventories unchanged')

if __name__=='__main__':main()
