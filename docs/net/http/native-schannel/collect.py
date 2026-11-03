"""Collect only named public receipts, never fixture directories or credentials."""
import hashlib
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[4]
OUT=ROOT/'.toolchains/native-schannel'
DEST=ROOT/'docs/net/http/evidence/windows-native-schannel-20261103.json'
routine={'property_size','property_read','public_size','public_export','sha256',
         'token_open','token_user','sid_convert','dacl_construct','spki_size','spki_export'}

def compact(value):
    if isinstance(value,list):
        return [compact(v) for v in value if not isinstance(v,dict) or v.get('operation') not in routine]
    if isinstance(value,dict):
        return {k:('<private CurrentUser test-store selector>' if k=='selector' else compact(v)) for k,v in value.items()}
    return value

cases=['normal-RSA','normal-ECDSA','concurrent-RSA','collision-RSA','deferred-collision-RSA',
       'stop-intent-RSA','stop-import-RSA','stop-finalize-RSA','stop-store-unmarked-RSA',
       'stop-store-marked-RSA','stop-key-unmarked-RSA','stop-wrong-marker-RSA','deny-store-delete-RSA']
report={'schema':1,'date':'2026-11-03','source_baseline':'440427c5f86b8921b6208a5ff62bab93c0eaddad',
        'os':'Windows 10 Home 22H2 x64, 10.0.19045.6456',
        'toolchain':{'clang':'23.1.4','lld':'23.1.4','sdk':'10.0.19041.0','msvc_headers':'14.44.35207',
                     'crt':'static /MT','cmake':'4.4.3','python':'3.14.8','curl':'8.23.0 Schannel',
                     'fixture_openssl':'3.5.9','peer_openssl':'3.5.9',
                     'llvm_archive_sha256':'854bffcfc84528e0ee92490ff5c178920abd358a79c3e98f053e41e3e8e4c6d2'},
        'source_sha256':{},'profiles':{},'earlier_failures':{},
        'production_verdict':'HOLD: named PKCS8 collision overwrote a different owned sentinel without an overwrite flag',
        'tls13':'Explicitly requested; unsupported on this Windows 10 VM',
        'residual_credentials':0}
for profile in ['build','build-release']:
    report['profiles'][profile]={}
    for case in cases:
        data=json.loads((OUT/f'{profile}-{case}.json').read_text())
        assert data['probe_source_sha256']==hashlib.sha256((Path(__file__).parent/'probe.cpp').read_bytes()).hexdigest()
        assert data['probe_binary_sha256']==hashlib.sha256((OUT/profile/'probe.exe').read_bytes()).hexdigest()
        assert data['peer_binary_sha256']==hashlib.sha256((OUT/profile/'peer-client.exe').read_bytes()).hexdigest()
        assert data['residual'] is False
        assert data['recovery'][-1]['exit']==0
        absent={r.get('operation'):r.get('status') for r in data['recovery'][-1]['records']}
        assert absent['recovery_key_open']=='0x80090016'
        assert absent['recovery_store_open']=='0x00000002'
        if case in ('collision-RSA','deferred-collision-RSA'):
            assert data['probe_exit']==1
            assert any(r.get('sentinel_public_unchanged') is False and
                       r.get('sentinel_marker_unchanged') is False for r in data['records'])
        elif case.startswith(('normal-','concurrent-','deny-store-delete-')):
            assert data['probe_exit']==0
            named=[h for h in data['handshakes'] if h['case']=='named' and h['requested_tls']=='1.2']
            assert named and all(h['exit']==0 and any(p.get('exact_der_match') for p in h['peer']) for h in named)
            if case.startswith('deny-store-delete'):
                assert any(r.get('operation')=='denied_store_delete' and r.get('status')=='0x00000005' for r in data['records'])
        elif case.startswith('stop-'):
            checks={r['case']:r for r in data['recovery']}
            assert checks['live_owner']['exit']==3
            quarantine=case in ('stop-store-unmarked-RSA','stop-key-unmarked-RSA','stop-wrong-marker-RSA')
            assert checks['after_termination']['exit']==(4 if quarantine else 0)
            if quarantine:assert checks['explicit_test_owner_cleanup']['exit']==0
            if case=='stop-finalize-RSA':
                assert all(checks[k]['exit']==1 for k in ['corrupt_record','oversized_record','wrong_public_identity'])
        report['profiles'][profile][case]=compact(data)
for name in ['initial-aes-p12-result.json','initial-3des-textpipe-result.json',
             'plaintext-pkcs8-rejected.json','marker-persist-only-missing.json',
             'unknown-credentials-key-spec.json','revocation-unknown.json']:
    path=OUT/name
    if path.exists():report['earlier_failures'][name]=compact(json.loads(path.read_text()))
# The unsuccessful key-only deletion-denial control must remain visible.
path=OUT/'build-deny-delete-RSA.json'
if path.exists():report['earlier_failures']['key_delete_denial_ineffective']=compact(json.loads(path.read_text()))
report['earlier_failures']['session_transcript_excerpts']={
    'origin':'Native operation/status excerpts from earlier probe revisions, retained from the session transcript',
    'secret_only_encrypted_pkcs8_size':'0x80090029',
    'explicit_pbe_oid_without_pbe_parameters':{'encrypted_pkcs8_export':'0x00000000','named_import_deferred':'0x80092002'},
    'immediate_import_policy_control':{'named_import_immediate':'0x00000000','initial_export_policy':0,
                                       'attempt_to_set_export_policy_zero_after_import':'0x8009000B',
                                       'key_delete':'0x00000000','key_open_after_delete':'0x80090016'}}
for name in ['CMakeLists.txt','probe.cpp','peer-client.cpp','peer.py','run.py','README.md','collect.py']:
    path=Path(__file__).parent/name
    report['source_sha256'][name]=hashlib.sha256(path.read_bytes()).hexdigest()
encoded=json.dumps(report,separators=(',',':'))+'\n'
assert 'PRIVATE KEY' not in encoded and 'C:\\' not in encoded and 'S-1-5-21' not in encoded
DEST.write_text(encoded,encoding='utf-8')
print(f'{len(report["profiles"])*len(cases)} final profile/case receipts; {len(encoded)} public bytes; no residual credentials')
