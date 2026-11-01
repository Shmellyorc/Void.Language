from pathlib import Path
import subprocess
root=Path.cwd();work=root/'.toolchains/ci41307437931-ubuntu';build=work/'build'
subprocess.run(['apt-get','install','-y','--no-install-recommends','ca-certificates'],check=True)
subprocess.run(['cmake','-S',str(root),'-B',str(build)],check=True)
subprocess.run(['cmake','--build',str(build),'--target','voidc','voidls','--parallel','3'],check=True)
subprocess.run(['python3','tests/net/http/install.py',str(build/'voidc'),'--evidence',str(work/'install-after')],check=True)
