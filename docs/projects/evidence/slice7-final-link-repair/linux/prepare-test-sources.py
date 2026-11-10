"""Prepare per-profile test sources without modifying the prior source packet."""
from hashlib import sha256
from pathlib import Path
import json, shutil, subprocess, zipfile

root=Path.cwd()
work=root/'build/slice7-final-link-repair'
prior=root/'build/slice7-final-integrated'
prior_evidence=root/'docs/projects/evidence/slice7-final-integrated'
snapshot=work/'source'
opening=json.loads((work/'opening.json').read_text())
changed=set(opening['repair_paths'])-{'docs/projects/SLICE7_FINAL_CLOSEOUT.md'}
archive=prior/'source.zip'
assert sha256(archive.read_bytes()).hexdigest()==(prior_evidence/'source-archive.txt').read_text().split()[0]
manifest=json.loads((prior_evidence/'source-inputs.json').read_text())
snapshot.mkdir()
with zipfile.ZipFile(archive) as packet:
    assert set(packet.namelist())==manifest.keys()
    for name,expected in manifest.items():
        path=Path(name)
        assert not path.is_absolute() and '..' not in path.parts
        accepted=packet.read(name)
        assert sha256(accepted).hexdigest()==expected,name
        current=(root/path).read_bytes()
        assert (current!=accepted)==(name in changed),name
        output=snapshot/path
        output.parent.mkdir(parents=True,exist_ok=True)
        output.write_bytes(current)
        manifest[name]=sha256(current).hexdigest()
assert changed<=manifest.keys()

extras=subprocess.check_output(['git','--no-optional-locks','ls-files','-z','--','.github','docs/wiki']).split(b'\0')[:-1]
for encoded in extras:
    path=Path(encoded.decode())
    output=snapshot/path
    output.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(root/path,output)
    manifest[str(path)]=sha256(output.read_bytes()).hexdigest()
for name in ('docs/json/evidence/implementation','docs/math/evidence','docs/text-encoding/evidence',
             'docs/random/evidence','docs/os/evidence','docs/log/evidence','docs/io/evidence'):
    (snapshot/name).mkdir(parents=True,exist_ok=True)
(snapshot/'.toolchains').symlink_to(root/'.toolchains',target_is_directory=True)
for mode in ('release','debug','strict'):
    subprocess.run(['cp','-a','--reflink=always',snapshot,work/f'test-source-{mode}'],check=True)
print(json.dumps(dict(prior_archive_sha256=sha256(archive.read_bytes()).hexdigest(),
    original_entries=3440, test_sources=manifest, edited_entries=sorted(changed)),indent=2,sort_keys=True))
