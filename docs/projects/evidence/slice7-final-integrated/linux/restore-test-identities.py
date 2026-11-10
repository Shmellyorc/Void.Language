"""Restore exact on-disk provenance for the copied-source Linux test commands."""
from pathlib import Path
from hashlib import sha256
import shutil, subprocess

root=Path.cwd()
work=root/'build/slice7-final-integrated'
mirror=work/'src'
names=subprocess.check_output(['git','--no-optional-locks','ls-files','-z','--','docs/wiki']).split(b'\0')[:-1]
assert names
for encoded in names:
    name=encoded.decode()
    original=root/name
    copied=mirror/name
    copied.parent.mkdir(parents=True,exist_ok=True)
    assert not copied.exists()
    shutil.copy2(original,copied)
    print(sha256(copied.read_bytes()).hexdigest(),name)
for profile in ('release','debug','strict'):
    build=work/f'linux-{profile}'
    path=build/'CTestTestfile.cmake'
    original=build/'CTestTestfile-original.cmake'
    cases={line.split(']=]',1)[0]:line.replace(
                str(root/'tests/async/syntax_aot.py'),str(mirror/'tests/async/syntax_aot.py'))
           for line in original.read_text().splitlines()
           if line.startswith('add_test([=[async_syntax_aot_')}
    rewritten=[]
    changes=0
    for line in path.read_text().splitlines(keepends=True):
        key=line.split(']=]',1)[0]
        if key in cases:
            line=cases[key]+'\n'
            changes+=1
        rewritten.append(line)
    assert changes==len(cases)==10
    path.write_text(''.join(rewritten))
    print(profile,changes,'exact source paths for limited-stack object identity')
