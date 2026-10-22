"""Installed and relocated compiler/LSP sources; standalone embedded zone rules."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile
cmake, build, configuration = sys.argv[1:]
environment=dict(os.environ)
environment.pop("VOID_STDLIB_ROOT",None)
checks=0
def execute(arguments,success=True,**kwargs):
    global checks
    result=subprocess.run([str(a) for a in arguments],capture_output=True,text=True,encoding="utf-8",env=environment,timeout=120,**kwargs)
    assert (result.returncode==0)==success,(arguments,result.returncode,result.stdout,result.stderr)
    checks+=1
    return result
with tempfile.TemporaryDirectory(prefix="VOID Stage10 time install ") as directory:
    root=Path(directory)
    original=root/"toolchain before move"
    for component in ("compiler","tooling"):
        execute([cmake,"--install",build,"--prefix",original,"--config",configuration,"--component",component])
    prefix=root/"toolchain relocated é"
    original.rename(prefix)
    extension=".exe" if os.name=="nt" else ""
    compiler=prefix/"bin"/("voidc"+extension)
    server=prefix/"bin"/("voidls"+extension)
    library=prefix/"share"/"void"/"stdlib"/"time.void"
    for name in ("LICENSE-Boost","LICENSE.txt","NOTICE","LICENSE","stage10-versions.json"):
        assert (prefix/"share"/"void"/"licenses"/name).is_file(),name
        checks+=1
    source=root/"application.void"
    source.write_text('''use time
func main() -> i32:
    if find_zone("Europe/Paris") is Ok(var selected):
        if make_instant(-2208988800, 0) is Ok(var point):
            if in_zone(point, selected) is Ok(var value):
                if "{value:zzzz VV}" == "+00:09:21 Europe/Paris":
                    return 0
    return 1
''',encoding="utf-8")
    assert execute([compiler,"check",source]).stdout.strip()=="Source checked successfully"
    backup=library.with_suffix(".source-backup")
    library.rename(backup)
    try:execute([compiler,"check",source],False)
    finally:backup.rename(library)
    execute([sys.executable,Path(__file__).with_name("lsp_display.py"),server])
    binary=root/("program"+extension)
    execute([compiler,"build",source,"-o",binary])
    isolated=root/"standalone empty directory"
    isolated.mkdir()
    standalone=isolated/binary.name
    binary.rename(standalone)
    environment.update(VOID_STDLIB_ROOT=str(root/"absent sources"),TZ="Void/Absent",TZDIR=str(root/"absent rules"))
    assert not execute([standalone],cwd=isolated).stdout
print(f"{checks} installed/relocated sources, LSP, notices and standalone zone checks passed")
