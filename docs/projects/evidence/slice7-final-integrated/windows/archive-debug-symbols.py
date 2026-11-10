"""Archive only this audit's generated Debug CLI PDBs before reclaiming guest space."""
from hashlib import file_digest
from pathlib import Path
import json, re, shutil, subprocess, sys, zipfile
share=Path(r'\\vboxsvr\VM\slice7-final-integrated')
archive_dir=Path(r'\\vboxsvr\VM\slice7-final-integrated-debug-symbols')
build=Path.home()/'VS7I2/Debug'
phase=sys.argv[1]
assert phase in ('initial','final')
ninja=Path(r'C:\gcc\bin\ninja.exe')
with (share/f'Debug-{phase}-ninja-query.txt').open('wb') as stream:
    subprocess.run([ninja,'-C',build,'-t','query','void_arrays_cli_values_normal.exe'],stdout=stream,
                   stderr=subprocess.STDOUT,check=True)

def digest(path):
    with path.open('rb') as stream:
        return file_digest(stream,'sha256').hexdigest()

candidates=sorted(p for p in build.glob('void_*_cli_*.pdb')
                  if re.fullmatch(r'void_\w+_cli_\w+_(?:normal|optimized)\.pdb',p.name))
assert candidates and all(p.is_file() and not p.is_symlink() and p.parent==build for p in candidates)
free=shutil.disk_usage(build).free
archive=archive_dir/f'Debug-cli-symbols-{phase}.zip'
manifest={}
with zipfile.ZipFile(archive,'x',compression=zipfile.ZIP_STORED,allowZip64=True) as output:
    for path in candidates:
        manifest[path.name]={'bytes':path.stat().st_size,'sha256':digest(path)}
        output.write(path,path.name)
with zipfile.ZipFile(archive) as package:
    assert set(package.namelist())==manifest.keys()
    for path in candidates:
        with package.open(path.name) as stream:
            assert file_digest(stream,'sha256').hexdigest()==manifest[path.name]['sha256']
        assert digest(path)==manifest[path.name]['sha256']
archive_sha=digest(archive)
for path in candidates:
    path.unlink()
    assert not path.exists()
receipt=dict(archive=archive.name, archive_sha256=archive_sha, files=manifest,
             source_directory=str(build), originals_verified_absent=True, free_before=free,
             free_after=shutil.disk_usage(build).free)
(share/f'Debug-symbols-{phase}.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(len(candidates),'verified PDBs',receipt['free_after']-free,'bytes reclaimed',flush=True)
