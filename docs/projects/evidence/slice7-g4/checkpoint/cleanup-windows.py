from hashlib import file_digest
from pathlib import Path
import csv
import json
import platform
import shutil
import subprocess
import sys
import traceback

share = Path(r'\\vboxsvr\VM\project-slice7-g4-checkpoint')
home = Path.home().resolve(strict=True)
system = Path(home.anchor)
work = home/'VoidS7-G4-checkpoint'
replacement = work/'replacement'
disposable = replacement/'void_g4_probe_dep.dll'
equivalent = work/'bad'/'void_g4_probe_dep.dll'
report = {'platform':platform.platform(), 'build':sys.getwindowsversion().build,
          'free_before':shutil.disk_usage(system).free, 'deleted':[], 'retained':{}}

def digest(path):
    with path.open('rb') as file:
        return file_digest(file, 'sha256').hexdigest()

try:
    assert report['build']==19045
    for name, expected in json.loads((share/'preserved-evidence.json').read_text()).items():
        assert digest(share/name)==expected, name
    tasks = subprocess.run(['tasklist.exe','/FO','CSV','/NH'],capture_output=True,text=True,check=True)
    running = {row[0].lower() for row in csv.reader(tasks.stdout.splitlines()) if row}
    assert not running.intersection({'probe.exe','bare.exe','manifested.exe','activation.exe','edge.exe',
                                     'edge-process-default.exe','clang-cl.exe','lld-link.exe'}), running
    for path in (work,replacement,disposable,equivalent):
        assert path.resolve(strict=True)==path and not path.is_symlink() and not path.is_junction(), path
    assert list(replacement.iterdir()) == [disposable]
    assert disposable.stat().st_nlink==1 and digest(disposable)==digest(equivalent)
    for name in ('probe.exe', 'dep.lib', 'good/void_g4_probe_dep.dll', 'good/void_g4_probe_main.dll',
                 'bad/void_g4_probe_dep.dll', 'redirection/bare.exe', 'redirection/manifested.exe',
                 'redirection/activation.exe', 'redirection/edge.exe', 'redirection/edge-process-default.exe',
                 'redirection/void_g4_probe_dep.dll', 'redirection/void_g4_probe_main.dll'):
        path = work/name
        assert path.resolve(strict=True)==path and path.stat().st_nlink==1, path
        report['retained'][str(path)] = digest(path)
    report['removed_duplicate_sha256'] = digest(disposable)
    disposable.unlink()
    report['deleted'].append(str(disposable))
    replacement.rmdir()
    report['deleted'].append(str(replacement))
    report['status'] = 'PASS'
except BaseException:
    report['status'] = 'RETAINED_OR_PARTIAL'
    report['error'] = traceback.format_exc()
finally:
    report['free_after'] = shutil.disk_usage(system).free
    (share/'cleanup-results.json').write_text(json.dumps(report,indent=2)+'\n')
