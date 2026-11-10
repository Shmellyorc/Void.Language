from hashlib import file_digest
from pathlib import Path
import csv
import json
import platform
import shutil
import subprocess
import sys
import traceback

share = Path(r'\\vboxsvr\VM\project-slice7-g4-windows')
home = Path.home().resolve(strict=True)
system = Path(home.anchor)
first = home/'VoidS7-G4W-architecture'
replay = home/'VoidS7-G4W-architecture-replay'
qualified = home/'VoidS7-G4W-architecture-final'
receipt = {'status':'PENDING','os':platform.platform(),'build':sys.getwindowsversion().build,
           'free_before':shutil.disk_usage(system).free,'deleted':[],'verified_retained':{}}

def digest(path):
    with path.open('rb') as raw:
        return file_digest(raw,'sha256').hexdigest()

def exact(path):
    assert path.resolve(strict=True)==path and not path.is_symlink() and not path.is_junction(),path

try:
    assert receipt['build']==19045
    final = json.loads((share/'native-results.json').read_text())
    assert final['status']=='PASS' and Path(final['work'])==qualified
    for name, expected in json.loads((share/'preserved-evidence.json').read_text()).items():
        assert digest(share/name)==expected,name
    tasks = subprocess.run(['tasklist.exe','/FO','CSV','/NH'],capture_output=True,text=True,check=True)
    running = {row[0].lower() for row in csv.reader(tasks.stdout.splitlines()) if row}
    assert not running.intersection({'launcher.exe','loader-probe.exe','process-default.exe','startup.exe',
                                     'clang-cl.exe','lld-link.exe','llvm-readobj.exe'}),running
    for root in (first,replay,qualified):
        exact(root)
    for name, expected in final['artifacts'].items():
        path = qualified/name
        assert path.is_relative_to(qualified) and '..' not in Path(name).parts,path
        exact(path)
        assert path.stat().st_nlink==1 and digest(path)==expected,path
        receipt['verified_retained'][str(path)] = expected
    targets = {first/'replay-launch.cmd':final['inputs']['native-run.cmd'],
               first/'replay-run.py':final['inputs']['native-run.py']}
    directories = []
    for label, owner in (('good','good'),('bad','alternate')):
        child = replay/f'child-{label}'
        exact(child)
        assert {p.name for p in child.iterdir()}=={'startup.exe','void_g4_probe_dep.dll'},child
        for name, reference in (('startup.exe',replay/'startup.exe'),
                               ('void_g4_probe_dep.dll',replay/owner/'void_g4_probe_dep.dll')):
            exact(reference)
            targets[child/name] = digest(reference)
        directories.append(child)
    for path, expected in targets.items():
        exact(path)
        assert path.is_file() and path.stat().st_nlink==1 and digest(path)==expected,path
    for path, expected in targets.items():
        assert digest(path)==expected,path
        path.unlink()
        receipt['deleted'].append(str(path))
    for path in directories:
        path.rmdir()
        receipt['deleted'].append(str(path))
    receipt['retained_roots'] = [str(root) for root in (first,replay,qualified)]
    receipt['status']='PASS'
except BaseException:
    receipt['status']='RETAINED_OR_PARTIAL'
    receipt['error']=traceback.format_exc()
finally:
    receipt['free_after']=shutil.disk_usage(system).free
    (share/'cleanup-results.json').write_text(json.dumps(receipt,indent=2)+'\n')
