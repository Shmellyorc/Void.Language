"""Targeted final-12 JIT/AOT/relocation and real callback lifecycle checks.

Arguments: voidc approved-catalog [reload-executable] [--audio]
The catalog is prepared/approved separately; this script downloads nothing.
"""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile

compiler,catalog=map(lambda p:Path(p).resolve(),sys.argv[1:3])
arguments=sys.argv[3:];audio='--audio' in arguments
reload=[Path(arg).resolve() for arg in arguments if arg!='--audio']
repo=Path(__file__).resolve().parents[2]
windows=os.name=='nt'
with tempfile.TemporaryDirectory(prefix='VOID raylib final callbacks ') as temporary:
    root=Path(temporary);project=root/'project'
    def run(*args,expected=0):
        result=subprocess.run(list(map(str,args)),capture_output=True,timeout=240,cwd=root)
        output=(result.stdout+result.stderr).decode(errors='replace')
        assert result.returncode==expected,(args,result.returncode,output)
        assert 'AddressSanitizer' not in output and 'UndefinedBehaviorSanitizer' not in output,output
        return output
    run(compiler,'new',project)
    native=project/'native';shutil.copytree(catalog,native)
    platform='windows' if windows else 'linux';library='void_raylib.dll' if windows else 'libvoid_raylib.so'
    manifest=project/'void.project.toml'
    with manifest.open('a',encoding='utf-8') as f:f.write(f'\n[native.{platform}]\ndescriptor="raylib-{platform}.descriptor"\nlibrary="{library}"\nadapter="{library}"\n')
    shutil.copyfile(repo/'verified-packages/raylib/src/api.void',project/'src/api.void')
    source=repo/'examples/raylib-consumer/src/callbacks.void'
    (project/'src/checks.void').write_bytes(source.read_bytes().replace(b'use pkg/raylib as rl',b'use "./api.void" as rl'))
    entry=next(p for p in (project/'src').glob('*.void') if p.name not in ('api.void','checks.void'))
    entry.write_bytes(b'''use "./checks.void" as checks
use os
func main() -> i32:
    var mode = ""
    if os.args() is Ok(var options):
        for option in options:
            mode = option
    if mode == "audio":
        return checks.audio()
    if mode == "implicit":
        return checks.implicit()
    if mode != "":
        return checks.failure(mode)
    return checks.headless()
''')
    run(compiler,'check','--project',project)
    for profile in ('debug','release'):
        assert 'RAYLIB final typed format/log and file callbacks passed' in run(compiler,'run','--project',project,'--allow-native','--profile',profile)
        assert 'implicit shutdown passed' in run(compiler,'run','--project',project,'--allow-native','--profile',profile,'--','implicit')
        for mode in ('failure','self-close','global-audio','double-close'):
            text=run(compiler,'run','--project',project,'--allow-native','--profile',profile,'--',mode,expected=1)
            assert 'runtime error' in text,text
        image=root/profile/('app.exe' if windows else 'app')
        run(compiler,'build','--project',project,'--allow-native','--profile',profile,'-o',image)
        shutil.copytree(native,image.parent/'native',dirs_exist_ok=True)
        moved=root/('relocated '+profile+' 雪');image.parent.rename(moved)
        assert 'file callbacks passed' in run(moved/image.name)
        assert 'implicit shutdown passed' in run(moved/image.name,'implicit')
        run(moved/image.name,'failure',expected=1)
        if audio:
            assert 'audio callback invocation/replacement/drain passed' in run(moved/image.name,'audio')
            assert 'implicit audio shutdown passed' in run(moved/image.name,'audio-implicit')
            assert 'mixer replacement/shutdown stress passed' in run(moved/image.name,'race')
            for mode in ('bad-size','forbidden-audio','slots','wrong-detach'):
                text=run(moved/image.name,mode,expected=1)
                assert 'runtime error' in text,text
    if reload:
        # Same source API and G5 code lease, exercised through compatible reload.
        reload_entry=project/'src/reload.void'
        reload_entry.write_bytes(b'''use "./api.void" as rl
var registration: rl.callback_registration? = null
struct capture:
    bias: i32
    last: i32
var saved: capture? = null
func (self capture) receive(level: i32, message: string):
    self.last = self.bias
func entry(value: i32) -> i32:
    if value == 0:
        var state = capture(bias=40, last=0)
        saved = state
        registration = rl.set_trace_log_callback(state.receive)
        return 1
    if value == -1:
        var status = rl.callback_close(registration as rl.callback_registration)
        registration = null
        return status
    rl.trace_log(rl.log_warning, "retained code")
    return (saved as capture).last
''')
        print(run(reload[0],reload_entry,native,library,library,native/f'raylib-{platform}.descriptor').strip())
print('Raylib final typed callbacks normal/O2 JIT/AOT, failure, replacement, implicit cleanup and relocation passed')
