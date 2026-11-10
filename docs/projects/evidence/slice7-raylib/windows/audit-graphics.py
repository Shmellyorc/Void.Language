"""Record the retained C: AOT packages and generated Windows binding."""
import hashlib
import json
from pathlib import Path
import shutil
import zipfile

share = Path(r'\\vboxsvr\VM\slice7-raylib')
local = Path.home() / 'VoidS7-Raylib-Graphics'
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
data = {}
for profile in ('Release', 'Debug'):
    work = local / (profile + '-psapi')
    assert not (work / 'gallery project').exists()
    assert not (work / 'compiler install').exists()
    native = work / 'archived project/native'
    assert not list(native.glob('.bind-*'))
    assert not any((work / 'native-link-temp').iterdir())
    data[profile] = {}
    for mode in ('normal', 'optimized'):
        package = work / ('relocated ' + mode + ' 雪')
        assert not (work / ('build-' + mode)).exists()
        expected = ['gallery.exe', 'native/RAYLIB-LICENSE.txt', 'native/raylib.dll', 'native/void_raylib.dll']
        files = sorted(p.relative_to(package).as_posix() for p in package.rglob('*') if p.is_file())
        assert files == expected, files
        run = json.loads((work / ('aot-' + mode) / 'result.json').read_text())
        identities = {name: digest(package / name) for name in files}
        for name, sha in run['sha256'].items():
            assert identities['native/' + name] == sha
        data[profile][mode] = dict(package=str(package), identities=identities)
    data[profile]['temporary_directories'] = 'No .bind-* directories. Private link temp empty.'
    if profile == 'Release':
        for name in ('raylib-windows.descriptor', 'raylib_checked.c'):
            shutil.copyfile(native / name, share / name)
with zipfile.ZipFile(local / 'gallery.zip') as packet:
    data['gallery_source'] = {name: hashlib.sha256(packet.read(name)).hexdigest() for name in packet.namelist()}
(share / 'graphics-packages.json').write_text(json.dumps(data, indent=2) + '\n')
