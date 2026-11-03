"""Independent, bounded collision investigation. Run from an approved native VM."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import shutil
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
OUT = ROOT / '.toolchains/native-schannel'
DEST = ROOT / 'docs/net/http/evidence/windows-cng-collision-20261103.json'
CASES = [('reserved', 'normal', a) for a in ('RSA', 'ECDSA')]
CASES += [('reserved', 'collision', a) for a in ('RSA', 'ECDSA')]
CASES += [('reserved', 'stop-'+stage, 'RSA') for stage in
          ('intent', 'import', 'finalize', 'store-unmarked', 'store-marked', 'key-unmarked', 'wrong-marker')]
CASES += [('reserved', 'concurrent', 'RSA'), ('pkcs8', 'collision', 'RSA'), ('pkcs8', 'deferred-collision', 'RSA')]
ROUTINE = {'property_size', 'property_read', 'public_size', 'public_export', 'sha256',
           'token_open', 'token_user', 'sid_convert', 'dacl_construct', 'spki_size', 'spki_export'}

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def compact(value):
    if isinstance(value, list):
        return [compact(v) for v in value if not isinstance(v, dict) or v.get('operation') not in ROUTINE]
    if isinstance(value, dict):
        return {k: ('<private CurrentUser test-store selector>' if k == 'selector' else compact(v)) for k, v in value.items()}
    return value

def inventory(profile):
    p = subprocess.run([str(OUT/profile/'probe.exe'), 'inventory'], capture_output=True, text=True, check=True, timeout=30)
    keys = [json.loads(line) for line in p.stdout.splitlines()]
    script = r'''$ErrorActionPreference='Stop';
    $registry=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey('Software\Microsoft\SystemCertificates');
    try {$names=$registry.GetSubKeyNames()} finally {$registry.Dispose()};
    $rows = @($names | ForEach-Object {
      $name=$_; $store=[System.Security.Cryptography.X509Certificates.X509Store]::new($name,[System.Security.Cryptography.X509Certificates.StoreLocation]::CurrentUser);
      try {$store.Open([System.Security.Cryptography.X509Certificates.OpenFlags]::ReadOnly -bor [System.Security.Cryptography.X509Certificates.OpenFlags]::OpenExistingOnly);
        $certs=@($store.Certificates | ForEach-Object {$_.GetCertHashString([System.Security.Cryptography.HashAlgorithmName]::SHA256)});
        [pscustomobject]@{store=$name;certificates=$certs}
      } finally {$store.Dispose()}
    }); ConvertTo-Json -InputObject $rows -Depth 4 -Compress'''
    stores = json.loads(subprocess.check_output(['powershell', '-NoProfile', '-Command', script], text=True, timeout=30))
    return {'keys': [r for r in keys if 'key_inventory_sha256' in r or 'key_count' in r],
            'stores': sorted([{'store_name_sha256': hashlib.sha256(s['store'].encode()).hexdigest(),
                               'certificate_sha256': sorted(s['certificates'])} for s in stores], key=lambda s:s['store_name_sha256'])}

def validate(data, creation, mode):
    assert data['residual'] is False and data['recovery'][-1]['exit'] == 0
    operations = {r.get('operation'): r.get('status') for r in data['recovery'][-1]['records']}
    assert operations['recovery_key_open'] == '0x80090016' and operations['recovery_store_open'] == '0x00000002'
    assert data['probe_source_sha256'] == sha(HERE/'probe.cpp')
    assert data['import_source_sha256'] == sha(HERE/'reserved-import.hpp')
    if creation == 'reserved' and mode == 'collision':
        assert data['probe_exit'] == 1
        assert {'failure': 'candidate late collision accepted'} in data['records']
        operations = {r.get('operation'): r.get('status') for r in data['records']}
        assert operations['gate_race_first_finalize'] == operations['gate_race_second_finalize'] == '0x00000000'
        assert {'race_public_unchanged': False, 'race_all_properties_unchanged': False} in data['records']
        snapshots = {r['snapshot']:r for r in data['records'] if 'snapshot' in r}
        for kind in ('same', 'different'):
            a, b = (dict(snapshots[kind+'_'+stage]) for stage in ('before', 'after'))
            del a['snapshot']; del b['snapshot']; assert a == b
        assert operations['malformed_blob_rejected'] == '0x80090005'
        assert operations['invalid_finalize_flags_rejected'] == '0x80090009'
        matrix = {r['matrix_result']:r for r in data['records'] if 'matrix_result' in r}
        for kind in ('pkcs8', 'raw'):
            for timing in ('immediate', 'deferred'):
                assert matrix[kind+'_'+timing]['public_unchanged'] is (kind == 'raw')
                assert matrix[kind+'_'+timing]['all_properties_unchanged'] is (kind == 'raw')
        races = [r for r in data['records'] if 'raw_race_round' in r]
        assert len(races) == 8 and all(r['successful_creators'] > 1 for r in races)
    elif creation == 'pkcs8':
        assert data['probe_exit'] == 1
        assert any(r.get('sentinel_public_unchanged') is False and r.get('sentinel_marker_unchanged') is False for r in data['records'])
    elif mode.startswith('stop-'):
        assert data['recovery'][0]['exit'] == 3
        rows = {r['case']:r for r in data['recovery']}
        quarantine = mode in ('stop-store-unmarked', 'stop-key-unmarked', 'stop-wrong-marker')
        assert rows['after_termination']['exit'] == (4 if quarantine else 0)
        if quarantine: assert rows['explicit_test_owner_cleanup']['exit'] == 0
    else:
        assert data['probe_exit'] == 0
        positives = [h for h in data['handshakes'] if h['case'] == 'named' and h['requested_tls'] == '1.2']
        assert positives and all(h['exit'] == 0 and any(r.get('exact_der_match') for r in h['peer'])
                                 and [r['new_connections'] for r in h['client']] == [1, 0] for h in positives)
        assert all('TLS 1.3 not supported' in h['error'] if h['requested_tls'] == '1.3' else h['exit'] != 0
                   for h in data['handshakes'] if h not in positives)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--run', action='store_true')
    args = parser.parse_args()
    report = {'schema': 1, 'date': '2026-11-03', 'source_baseline': '23e642dc377ff643b6d76f806d84fd1b5f8d60ca',
              'os': 'Windows 10 Home 22H2 x64 10.0.19045.6456', 'provider': 'Microsoft Software Key Storage Provider',
              'provider_version': 65536, 'ncrypt_module_file_version': '10.0.19041.1 (WinBuild.160101.0800)',
              'ncryptprov_module_file_version': '10.0.19041.1 (WinBuild.160101.0800)',
              'toolchain': {'clang': '23.1.4', 'lld': '23.1.4', 'windows_sdk': '10.0.19041.0', 'crt': '/MT'},
              'prior_evidence_sha256': sha(ROOT/'docs/net/http/evidence/windows-native-schannel-20261103.json'),
              'verdict': 'REJECT: create/set-blob/finalize does not reserve a name against a pending competing creator',
              'profiles': {}, 'source_sha256': {}}
    for profile in ('build', 'build-release'):
        before_path, after_path = (OUT/f'{profile}-collision-study-inventory-{stage}.json' for stage in ('before', 'after'))
        if args.run: before_path.write_text(json.dumps(inventory(profile)))
        rows = {}
        for creation, mode, algorithm in CASES:
            suffix = ('reserved-' if creation == 'reserved' else '')+mode+'-'+algorithm
            path = OUT/f'{profile}-collision-study-{suffix}.json'
            if args.run:
                if path.exists():
                    shutil.copyfile(path, OUT/('earlier-'+path.stem+'-'+sha(path)[:12]+'.json'))
                with (OUT/f'{profile}-collision-study-{suffix}.log').open('w') as log:
                    p = subprocess.run([sys.executable, str(HERE/'run.py'), '--study', '--build', profile,
                                        '--creation', creation, '--mode', mode, '--algorithm', algorithm], stdout=log, stderr=subprocess.STDOUT, timeout=150)
                expected = 1 if creation == 'reserved' and mode == 'collision' else 0
                assert p.returncode == expected, (profile, suffix, p.returncode, expected)
            data = json.loads(path.read_text()); validate(data, creation, mode)
            assert data['probe_binary_sha256'] == sha(OUT/profile/'probe.exe')
            assert data['peer_binary_sha256'] == sha(OUT/profile/'peer-client.exe')
            rows[suffix] = compact(data)
            print(profile, suffix, 'recorded', flush=True)
        if args.run: after_path.write_text(json.dumps(inventory(profile)))
        before, after = (json.loads(p.read_text()) for p in (before_path, after_path))
        assert before == after, 'unrelated CurrentUser inventory changed'
        report['profiles'][profile] = {'cases': rows, 'inventory_before': before, 'inventory_after': after}
    for name in ('probe.cpp', 'reserved-import.hpp', 'run.py', 'collision-study.py', 'peer.py', 'peer-client.cpp', 'CMakeLists.txt'):
        report['source_sha256'][name] = sha(HERE/name)
    report['residual_credentials'] = 0
    report['earlier_experiments'] = {p.name:compact(json.loads(p.read_text())) for p in OUT.glob('earlier-*-collision-study-*.json')}
    for name in ('reserved-gate.log', 'reserved-gate-expanded.log', 'build-reserved-normal-RSA.json'):
        path = OUT/name
        if path.exists(): report['earlier_experiments'][name] = compact(json.loads(path.read_text()))
    encoded = json.dumps(report, separators=(',', ':'))+'\n'
    assert 'PRIVATE KEY' not in encoded and 'C:\\' not in encoded and 'S-1-5-21' not in encoded
    DEST.write_text(encoded, encoding='utf-8')
    print('28 independent case receipts; rejected candidate retained; unchanged inventories; no residual credentials')

if __name__ == '__main__': main()
