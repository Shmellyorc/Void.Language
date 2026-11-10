from hashlib import sha256
from pathlib import Path
from zipfile import ZipFile
import json
import shutil

repo = Path.cwd().resolve()
audit = repo/'docs/projects/evidence/slice7-g3-final'
prior = Path.home()/'VM/project-slice7-g3'
share = Path.home()/'VM/project-slice7-g3-final'
report = json.loads((prior/'native-results.json').read_text())
assert report['status'] == 'PASS' and report['restored_original_source']
packet = prior/'source-packet.zip'
assert sha256(packet.read_bytes()).hexdigest() == report['source_packet_sha256']
hashes = report['source_inputs_sha256']
expected = {'src/runtime/c_resource.cpp', 'tests/ffi/resources.cpp'}
changed = set()
share.mkdir(mode=0o700)
with ZipFile(packet) as original, ZipFile(share/'source-packet.zip', 'x') as final:
    assert set(original.namelist()) == set(hashes) and len(hashes) == 3288
    for item in original.infolist():
        old = original.read(item)
        assert sha256(old).hexdigest() == hashes[item.filename], item.filename
        source = repo/item.filename
        assert source.resolve(strict=True) == source and source.is_relative_to(repo), source
        content = source.read_bytes()
        if content != old:
            assert item.filename in expected, item.filename
            changed.add(item.filename)
        final.writestr(item, content)
        hashes[item.filename] = sha256(content).hexdigest()
assert changed == expected
(share/'source-packet-sha256.json').write_text(json.dumps(hashes, indent=2)+'\n')
for name in ('native.py', 'native.cmd', 'storage.ps1', 'storage.cmd'):
    shutil.copyfile(audit/'windows'/name, share/name)
print(f"Verified {len(hashes)} packet inputs. Updated {', '.join(sorted(changed))}.")
print(sha256((share/'source-packet.zip').read_bytes()).hexdigest())
