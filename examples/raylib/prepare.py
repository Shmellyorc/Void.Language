"""Compatibility entry for the canonical package-owned native preparation command."""
from pathlib import Path
import runpy
import shutil
import sys
import tempfile
canonical = Path(__file__).resolve().parents[2]/'verified-packages/raylib/prepare.py'
def has_option(name):
    return any(argument.partition('=')[0] == name for argument in sys.argv[1:])
if not has_option('--api-package'):
    sys.argv.append('--gallery-only')
if has_option('--managed-catalog'):
    runpy.run_path(str(canonical),run_name='__main__')
else:
    with tempfile.TemporaryDirectory(prefix='VOID raylib native preparation ') as temporary:
        catalog=Path(temporary)/'catalog'
        sys.argv += ['--managed-catalog',str(catalog)]
        runpy.run_path(str(canonical),run_name='__main__')
        destination=Path(__file__).resolve().parent/'native'
        for file in catalog.iterdir():
            shutil.copyfile(file,destination/file.name)
