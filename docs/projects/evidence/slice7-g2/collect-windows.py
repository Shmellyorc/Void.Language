from pathlib import Path
from hashlib import file_digest
import json
import shutil
import xml.etree.ElementTree as ET

share = Path('/home/shmellyorc/VM/project-slice7-g2')
dest = Path('docs/projects/evidence/slice7-g2/windows')
qualification = json.loads((share/'native-results.json').read_text())
assert qualification['status'] == 'PASS' and qualification['restored_original_source']
assert len(qualification['source_inputs_sha256']) == 3278
with (share/'source-packet.zip').open('rb') as archive:
    assert file_digest(archive, 'sha256').hexdigest() == qualification['source_packet_sha256']
changes = json.loads((dest.parent/'source-hashes.json').read_text())
assert all(qualification['source_inputs_sha256'][name] == item['after'] for name, item in changes.items())
files = ['native-results.json', 'native.py', 'native.cmd', 'console.log', 'vcvars.log', 'storage.json',
         'storage.ps1', 'storage.cmd', 'source-packet-sha256.json', 'copy-ctest.cmd', 'replay.py', 'replay.cmd', 'replay-console.log', 'replay-vcvars.log']
for profile in qualification['profiles']:
    name = profile['profile']
    assert profile['status'] == 'PASS' and profile['os_build'] == 19045 and len(profile['g1_artifacts']) == 11
    cases = ET.parse(share/(name + '-ctest.xml')).getroot()
    assert cases.attrib['tests'] == '44' and cases.attrib['failures'] == '0'
    assert 'JIT O2 returned 73; direct C oracle agreed; long=32 bits' in (share/(name+'-g1-direct.log')).read_text()
    assert len(profile['g2_artifacts']) == 8
    assert 'G2 JIT O2 returned 103' in (share/(name+'-g2-direct.log')).read_text()
    for mode in ('normal','optimized'):
        assert '7 0 103' in (share/(name+'-g2-aot-'+mode+'.log')).read_text()
    for toolchain in json.loads((share/(name+'-g2-c-abi.json')).read_text()):
        assert any('sret' in line and '@g2_calibrate' in line for line in toolchain['C_IR_signatures'])
        assert any('sret' in line and '@g2_packet' in line for line in toolchain['C_IR_signatures'])
    for mode in ('normal', 'optimized'):
        assert (share/(name+'-g1-aot-'+mode+'.log')).read_text().strip() == '73'
    assert 'provider rejected the exact descriptor or fixture identity' in (share/(name+'-g1-aot-mismatch.log')).read_text()
    suffixes = ('result.json','configure.log','build.log','ctest.log','ctest.xml','LastTest.log','g1-direct.log',
                'g1-aot-normal.log','g1-aot-optimized.log','g1-aot-mismatch.log','g1-c-compile-commands.json',
                'primitives.descriptor','primitives.void','g2-direct.log','g2-aot-normal.log',
                'g2-aot-optimized.log','g2-c-compile-commands.json','g2-c-abi.json','records.descriptor',
                'g2-abi-fixture.log','g2-abi-thunk.log')
    files.extend(name + '-' + suffix for suffix in suffixes)
preserved = {}
for name in files:
    source = share/name
    target = dest/(source.stem+'.txt' if source.suffix == '.log' else name)
    shutil.copyfile(source,target)
    with source.open('rb') as left, target.open('rb') as right:
        preserved[name] = file_digest(left, 'sha256').hexdigest()
        assert file_digest(right, 'sha256').hexdigest() == preserved[name]
(share/'preserved-evidence.json').write_text(json.dumps(preserved, indent=2) + '\n')
shutil.copyfile(share/'preserved-evidence.json', dest/'preserved-evidence.json')
print({p['profile']: p['status'] for p in qualification['profiles']})
print('Verified exact source-packet inputs for all',len(changes),'G2 source paths, saved',len(files),'receipts')
