"""Reject type-erased buffer aliases before private native pointers can escape."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile

compiler,catalog=map(lambda p:Path(p).resolve(),sys.argv[1:])
with tempfile.TemporaryDirectory(prefix='VOID raylib buffer guards ') as temporary:
    root=Path(temporary)
    def run(*args):
        result=subprocess.run(list(map(str,args)),capture_output=True,timeout=180)
        assert result.returncode==0,(args,result.returncode,result.stdout,result.stderr)
        return result.stdout
    run(compiler,'new',root/'project')
    project=root/'project';native=project/'native'
    shutil.copytree(catalog,native)
    platform='windows' if os.name=='nt' else 'linux'
    library='void_raylib.dll' if os.name=='nt' else 'libvoid_raylib.so'
    manifest=project/'void.project.toml'
    with manifest.open('a',encoding='utf-8') as f:
        f.write(f'\n[native.{platform}]\ndescriptor="raylib-{platform}.descriptor"\nlibrary="{library}"\nadapter="{library}"\n')
    source=next((project/'src').glob('*.void'))
    shutil.copyfile(Path(__file__).resolve().parents[2]/'verified-packages/raylib/src/raylib_native.void',source.parent/'raylib_native.void')
    source.write_bytes(b'''use "./raylib_native.void" as native
func main() -> i32:
    if native.buffer_new(8, 30) != null or native.api_error() != 1:
        return 11
    var pointers = native.strings_new(1) as native.owner_buffer
    var bytes: [u8] = [65]
    if native.buffer_write(pointers, 0, bytes, 0, 1) != -1 or native.api_error() != 1:
        return 12
    if native.buffer_read(pointers, 0, bytes, 0, 1) != -1 or native.api_error() != 1:
        return 13
    var text = native.buffer_new(1, 1) as native.owner_buffer
    native.buffer_write(text, 0, bytes, 0, 1)
    if native.text_copy(pointers, text) != 0 or native.api_error() != 1:
        return 14
    native.compute_crc32(pointers, 1)
    if native.api_error() != 1:
        return 15
    native.buffer_close(text)
    native.buffer_close(pointers)
    print("Raylib private buffer type guards passed")
    return 0
''')
    run(compiler,'check','--project',project)
    for profile in ('debug','release'):
        assert b'Raylib private buffer type guards passed' in run(compiler,'run','--project',project,'--allow-native','--profile',profile)
print('Raylib private buffer alias and tag rejection passed')
