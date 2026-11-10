"""Bind test source operands to a disposable copy, preserving all test cases."""
from pathlib import Path
import re, shutil, subprocess

root=Path.cwd()
work=root/'build/slice7-final-integrated'
mirror=work/'src'
workflow_paths=subprocess.check_output(['git','--no-optional-locks','ls-files','-z','--','.github']).split(b'\0')[:-1]
for name in workflow_paths:
    source=root/name.decode()
    dest=mirror/name.decode()
    dest.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(source,dest)

pattern=re.compile(re.escape(root.as_posix())+r'(?=/(?!build/|\.toolchains/)|")')
# The existing host-receipt test reads the absolute SDK identity from its own
# source root and makes all negative fixtures in a TemporaryDirectory.
attested_test='add_test([=[host_capabilities_dependencies]=]'
for profile in ('release','debug','strict'):
    directory=work/('linux-'+profile)
    ctest=directory/'CTestTestfile.cmake'
    before=ctest.read_text()
    after='\n'.join(line if line.startswith(attested_test) else pattern.sub(mirror.as_posix(),line)
                    for line in before.split('\n'))
    assert before!=after
    assert re.findall(r'^add_test\(\[=\[(.*?)\]=\]',before,re.M)==re.findall(r'^add_test\(\[=\[(.*?)\]=\]',after,re.M)
    backup=directory/'CTestTestfile-original.cmake'
    assert not backup.exists()
    shutil.copyfile(ctest,backup)
    ctest.write_text(after)
    print(profile, sum(line.startswith('add_test(') for line in after.splitlines()),
          'test commands, original retained at',backup)
