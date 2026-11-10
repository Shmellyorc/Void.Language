"""Keep local qualification outputs and remove only this audit's transfer/scratch roots."""
from hashlib import file_digest, sha256
from pathlib import Path
import json, re, shutil, zipfile

root=Path.cwd()
work=root/'build/slice7-final-integrated'
evidence=root/'docs/projects/evidence/slice7-final-integrated'
share=Path.home()/'VM/slice7-final-integrated'

def digest(path):
    with path.open('rb') as stream:
        return file_digest(stream,'sha256').hexdigest()

def record(line):
    lines.append(line)
    print(line)

assert digest(work/'source.zip')==(evidence/'source-archive.txt').read_text().split()[0]
manifest=json.loads((evidence/'source-inputs.json').read_text())
with zipfile.ZipFile(work/'source.zip') as packet:
    assert set(packet.namelist())==manifest.keys()
    for name,expected in manifest.items():
        assert digest(root/name)==digest(work/'src'/name)==sha256(packet.read(name)).hexdigest()==expected,name
lines=[]
record(f'Source archive and {len(manifest)} input files verified against the checkout and Linux test mirror.')
for file in sorted(share.iterdir()):
    assert file.is_file() and not file.is_symlink(),file
    if file.name in ('source.zip','source-inputs.json','source-archive.txt','public-workflow.py'):
        other=work/'source.zip' if file.name=='source.zip' else evidence/file.name
        assert digest(other)==digest(file),file
        continue
    other=evidence/'windows'/file.name
    if not other.exists():
        shutil.copy2(file,other)
    assert digest(other)==digest(file),file
cleanup=json.loads((evidence/'windows/workspace-cleanup.json').read_text())
assert cleanup['verified_absent'] and cleanup['retained_source_packet_sha256']==digest(work/'source.zip')
for prefix in ('Release','Debug'):
    result=json.loads((evidence/'windows'/f'{prefix}-result.json').read_text())
    assert result['cleanup']['removed']
    public=json.loads((evidence/'windows/public-results.json').read_text())
    assert public[prefix]['removed']
    record(f'Windows {prefix} regression and public scratch verified absent by guest receipts.')
for retry_receipt in (evidence/'windows').glob('retry-results*.json'):
    for prefix,result in json.loads(retry_receipt.read_text()).items():
        if 'scratch' in result:
            assert result['removed']
            record(f'Windows {prefix} retry scratch verified absent by guest receipt.')
record('Windows failed configure workspace verified absent: '+cleanup['failed_configure_root_removed'])
record('Windows retained build, packet, installs and packages: '+cleanup['retained_root'])
shutil.rmtree(share)
assert not share.exists()
record('Windows host transfer workspace removed after hash-checked collection: '+str(share))

symbol_share=Path.home()/'VM/slice7-final-integrated-debug-symbols'
symbol_output=work/'windows-debug-fixture-symbols'
assert symbol_share.is_dir() and not symbol_output.exists()
for path in sorted(symbol_share.iterdir()):
    assert path.is_file() and not path.is_symlink()
    match=re.fullmatch(r'Debug-cli-symbols-(initial|final)\.zip',path.name)
    assert match,path
    receipt=json.loads((evidence/'windows'/f'Debug-symbols-{match[1]}.json').read_text())
    assert receipt['archive']==path.name and receipt['originals_verified_absent']
    assert digest(path)==receipt['archive_sha256']
shutil.move(symbol_share,symbol_output)
assert not symbol_share.exists() and symbol_output.is_dir()
record('Windows Debug CLI fixture symbols retained as verified archives: '+str(symbol_output))

remainder=work/'retained-temp-remnants'
for marker in sorted((evidence/'linux').rglob('*-temp.txt')):
    path=Path(marker.read_text().strip())
    assert path.parent==Path('/tmp') and re.fullmatch(r'void-s7-(integrated|public)-(release|debug|strict)\.[A-Za-z0-9]+',path.name),path
    if path.exists():
        assert not path.is_symlink()
        entries=list(path.iterdir())
        if entries:
            remainder.mkdir(exist_ok=True)
            destination=remainder/path.name
            assert not destination.exists()
            shutil.move(path,destination)
            record(f'Archived {len(entries)} remaining top-level scratch entries in {destination}.')
        else:
            path.rmdir()
    assert not path.exists()
    record('Linux scratch verified absent: '+str(path))
audit=Path((work/'audit-temp.txt').read_text().strip())
assert audit.parent==Path('/tmp') and audit.name.startswith('void-s7-layer-audit.') and not audit.is_symlink()
shutil.rmtree(audit)
assert not audit.exists()
record('Layer replay scratch verified absent: '+str(audit))
for mode in ('release','debug','strict'):
    disposable=work/f'first-linux-{mode}'
    assert not disposable.is_symlink()
    if disposable.exists():
        shutil.rmtree(disposable)
    assert not disposable.exists()
    record('Linux failed configure build verified absent: '+str(disposable))
record('Linux retained builds, public packages, source archives and the isolated CTest source/evidence tree: '+str(work))
record(f'Linux filesystem free bytes after cleanup: {shutil.disk_usage(work).free}')
(evidence/'workspace.txt').write_text('\n'.join(lines)+'\n')
