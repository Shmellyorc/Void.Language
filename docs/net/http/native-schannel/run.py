"""Disposable fixture driver; run from repository root. No secrets in receipts."""
import csv
import hashlib
import json
import os
from pathlib import Path
import secrets
import shutil
import subprocess
import sys
import tempfile
import http.server
import functools
import threading
import argparse
from concurrent.futures import ThreadPoolExecutor
from peer import verify

ROOT = Path(__file__).resolve().parents[4]
OUT = ROOT / '.toolchains/native-schannel'
OPENSSL = Path('C:/Program Files/Git/usr/bin/openssl.exe')

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--mode',default='normal',choices=['normal','collision','deferred-collision','immediate','deny-delete','deny-store-delete','concurrent',
        'stop-intent','stop-import','stop-finalize','stop-store-unmarked','stop-store-marked','stop-key-unmarked','stop-wrong-marker'])
    parser.add_argument('--algorithm',default='RSA',choices=['RSA','ECDSA'])
    parser.add_argument('--build',default='build')
    parser.add_argument('--creation',default='pkcs8',choices=['pkcs8','reserved'])
    parser.add_argument('--study',action='store_true',help='write independent collision-study receipts')
    args=parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    scratch = Path(tempfile.mkdtemp(prefix='fixture-', dir=OUT))
    # Restrict the fresh disposable directory before generating private files.
    sid = list(csv.reader(subprocess.check_output(
        ['whoami', '/user', '/fo', 'csv', '/nh'], text=True).splitlines()))[0][1]
    subprocess.run(['icacls', str(scratch), '/inheritance:r', '/grant:r',
                    f'*{sid}:(OI)(CI)F', '*S-1-5-18:(OI)(CI)F'], check=True,
                   stdout=subprocess.DEVNULL)
    password = secrets.token_hex(24)
    records = []
    class QuietHandler(http.server.SimpleHTTPRequestHandler):
        def log_message(self, *args): pass
    crl_server=http.server.ThreadingHTTPServer(('127.0.0.1',0),functools.partial(QuietHandler,directory=str(scratch)))
    threading.Thread(target=crl_server.serve_forever,daemon=True).start()
    proc=None; second=None;resolved=False
    try:
        def openssl(*args, stdin=None):
            r = subprocess.run([str(OPENSSL), *args], cwd=scratch,
                               input=stdin.encode('ascii') if stdin is not None else None,
                               capture_output=True)
            if r.returncode: raise RuntimeError('fixture generation failed')
        openssl('req', '-x509', '-newkey', 'rsa:2048', '-noenc', '-keyout', 'ca.key',
                '-out', 'ca.pem', '-days', '2', '-subj', '/CN=VOID disposable CA',
                '-addext', 'basicConstraints=critical,CA:TRUE',
                '-addext', 'keyUsage=critical,keyCertSign,cRLSign')
        newkey=['-newkey','rsa:2048'] if args.algorithm=='RSA' else ['-newkey','ec','-pkeyopt','ec_paramgen_curve:P-256']
        openssl('req', '-new', *newkey, '-noenc', '-keyout', 'client.key',
                '-out', 'client.csr', '-subj', '/CN=VOID disposable client')
        (scratch/'client.ext').write_text('basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature\nextendedKeyUsage=clientAuth\n')
        openssl('x509', '-req', '-in', 'client.csr', '-CA', 'ca.pem', '-CAkey', 'ca.key',
                '-CAcreateserial', '-days', '2', '-extfile', 'client.ext', '-out', 'client.pem')
        openssl('req', '-new', '-newkey', 'rsa:2048', '-noenc', '-keyout', 'server.key',
                '-out', 'server.csr', '-subj', '/CN=localhost')
        (scratch/'server.ext').write_text('basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\nsubjectAltName=DNS:localhost\n'
            +f'crlDistributionPoints=URI:http://127.0.0.1:{crl_server.server_port}/revocation.crl\n')
        openssl('x509', '-req', '-in', 'server.csr', '-CA', 'ca.pem', '-CAkey', 'ca.key',
                '-CAcreateserial', '-days', '2', '-extfile', 'server.ext', '-out', 'server.pem')
        openssl('req', '-x509', '-newkey', 'rsa:2048', '-noenc', '-keyout', 'wrong-ca.key',
                '-out', 'wrong-ca.pem', '-days', '2', '-subj', '/CN=VOID wrong disposable CA')
        (scratch/'index.txt').write_text('');(scratch/'crlnumber').write_text('01\n')
        (scratch/'ca.cnf').write_text('[ca]\ndefault_ca=local\n[local]\ndatabase=index.txt\nprivate_key=ca.key\ncertificate=ca.pem\ndefault_md=sha256\ndefault_crl_days=1\ncrlnumber=crlnumber\n')
        openssl('ca','-gencrl','-config','ca.cnf','-out','revocation.pem')
        openssl('crl','-in','revocation.pem','-outform','DER','-out','revocation.crl')
        openssl('pkcs12', '-export', '-inkey', 'client.key', '-in', 'client.pem',
                '-certfile', 'ca.pem', '-out', 'client.p12', '-keypbe', 'PBE-SHA1-3DES',
                '-certpbe', 'PBE-SHA1-3DES', '-macalg', 'sha1', '-passout', 'stdin', stdin=password+'\n')
        executable=OUT/args.build/'probe.exe'
        def start(intent,mode):
            p=subprocess.Popen([str(executable),str(scratch/'client.p12'),str(scratch/intent),mode],
                                stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
            p.stdin.write(password+'\n');p.stdin.flush();return p
        def recovery(intent,operator=False):
            p=subprocess.run([str(executable),'operator-cleanup' if operator else 'recover',str(scratch/intent)],capture_output=True,text=True,timeout=15)
            return {'exit':p.returncode,'records':[json.loads(line) for line in p.stdout.splitlines()]}
        selected_mode=('reserved:' if args.creation=='reserved' else '')+args.mode
        proc=start('intent.txt',selected_mode)
        handshakes=[];recovery_rows=[];second_records=[];second_selector=None
        for line in proc.stdout:
            row = json.loads(line); records.append(row)
            if row.get('checkpoint'):
                recovery_rows.append({'case':'live_owner',**recovery('intent.txt')})
                proc.kill();proc.wait(timeout=10)
                if args.mode=='stop-finalize':
                    original=(scratch/'intent.txt').read_bytes()
                    for label,content in [('corrupt_record',b'bad\n'),('oversized_record',b'X'*4097)]:
                        (scratch/'bad-intent.txt').write_bytes(content)
                        recovery_rows.append({'case':label,**recovery('bad-intent.txt')})
                    fields=original.decode('ascii').splitlines();fields[1]='0'*64
                    (scratch/'bad-intent.txt').write_text('\n'.join(fields)+'\n')
                    recovery_rows.append({'case':'wrong_public_identity',**recovery('bad-intent.txt')})
                recovered=recovery('intent.txt');recovery_rows.append({'case':'after_termination',**recovered})
                if recovered['exit']==4:
                    recovery_rows.append({'case':'explicit_test_owner_cleanup',**recovery('intent.txt',True)})
                recovery_rows.append({'case':'absence_after_cleanup',**recovery('intent.txt')})
                break
            if row.get('ready'):
                journal=scratch/'intent.txt'
                replacement=scratch/'intent.next'
                with replacement.open('wb') as record:
                    record.write(journal.read_bytes());record.flush();os.fsync(record.fileno())
                os.replace(replacement,journal);records.append({'journal_replace_while_live':True})
                replacement.write_bytes(b'')
                try:os.replace(replacement,scratch/'intent.txt.lock');records.append({'lock_replace_error':0})
                except OSError as error:records.append({'lock_replace_error':error.winerror})
                recovery_rows.append({'case':'live_owner',**recovery('intent.txt')})
                if args.mode=='concurrent':
                    second=start('second-intent.txt',('reserved:' if args.creation=='reserved' else '')+'normal')
                    for second_line in second.stdout:
                        second_row=json.loads(second_line);second_records.append(second_row)
                        if second_row.get('ready'):second_selector=second_row['selector'];break
                    if not second_selector or second_selector==row['selector']:raise RuntimeError('independent owners')
                    with ThreadPoolExecutor(max_workers=2) as workers:
                        first_run=workers.submit(verify,scratch,OUT,row['selector'],args.build,password)
                        second_run=workers.submit(verify,scratch,OUT,second_selector,args.build,password)
                        handshakes=first_run.result()+second_run.result()
                else:handshakes=verify(scratch,OUT,row['selector'],args.build,password)
                proc.stdin.write('cleanup\n');proc.stdin.flush()
        rc = proc.wait(timeout=30)
        if second is not None:
            handshakes+=verify(scratch,OUT,second_selector,args.build,password)
            second.stdin.write('cleanup\n');second.stdin.flush()
            second_records.extend(json.loads(line) for line in second.stdout)
            if second.wait(timeout=30):raise RuntimeError('second owner failure')
            recovery_rows.append({'case':'second_absence_after_cleanup',**recovery('second-intent.txt')})
        if not args.mode.startswith('stop-'):
            recovery_rows.append({'case':'absence_after_cleanup',**recovery('intent.txt')})
        resolved=all(not any(r.get('residual') for r in check['records']) for check in recovery_rows) and recovery_rows[-1]['exit']==0
        report = {'schema':1, 'fixture':args.algorithm+' synthetic, two-day validity', 'mode':args.mode,'creation':args.creation,
                  'probe_exit':rc, 'records':records, 'handshakes':handshakes,'recovery':recovery_rows,'residual':not resolved,
                  'second_owner_records':second_records,
                  'probe_source_sha256':hashlib.sha256((Path(__file__).parent/'probe.cpp').read_bytes()).hexdigest(),
                  'import_source_sha256':hashlib.sha256((Path(__file__).parent/'reserved-import.hpp').read_bytes()).hexdigest(),
                  'probe_binary_sha256':hashlib.sha256(executable.read_bytes()).hexdigest(),
                  'peer_binary_sha256':hashlib.sha256((OUT/args.build/'peer-client.exe').read_bytes()).hexdigest()}
        (OUT/'initial-result.json').write_text(json.dumps(report, indent=2)+'\n')
        receipt_case=('collision-study-' if args.study else '')+('reserved-' if args.creation=='reserved' else '')+args.mode
        (OUT/f'{args.build}-{receipt_case}-{args.algorithm}.json').write_text(json.dumps(report, indent=2)+'\n')
        print(json.dumps(report, indent=2))
        if args.mode.startswith('stop-'):
            return 0 if resolved and recovery_rows[0]['exit']==3 else 1
        if args.creation=='pkcs8' and args.mode in ('collision','deferred-collision'):
            return 0 if rc==1 and resolved else 1
        good=rc==0 and resolved
        for h in handshakes:
            if h['requested_tls']=='1.3':good &= 'TLS 1.3 not supported' in h['error']
            elif h['case']=='named':
                good &= h['exit']==0 and any(r.get('exact_der_match') is True for r in h['peer'])
                good &= [r['new_connections'] for r in h['client']]==[1,0]
            else:good &= h['exit']!=0
        return 0 if good else 1
    finally:
        if proc is not None and proc.poll() is None:
            try:proc.stdin.write('cleanup\n');proc.stdin.flush();proc.wait(timeout=10)
            except Exception:proc.kill();proc.wait(timeout=10)
        if second is not None and second.poll() is None:
            try:second.stdin.write('cleanup\n');second.stdin.flush();second.wait(timeout=10)
            except Exception:second.kill();second.wait(timeout=10)
        if not resolved:
            for intent_name in ('intent.txt','second-intent.txt'):
                if (scratch/intent_name).exists():
                    shutil.copyfile(scratch/intent_name,OUT/(scratch.name+'-'+intent_name+'-public-recovery.txt'))
        crl_server.shutdown();crl_server.server_close()
        # Only this newly created, absolute, owned fixture directory.
        if scratch.parent.resolve() != OUT.resolve(): raise RuntimeError('cleanup boundary')
        shutil.rmtree(scratch)

if __name__ == '__main__': sys.exit(main())
