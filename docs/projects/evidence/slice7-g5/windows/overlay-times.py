from pathlib import Path
import hashlib
import json

share=Path(r'\\vboxsvr\VM\slice7-g5')
source=Path.home()/'VoidCI42377496702-Native'
overlay=json.loads((share/'compiler/overlay.json').read_text())
assert overlay['status']=='active'
packet=json.loads((share/'compiler/hashes.json').read_text())
files={}
for name in overlay['originals']:
    path=source/name
    digest=hashlib.sha256(path.read_bytes()).hexdigest()
    assert digest==packet[name],name
    files[name]={'sha256':digest,'mtime_ns':path.stat().st_mtime_ns}
assert json.loads((share/'compiler/overlay.json').read_text())['status']=='active'
(share/'compiler-final/reusable-input-times.json').write_text(json.dumps(files,indent=2)+'\n')
