"""Focused VOIDUP release stages over the existing source-install CI recipe."""
import argparse
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
from install_smoke import Qualification, ROOT

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('phase',choices=['qualify','prepare'])
    p.add_argument('--version')
    p.add_argument('--public-config',type=Path)
    a=p.parse_args();q=Qualification()
    if a.phase=='qualify':
        evidence=q.evidence/'voidup-first-run'
        q.run('voidup-first-run',[sys.executable,'-B',ROOT/'tests/voidup/first_run.py','--build',q.build,'--sdk',q.sdk,'--licenses',q.prefix/'share/void/licenses','--evidence',evidence,*( ['--qualify-user-path'] if os.name=='nt' else [])])
        q.run('voidup-policy',[sys.executable,'-B',ROOT/'tests/voidup/policy.py',q.voidc])
        q.run('voidup-release-boundary',[sys.executable,'-B',ROOT/'tests/voidup/release.py'])
        q.run('voidup-production-boundary',[sys.executable,'-B',ROOT/'tests/voidup/production.py'])
        q.run('voidup-developer-environment',[sys.executable,'-B',ROOT/'tests/voidup/developer_environment.py',q.voidc])
        if platform.system()=='Linux':
            q.run('voidup-launcher-cancellation',[sys.executable,'-B',ROOT/'tests/voidup/launcher.py','--sdk',q.sdk])
            q.run('voidup-profile-access',[sys.executable,'-B',ROOT/'tests/voidup/profile.py','--sdk',q.sdk])
        if os.name=='nt':
            q.run('voidup-windows-launcher',[sys.executable,'-B',ROOT/'tests/voidup/windows_launcher.py','--sdk',q.sdk])
        q.save('voidup-qualification.json',{'status':'PASS','scope':'Authenticated engineering first install, compiler/LSP/native build and policy. Disposable Windows CI additionally exercises user registry PATH.','user_registry_path_qualified':os.name=='nt'})
        return
    sys.path.insert(0,str(ROOT/'tools'))
    from voidup_release import read_public_config
    config=read_public_config(a.public_config)
    if config['trust']!='production':raise ValueError('Production preparation requires approved public production roots')
    if json.loads((q.evidence/'voidup-qualification.json').read_text())['status']!='PASS':raise ValueError('Passing native first-install evidence is required')
    if platform.system() not in ('Linux','Windows'):raise ValueError('No qualified macOS complete payload recipe')
    if platform.system()=='Linux':
        floor=platform.freedesktop_os_release()
        if floor.get('ID')!='ubuntu' or floor.get('VERSION_ID')!='24.04':raise ValueError('Production Linux preparation must run on Ubuntu 24.04 userspace')
    if not a.version:raise ValueError('An explicit release version is required')
    host='windows-x86_64' if os.name=='nt' else 'linux-x86_64'
    release=q.work/'release';release.mkdir(mode=0o700)
    bootstrap=q.work/'bootstrap'
    q.run('voidup-bootstrap',[sys.executable,'-B',ROOT/'tools/build_voidup.py','--compiler',q.voidc,'--licenses',q.prefix/'share/void/licenses','--prefix',bootstrap,'--trust-config',a.public_config])
    name='voidup-'+host+('.zip' if os.name=='nt' else '.run')
    q.run('voidup-bootstrap-package',[sys.executable,'-B',ROOT/'tools/voidup_release.py','bootstrap','--prefix',bootstrap,'--host',host,'--production','--output',release/name])
    q.run('voidup-toolchain-package',[sys.executable,'-B',ROOT/'tools/voidup_release.py','package','--build',q.build,'--sdk',q.sdk,'--version',a.version,'--release-id','void-'+a.version+'-'+host,'--configuration','Release','--output',release])
    from package_voidup_fixtures import sha
    files=[p for p in release.rglob('*') if p.is_file() and 'producer' not in p.relative_to(release).parts]
    if len({p.name for p in files})!=len(files):raise ValueError('Prepared asset basename collision')
    q.save('voidup-production-prepared.json',{'status':'UNSIGNED; NOT PUBLISHED','version':a.version,'host':host,'source_sha':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'production_public_config_sha256':sha(a.public_config),'native_evidence':'voidup-qualification.json','windows_registry_path_qualified':os.name=='nt','assets':{p.name:{'bytes':p.stat().st_size,'sha256':sha(p)} for p in files}})

if __name__=='__main__':main()
