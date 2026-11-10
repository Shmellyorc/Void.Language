from hashlib import file_digest
from pathlib import Path
import json
import shutil
from xml.etree import ElementTree

share = Path.home()/'VM/project-slice7-g3-final'
audit = Path('docs/projects/evidence/slice7-g3-final')
dest = audit/'windows'
result = json.loads((share/'native-results.json').read_text())
assert result['status'] == 'PASS' and result['restored_original_source']
prior = json.loads((audit.parent/'slice7-g3/windows/native-results.json').read_text())
changes = json.loads((audit/'source-hashes.json').read_text())
assert result['source_inputs_sha256'] == prior['source_inputs_sha256'] | {
    name: value['after'] for name, value in changes.items()
}
with (share/'source-packet.zip').open('rb') as packet:
    assert file_digest(packet, 'sha256').hexdigest() == result['source_packet_sha256']
assert set(p['profile'] for p in result['profiles']) == {'Release','Debug'}
files = ['native-results.json','console.log','vcvars.log','storage.json','storage-console.txt',
         'native.py','native.cmd','storage.ps1','storage.cmd','source-packet-sha256.json']
for profile in result['profiles']:
    name = profile['profile']
    assert profile['status'] == 'PASS' and profile['os_build'] == 19045
    cases = ElementTree.parse(share/f'{name}-ctest.xml').getroot().attrib
    assert (cases['tests'], cases['failures'], cases['disabled'], cases['skipped']) == ('14','0','0','0')
    runs = [step for step in profile['commands'] if step['label'] == 'ctest']
    assert all(step['argv'] == runs[-1]['argv'] for step in runs)
    assert runs[-1]['argv'][runs[-1]['argv'].index('-j') + 1] == '2' and runs[-1]['returncode'] == 0
    direct = (share/f'{name}-g3-direct.log').read_text()
    for observation in ('explicit close after reload retained the acquisition lease across the C release',
                        'shutdown after reload retained the acquisition lease across the C release'):
        assert direct.count(observation) == 2, observation
    for marker in ('null and refused acquisition, refused release and retry preserved exact ownership',
                   'creation-publication race passed', 'borrow vs wrong-thread close and shutdown passed',
                   'terminal provider error reported on shutdown after release',
                   'G3 JIT normal returned 104', 'G3 JIT O2 returned 104'):
        assert marker in direct, marker
    for mode in ('normal', 'optimized'):
        assert {'7 0 104','-4 0 22'} <= set((share/f'{name}-g3-aot-{mode}.log').read_text().splitlines())
    suffixes = ('result.json','configure.log','build.log','ctest.log','ctest.xml','LastTest.log',
                'g1-direct.log','g1-aot-normal.log','g1-aot-optimized.log','g1-aot-mismatch.log',
                'g1-c-compile-commands.json','primitives.descriptor','primitives.void','g2-direct.log',
                'g2-aot-normal.log','g2-aot-optimized.log','g2-c-compile-commands.json','records.descriptor',
                'g3-direct.log','g3-aot-normal.log','g3-aot-optimized.log','g3-c-compile-commands.json',
                'resources.descriptor')
    files.extend(f'{name}-{suffix}' for suffix in suffixes)
    files.extend(p.name for p in sorted(share.glob(f'{name}-attempt-*')))
preserved = {}
for name in files:
    source = share/name
    target = dest/(source.stem+'.txt' if source.suffix=='.log' else name)
    if target.exists():
        assert source.read_bytes() == target.read_bytes(), name
    else:
        shutil.copyfile(source,target)
    with source.open('rb') as a, target.open('rb') as b:
        preserved[name] = file_digest(a, 'sha256').hexdigest()
        assert file_digest(b, 'sha256').hexdigest() == preserved[name]
receipt = json.dumps(preserved, indent=2)+'\n'
(share/'preserved-evidence.json').write_text(receipt)
(dest/'preserved-evidence.json').write_text(receipt)
print(f"Verified both 14-test Windows profiles and preserved {len(files)} files.")
