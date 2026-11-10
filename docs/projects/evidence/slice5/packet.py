from pathlib import Path
import hashlib,json,subprocess,zipfile,sys
root=Path.cwd()
out=Path(sys.argv[1])
prior=Path('/home/shmellyorc/VM/project-slice4/source-packet-sha256.json')
names=set(json.loads(prior.read_text()))
for p in ('src/frontend/project_creation.cpp','include/void/project_creation.hpp','tests/project/creation.cpp','tests/project/creation.py'):
    names.add(p)
hashes={}
with zipfile.ZipFile(out/'source-packet.zip','w',zipfile.ZIP_DEFLATED,compresslevel=5) as archive:
    for name in sorted(names):
        path=root/name
        if not path.is_file(): raise FileNotFoundError(name)
        data=path.read_bytes()
        hashes[name]=hashlib.sha256(data).hexdigest()
        archive.writestr(name,data)
(out/'source-packet-sha256.json').write_text(json.dumps(hashes,indent=2)+'\n')
print(f'Packet: {len(hashes)} files')
