from pathlib import Path
import hashlib,json
source=Path.home()/'VoidCI42377496702-Native'
out=Path(r'\\vboxsvr\VM\project-slice6')
active=json.loads((out/'native-results.json').read_text())
assert active['status']=='running' and active.get('source_inputs_sha256')
result={}
for name,digest in active['source_inputs_sha256'].items():
    path=source/name
    before=path.stat()
    with path.open('rb') as stream: observed=hashlib.file_digest(stream,'sha256').hexdigest()
    after=path.stat()
    assert before.st_mtime_ns==after.st_mtime_ns and observed==digest,name
    result[name]={'sha256':digest,'mtime_ns':after.st_mtime_ns}
(out/'first-source-times.json').write_text(json.dumps(result,indent=2)+'\n')
