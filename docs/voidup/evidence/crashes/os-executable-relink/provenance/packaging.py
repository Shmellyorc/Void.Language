"""Real macOS APPL bundles; a native run is required, with no sandbox certification implied."""
from pathlib import Path
import os
import plistlib
import shutil
import sys
import tempfile
import support as h
assert sys.platform=='darwin','real macOS is required'
compiler=Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='VOID OS bundles ') as directory:
    work=Path(directory);source=work/'main.void'
    source.write_text('''use os
func main():
    if os.is_bundle() is Ok(var bundled):
        print(bundled)
    else:
        print("classification error")
    if os.resource_dir() is Ok(var resources):
        if os.env("VOID_OS_EXPECT_RESOURCES") is Ok(var expected):
            print(resources == expected)
    else:
        print("resource error")
''')
    unrelated=work/'unrelated cwd';unrelated.mkdir()
    for flags in (['--no-opt'],[]):
        app=work/('Game 雪 # % '+str(bool(flags))+'.app')
        binary=app/'Contents/MacOS/game';binary.parent.mkdir(parents=True)
        resources=app/'Contents/Resources';resources.mkdir()
        info={'CFBundlePackageType':'APPL','CFBundleExecutable':'game','CFBundleIdentifier':'com.void.os.fixture','CFBundleName':'OS fixture','CFBundleVersion':'1'}
        (app/'Contents/Info.plist').write_bytes(plistlib.dumps(info))
        h.run([compiler,'build',source,'-o',binary,*flags],work)
        h.run([binary],unrelated,b'true\ntrue\n',env=dict(os.environ,VOID_OS_EXPECT_RESOURCES=str(resources)),sources=(source,))
        moved=app.with_name('Moved '+app.name);app.rename(moved)
        binary=moved/'Contents/MacOS/game';resources=moved/'Contents/Resources'
        h.run([binary],unrelated,b'true\ntrue\n',env=dict(os.environ,VOID_OS_EXPECT_RESOURCES=str(resources)),sources=(source,))
        plain=work/('plain'+str(bool(flags)));shutil.copy2(binary,plain)
        h.run([plain],unrelated,b'false\ntrue\n',env=dict(os.environ,VOID_OS_EXPECT_RESOURCES=str(plain.parent)),sources=(source,))
print(f'Real macOS app/plain/moved bundle: {h.checks} processes passed; signed App Sandbox remains unverified')
