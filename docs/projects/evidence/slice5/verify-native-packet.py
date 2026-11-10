from pathlib import Path
import hashlib
import json
import zipfile

directory = Path('/home/shmellyorc/VM/project-slice5/final')
inventory = json.loads((directory / 'source-packet-sha256.json').read_text())
with zipfile.ZipFile(directory / 'source-packet.zip') as archive:
    assert set(archive.namelist()) == set(inventory)
    for name, digest in inventory.items():
        data = Path(name).read_bytes()
        if name == 'tests/project/creation.cpp':
            fixture = directory.parent / 'fixture'
            assert (fixture / 'creation.cpp').read_bytes() == data, name
            assert hashlib.sha256(data).hexdigest() == (fixture / 'creation.sha256').read_text().strip(), name
        else:
            assert hashlib.sha256(data).hexdigest() == digest, name
        assert hashlib.sha256(archive.read(name)).hexdigest() == digest, name
print(f'{len(inventory)-1} packet files match final working-tree bytes; the corrected creation fixture matches the hashed addendum')
