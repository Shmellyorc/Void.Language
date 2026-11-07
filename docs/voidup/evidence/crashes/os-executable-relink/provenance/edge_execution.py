"""Real Linux deleted images, long owned environment values and cwd snapshot reuse."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import sys
import tempfile
import support as h
assert sys.platform=='linux'
compiler=Path(sys.argv[1]).resolve()
small='--small-stack' in sys.argv
with tempfile.TemporaryDirectory(prefix='VOID OS edge ') as directory:
    work=Path(directory);source=work/'main.void';images=[]
    source.write_text('''use os
use io
func main():
    if os.executable_path() is Ok(var image):
        var removed = io.remove_file(image)
        if os.executable_path() is Ok(var after):
            print(after == image + " (deleted)")
''')
    for flags in (['--no-opt'],[]):
        binary=work/('image '+str(bool(flags))+' (deleted)')
        h.run([compiler,'build',source,'-o',binary,*flags],work,small=small)
        images.append({'path':str(binary),'sha256':hashlib.sha256(binary.read_bytes()).hexdigest()})
        h.run([binary],work,b'true\n',sources=(source,),small=small)
        assert not binary.exists()
    (h.ROOT/'docs/os/evidence'/('deleted-image-'+compiler.parent.name+('-stack' if small else '')+'.json')).write_text(json.dumps(images,indent=2)+'\n')
    source.write_text('''use os
use path
func main():
    if os.env("VOID_OS_LONG") is Ok(var value):
        print(value.byte_length)
    if path.absolute("") is Ok(var first):
        if path.absolute("", "") is Ok(var second):
            if os.env("VOID_OS_EXPECT_CWD") is Ok(var expected):
                print(first == second and first == expected)
''')
    deep=work/'cwd 雪';deep.mkdir()
    env=dict(os.environ,VOID_OS_LONG='x'*100000,VOID_OS_EXPECT_CWD=str(deep)+os.sep)
    for flags in (['--no-opt'],[]):
        h.run([sys.executable,h.ROOT/'tests/os/exec_host.py',compiler,'run',source,*flags,'--no-cache'],deep,b'100000\ntrue\n',env=env,small=small)
        binary=work/('long'+str(bool(flags)))
        h.run([compiler,'build',source,'-o',binary,*flags],work,small=small)
        h.run([sys.executable,h.ROOT/'tests/os/exec_host.py',binary],deep,b'100000\ntrue\n',env=env,small=small,sources=(source,))
print(f'OS deleted image/long env/cwd: {h.checks} process checks passed; hard_stack_1MiB={small}')
