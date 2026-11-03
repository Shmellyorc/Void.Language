"""Validate public native receipts and collect no credentials or host paths."""
import hashlib
import json
from pathlib import Path

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
OUT=ROOT/'.toolchains/native-awslc'
DEST=ROOT/'docs/net/http/evidence/windows-awslc-provider-20261103.json'
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def sanitize(value):
    if isinstance(value,str):return value.replace(str(ROOT),'<repository>').replace(str(ROOT).replace('\\','/'),'<repository>')
    if isinstance(value,list):return [sanitize(v) for v in value]
    if isinstance(value,dict):return {k:sanitize(v) for k,v in value.items()}
    return value

report={'schema':1,'baseline':'51e7ff6f8c202db56fc0a317b87d73ddb10972d9','date':'2026-11-03',
    'os':'Windows 10 Home 22H2 x64 10.0.19045.6456',
    'toolchain':{'clang':'23.1.4','lld':'23.1.4','windows_sdk':'10.0.19041.0','crt':'/MT','nasm':'2.16.03','perl':'Git distribution'},
    'verdict':'In-memory P12 transport works; unchanged AWS-LC does not preserve Schannel revocation or Windows store identity semantics',
    'profiles':{},'earlier_experiments':{},'build':json.loads((OUT/'build-receipt.json').read_text()),'source_sha256':{}}
for profile in ('Debug','Release'):
    data=json.loads((OUT/(profile+'-result.json')).read_text())
    assert len(data['cases'])==38
    assert data['inventory_before']==data['inventory_after'] and data['fixture_directories_removed'] is True
    assert data['probe_sha256']==sha(OUT/profile/'curl/probe.exe')
    for name,digest in data['source_sha256'].items():assert digest==sha(HERE/name)
    for case in data['cases']:
        assert (case['exit']==0)==case['expected_success'],case['case']
        assert any(r.get('provider_handles_released') for r in case['client'])
        if case['expected_success']:
            assert any(r.get('exact_der_match') and r.get('tls')=='TLSv'+case['tls']
                and r.get('alpn')==('h2' if case['protocol']=='h2' else 'http/1.1') for r in case['peer'])
            assert [r['new_connections'] for r in case['client'] if 'new_connections' in r]==[1,0]
            expected_http=3 if case['protocol']=='h2' else 2
            assert all(r['http_version']==expected_http for r in case['client'] if 'http_version' in r)
            assert any(r.get('tls_provider')=='AWS-LC/5.12.0' for r in case['client'])
        if case['case']=='in_memory_p12':assert [r['ssl_context_trust_objects'] for r in case['client'] if 'ssl_context_trust_objects' in r]==[1]
        if case['case']=='native_plus_explicit':assert any(r.get('ssl_context_trust_objects',0)>1 for r in case['client'])
        if case['case']=='explicit_crl_revoked':assert any(r.get('curl_status')==60 for r in case['client'])
        if case['case']=='ocsp_staple_required_missing':assert any(r.get('curl_status')==91 for r in case['client'])
        if case['case']=='https_proxy_p12_identity':assert any(r.get('exact_der_match') for r in case['proxy'])
        if case['case'] in ('https_proxy_identity_absent','https_proxy_wrong_root','https_proxy_wrong_hostname'):
            assert not any(r.get('event')=='connect' for r in case['proxy'])
    assert all(f['cdp_http_requests']==0 for f in data['fixtures'])
    data['link_files']={str(p.relative_to(OUT/profile)).replace('\\','/'):{'bytes':p.stat().st_size,'sha256':sha(p)}
        for p in (OUT/profile/'aws-lc/crypto/crypto.lib',OUT/profile/'aws-lc/ssl/ssl.lib',OUT/profile/'prefix/lib/nghttp2.lib',OUT/profile/'curl/probe.exe')}
    report['profiles'][profile]=data
selection=json.loads((OUT/'selection.json').read_text());assert len(selection)==4
for row in selection:
    assert {'first_selection':0,'global_init':0,'other_selection_after_init':2} in row['records']
    assert row['probe_sha256']==sha(OUT/row['profile']/'multi/probe.exe')
report['multi_ssl_selection']=selection
for path in OUT.glob('earlier-*.json'):report['earlier_experiments'][path.name]=json.loads(path.read_text())
# Preserve compact excerpts and hashes of original build/download failures.
report['original_failure_logs']={}
for path in sorted(OUT.glob('earlier-*.log')):
    lines=path.read_text(errors='replace').splitlines()
    errors=[line for line in lines if any(word in line for word in ('CMake Error','Could NOT find Perl','Could NOT find OpenSSL','error:'))]
    if errors:report['original_failure_logs'][path.name]={'sha256':sha(path),'error_lines':errors[:40]}
download_log=ROOT/'.toolchains/native-awslc-Debug-run.log'
if download_log.exists():report['original_failure_logs'][download_log.name]={'sha256':sha(download_log),
    'error_lines':[line for line in download_log.read_text().splitlines() if line.startswith('urllib.error.URLError:')]}
for path in HERE.iterdir():
    if path.is_file() and path.suffix in ('.py','.cpp','.md','.txt'):report['source_sha256'][path.name]=sha(path)
report['credential_inventory_unchanged']=True
report['native_process_checks']=80
encoded=json.dumps(sanitize(report),separators=(',',':'))+'\n'
assert 'PRIVATE KEY' not in encoded and 'C:\\Users\\' not in encoded and 'S-1-5-21' not in encoded
DEST.write_text(encoded,encoding='utf-8')
print('76 TLS/security controls plus 4 MultiSSL controls validated; original failures retained; OS inventories unchanged')
