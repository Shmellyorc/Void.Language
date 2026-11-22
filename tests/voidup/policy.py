"""Run selection and redirect policy against copied manager sources."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile

root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='voidup-policy-') as temporary:
    folder=Path(temporary)
    for p in (root/'apps/voidup').glob('*.void'):shutil.copyfile(p,folder/p.name)
    shutil.copyfile(Path(__file__).with_suffix('.void'),folder/'policy.void')
    def limits():
        import resource
        resource.setrlimit(resource.RLIMIT_STACK,(1048576,1048576))
    subprocess.run([str(Path(sys.argv[1]).resolve()),'run','--no-cache',str(folder/'policy.void')],check=True,preexec_fn=limits if os.name!='nt' else None)
