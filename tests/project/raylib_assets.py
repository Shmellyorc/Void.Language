"""Extract optional native test assets from the already approved raylib archive.

No download. These assets are qualification inputs, not Git package dependencies.
"""
from hashlib import sha256
from pathlib import Path
import struct
import sys
import tarfile

archive, destination = map(Path, sys.argv[1:])
assert sha256(archive.read_bytes()).hexdigest() == '2b3ee1e2120c7a0796b33062c7e9a694dd8a8caa56a96319ac8c8ecf54a90d0b'
destination.mkdir(parents=True, exist_ok=False)
inputs = {'guy.iqm':'models/resources/models/iqm/guy.iqm',
          'guyanim.iqm':'models/resources/models/iqm/guyanim.iqm',
          'font.ttf':'text/resources/anonymous_pro_bold.ttf',
          'MODEL-LICENSE.md':'models/resources/LICENSE.md',
          'FONT-NOTICES.md':'text/resources/LICENSE.md'}
with tarfile.open(archive) as source:
    for name, member in inputs.items():
        data = source.extractfile('raylib-6.0/examples/'+member).read()
        (destination/name).write_bytes(data)
# Retain the font's embedded complete license with privately packaged test assets.
data = (destination/'font.ttf').read_bytes()
for i in range(struct.unpack_from('>H',data,4)[0]):
    tag, _, offset, size = struct.unpack_from('>4sIII',data,12+16*i)
    if tag == b'name':
        _, count, strings = struct.unpack_from('>HHH',data,offset)
        notices=[]
        for j in range(count):
            platform, _, _, name, length, at = struct.unpack_from('>HHHHHH',data,offset+6+12*j)
            if name not in (0,13,14):continue
            raw=data[offset+strings+at:offset+strings+at+length]
            notice=raw.decode('utf-16-be' if platform in (0,3) else 'mac_roman')
            if notice not in notices:notices.append(notice)
        (destination/'FONT-LICENSE.txt').write_text('\n\n'.join(notices)+'\n',encoding='utf-8')
assert 'OPEN FONT LICENSE' in (destination/'FONT-LICENSE.txt').read_text().upper()
print('Approved raylib native test assets prepared:',destination)
