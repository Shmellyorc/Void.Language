"""Validate and compact public receipts; never turn a reproduced flaw into a pass."""
import hashlib
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
OUT = ROOT / '.toolchains/native-awslc-adapters'
DEST = ROOT / 'docs/net/http/evidence/windows-awslc-adapters-20261103.json'
CORE = ('common.hpp', 'trust.hpp', 'signing.hpp', 'probe.cpp', 'fixture.cpp')
ROUTINE = {'key_property_size', 'key_property_read', 'public_size', 'public_export',
           'public_info_size', 'public_info', 'certificate_sha256', 'public_metadata_sha256',
           'token_open', 'token_sid', 'sid_text', 'key_acl', 'subject_size', 'subject', 'csr_size'}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inventory(value):
    if value is None:
        return None
    if 'sha256' in value:
        return value
    return {'sha256': hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest(),
            'key_count': next(r['key_count'] for r in value['keys'] if 'key_count' in r),
            'store_count': len(value['stores']),
            'certificate_count': sum(len(s['certificate_sha256']) for s in value['stores'])}


def compact(value):
    if isinstance(value, str):
        return value.replace(str(ROOT), '<repository>').replace(ROOT.as_posix(), '<repository>')
    if isinstance(value, list):
        return [compact(v) for v in value if not isinstance(v, dict) or
                v.get('operation') not in ROUTINE or v.get('status') != '0x00000000']
    if isinstance(value, dict):
        result = {}
        for key, item in value.items():
            if key in ('inventory_before', 'inventory_after'):
                result[key] = inventory(item)
            elif key == 'public_blob':
                result['public_blob_sha256'] = hashlib.sha256(bytes.fromhex(item)).hexdigest()
            else:
                result[key] = compact(item)
        return result
    return value


def read(path):
    data = json.loads(path.read_text())
    data['raw_receipt_sha256'] = sha(path)
    if 'inventory_before' in data:
        data['inventories_equal'] = data['inventory_before'] == data['inventory_after']
    for case in data.get('cases', []):
        if case['case'] == 'origin_wrong_hostname_after_proxy':
            case['assessment'] = 'INCONCLUSIVE: fixture rejected CONNECT before origin TLS; corrected separately'
        elif 'security_assertion' in case:
            case['assessment'] = 'FAILED SECURITY GUARANTEE: origin identity disclosed to HTTPS proxy'
        else:
            case['assessment'] = 'Recorded expected command outcome; see report for capability limits'
    return data


def validate(data, profile, track):
    assert data['complete'] and data['error'] is None and data['inventories_equal']
    assert len(data['cases']) == (39 if track == 'all' else 1)
    assert data['probe_sha256'] == sha(OUT / profile / 'adapter-probe.exe')
    assert data['fixture_sha256'] == sha(OUT / profile / 'fixture.exe')
    for name in CORE:
        assert data['source_sha256'][name] == sha(HERE / name), name
    for case in data['cases']:
        assert (case['exit'] == 0) == case['expected_success'], case['case']
        assert any(r.get('borrowed_identity_handles_released') for r in case['client'])
        if case['expected_success']:
            requests = [r for r in case['client'] if 'curl_status' in r]
            assert all(r['curl_status'] == 0 for r in requests)
            assert sorted(r['new_connections'] for r in requests) == (
                [0, 1, 1] if case['case'] == 'two_independent_store_clients' else [0, 1])
        if track == 'scope':
            assert case['security_assertion'] == {
                'expected_proxy_client_identity': False, 'observed_proxy_client_identity': True, 'passed': False}
        if track == 'proxy-origin':
            assert [r['status'] for r in case['client'] if r.get('operation') == 'native_ssl_policy'] == [
                '0x00000000', '0x800B010F']
    for record in data['fixture_records']:
        assert record['exit'] == 0
    labels = {r['fixture'] for r in data['fixture_records'] if r['action'] == 'create'}
    for label in labels:
        snapshots = [r for r in data['fixture_records'] if r.get('fixture') == label and r['action'] == 'inspect']
        public = [[v for v in r['records'] if 'public_blob' in v] for r in snapshots]
        assert len(public) == 2 and public[0] == public[1]
        assert any(r.get('fixture') == label and r['action'] == 'cleanup' for r in data['fixture_records'])


def main():
    report = {'schema': 1, 'baseline': '5bf1c514cb9c6a52d00a1ee359d5c54af762e8ec',
              'date': '2026-11-03', 'os': 'Windows 10 Home 22H2 x64 10.0.19045.6456',
              'toolchain': {'clang': '23.1.4', 'lld': '23.1.4', 'sdk': '10.0.19041.0', 'crt': '/MT'},
              'verdict': 'REJECT current public-hook integration; retain Schannel for the full Windows contract',
              'counts': {'current_network_observations': 82, 'valid_functional_or_limitation_controls': 78,
                         'inconclusive_original_proxy_controls': 2, 'failed_identity_scope_assertions': 2,
                         'read_only_public_anchor_controls': 2},
              'profiles': {}, 'earlier_experiments': {}, 'source_sha256': {},
              'source_chronology': 'Core C++ and actual binaries match final all/scope/proxy-origin receipts. '
              'study.py later gained the scope and corrected proxy control; CMake later gained root-audit. '
              'Historical controller/CMake hashes are retained, not represented as final-source executions.'}
    for profile in ('Debug', 'Release'):
        rows = {}
        for track in ('all', 'scope', 'proxy-origin'):
            data = read(OUT / (profile + '-' + track + '.json'))
            validate(data, profile, track)
            rows[track] = compact(data)
        roots = read(OUT / (profile + '-roots.json'))
        assert roots['exit'] == 0 and roots['inventories_equal']
        assert roots['root_audit_sha256'] == sha(OUT / profile / 'root-audit.exe')
        for name, digest in roots['source_sha256'].items():
            assert digest == sha(HERE / name)
        assert {r['operation']: r['status'] for r in roots['records'] if 'operation' in r}[
            'system_anchor_explicit_replacement_trust'] == '0x00000020'
        rows['public_anchor'] = compact(roots)
        report['profiles'][profile] = rows
    # Preserve original failures and earlier independent track observations.
    paths = list(OUT.glob('earlier-Debug-*.json')) + [OUT / 'Debug-trust.json', OUT / 'Debug-sign.json']
    for path in sorted(paths):
        report['earlier_experiments'][path.name] = compact(read(path))
    report['aia_first_cleanup'] = {}
    for path in sorted(OUT.glob('aia-first*.jsonl')):
        report['aia_first_cleanup'][path.name] = {'sha256': sha(path),
            'records': [json.loads(line) for line in path.read_text().splitlines()]}
    report['build'] = json.loads((OUT / 'build.json').read_text())
    assert all(c['exit'] == 0 for c in report['build']['commands'])
    for profile, archives in report['build']['archives'].items():
        for name, item in archives.items():
            path = ROOT / '.toolchains/native-awslc' / profile / name
            assert sha(path) == item['sha256'] and path.stat().st_size == item['bytes']
    report['earlier_builds'] = {p.name: json.loads(p.read_text()) for p in OUT.glob('earlier-build-*.json')}
    report['source_sha256'] = {p.name: sha(p) for p in HERE.iterdir()
                               if p.suffix in ('.hpp', '.cpp', '.py', '.txt')}
    text = json.dumps(compact(report), indent=2) + '\n'
    for forbidden in ('BEGIN PRIVATE KEY', 'BEGIN RSA PRIVATE KEY', 'BEGIN EC PRIVATE KEY', 'S-1-5-21-', str(ROOT)):
        assert forbidden not in text, forbidden
    DEST.write_text(text)
    print('Collected 82 TLS observations (2 inconclusive, 2 privacy failures) and 2 public-anchor controls')


if __name__ == '__main__':
    main()
