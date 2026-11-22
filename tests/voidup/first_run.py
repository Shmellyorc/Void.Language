"""Real authenticated first install through the native bootstrap, never a user prefix."""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time
import shutil
import zipfile
import atexit
import struct
from contextlib import nullcontext

from support import ROOT, Service, encoded, sign
from package_voidup_fixtures import produce

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--build',type=Path,required=True)
p.add_argument('--sdk',type=Path,required=True)
p.add_argument('--keys',type=Path,help='Existing local engineering key directory; otherwise use ephemeral in-memory CI roots')
p.add_argument('--licenses',type=Path,required=True)
p.add_argument('--evidence',type=Path,required=True)
p.add_argument('--no-evidence',action='store_true',help='Emit only a generic result; retain no guest details or logs')
p.add_argument('--qualify-user-path',action='store_true',help='Disposable Windows CI user only: exercise and restore HKCU PATH')
p.add_argument('--reuse-work',type=Path,help='New caller-owned directory retained for developer/editor acceptance')
p.add_argument('--installation-root',type=Path,help='Native Windows only: new disposable local root when fixture archives use a shared drive')
a=p.parse_args();a.evidence.mkdir(mode=0o700,exist_ok=True)
assert sys.platform in ('linux','win32','darwin'),'Use a supported native platform'
macos=sys.platform=='darwin'
windows=os.name=='nt';suffix='.exe' if windows else ''
if a.installation_root and (not windows or a.installation_root.exists()):p.error('--installation-root must be a new disposable native Windows root')
if a.qualify_user_path and (not windows or not os.environ.get('CI')):p.error('--qualify-user-path requires a disposable Windows CI user')
registry_before=None
if a.qualify_user_path:
    import winreg
    with winreg.CreateKey(winreg.HKEY_CURRENT_USER,'Environment') as key:
        try:registry_before=winreg.QueryValueEx(key,'Path')
        except FileNotFoundError:registry_before=None
    def restore_registry():
        with winreg.CreateKey(winreg.HKEY_CURRENT_USER,'Environment') as key:
            if registry_before is None:
                try:winreg.DeleteValue(key,'Path')
                except FileNotFoundError:pass
            else:winreg.SetValueEx(key,'Path',0,registry_before[1],registry_before[0])
    atexit.register(restore_registry)
rows=[]
profile_acl=None

def stack():
    import resource
    resource.setrlimit(resource.RLIMIT_STACK,(1048576,1048576))
def run(argv,*,env=None,cwd=None,expected=0,timeout=900,data=None):
    result=subprocess.run(list(map(str,argv)),cwd=cwd or ROOT,env=env,input=data,capture_output=True,timeout=timeout,preexec_fn=None if windows else stack)
    assert result.returncode==expected,(argv,result.returncode,result.stdout[-2000:],result.stderr[-3000:])
    assert not any(x in result.stderr for x in [b'AddressSanitizer',b'UndefinedBehaviorSanitizer',b'LeakSanitizer'])
    rows.append({'case':str(argv[0]),'exit':result.returncode,'stdout':result.stdout.decode(errors='replace'),'stderr':result.stderr.decode(errors='replace')})
    return result

if a.reuse_work:a.reuse_work.mkdir(mode=0o700,parents=True,exist_ok=False)
with (nullcontext(str(a.reuse_work.resolve())) if a.reuse_work else tempfile.TemporaryDirectory(prefix='voidup-first-run-')) as temporary:
    work=Path(temporary);fixtures=work/'fixtures';fixtures.mkdir(mode=0o700)
    cases=a.installation_root.parent if a.installation_root else work
    # Existing private engineering keys stay in their original private location.
    # The fixture retains a link only; private bytes are never copied to evidence.
    signer=None
    if a.keys:
        (fixtures/'keys').symlink_to(a.keys.resolve(),target_is_directory=True)
        keys=re.findall(r'"([0-9a-f]{64})"',(ROOT/'apps/voidup/trust_keys.void').read_text())
    else:
        from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
        from cryptography.hazmat.primitives.serialization import Encoding,PublicFormat
        private=[Ed25519PrivateKey.generate() for _ in range(3)]
        keys=[k.public_key().public_bytes(Encoding.Raw,PublicFormat.Raw).hex() for k in private]
        def signer(raw,signers):
            return encoded({'schema':1,'signatures':[{'key_id':f'test-{n}','signature':private[n-1].sign(b'VOIDUP CATALOG v1\n'+raw).hex()} for n in signers]})
    release=produce(a.build.resolve(),'stable-current',fixtures,a.sdk.resolve(),host='windows-x86_64' if windows else 'macos-aarch64-14.0' if macos else 'linux-x86_64-ubuntu24.04')
    release.update(selection={'channel':'stable','version':'1.10.0'})
    older=dict(release,id='zzz-older',selection={'channel':'stable','version':'1.9.0'})
    wrong_host=dict(release,id='other-host',host='linux-x86_64-ubuntu24.04' if windows else 'windows-x86_64',selection={'channel':'stable','version':'99.0.0'})
    preview=dict(release,id='preview',selection={'channel':'preview','version':'99.0.0'})
    now=int(time.time());catalog=dict(schema=1,domain='voidup-catalog-v1',serial=100,issued=now-60,expires=now+86400,min_manager=2,llvm_version='23.1.4',revoked=[],releases=[older,wrong_host,release,preview])
    with Service(fixtures,signer=signer) as service:
        service.catalog(catalog)
        config=work/'public-test-config.json'
        config.write_bytes(encoded({'schema':1,'trust':'engineering','approval':'isolated G96 qualification only','catalog':service.url,'keys':[{'id':f'test-{n+1}','public':k} for n,k in enumerate(keys)]}))
        bootstrap=work/'downloaded-bootstrap'
        # Producer tools run before the consumer environment is made Python-free.
        run([sys.executable,'-B',ROOT/'tools/build_voidup.py','--compiler',a.build.resolve()/('voidc'+suffix),'--prefix',bootstrap,'--licenses',a.licenses.resolve(),'--trust-config',config])
        receipt=json.loads((bootstrap/'build.json').read_text())
        installer=work/('download.zip' if windows or macos else 'download.run')
        run([sys.executable,'-B',ROOT/'tools/voidup_release.py','bootstrap','--prefix',bootstrap,'--host','windows-x86_64' if windows else 'macos-aarch64' if macos else 'linux-x86_64','--output',installer])
        download_metadata=json.loads(Path(str(installer)+'.json').read_text())
        if macos:
            download=work/'extracted-download'
            run(['/usr/bin/ditto','-x','-k',installer,download])
        elif windows:
            download=work/'extracted-download'
            with zipfile.ZipFile(installer) as archive:archive.extractall(download)
        else:
            download=bootstrap
        home=work/"first user 雪 ' $() # %";home.mkdir(mode=0o775)
        if not windows:
            home.chmod(0o775)
            (home/'.bashrc').write_text('# existing user setting\nexport G96_PRESERVED=yes\n');(home/'.bashrc').chmod(0o664)
            if sys.platform=='linux':
                acl=struct.pack('<I',2)+b''.join(struct.pack('<HHI',*e) for e in
                    [(1,6,0xffffffff),(4,6,0xffffffff),(8,4,65534),(16,6,0xffffffff),(32,4,0xffffffff)])
                try:
                    os.setxattr(home/'.bashrc','system.posix_acl_access',acl)
                    profile_acl=os.getxattr(home/'.bashrc','system.posix_acl_access')
                except OSError as error:
                    if error.errno!=95:raise
        minimal=work/'os-utilities';minimal.mkdir()
        if not windows:
            for tool in ['mktemp','tail','tar','gzip','rm']:
                (minimal/tool).symlink_to(shutil.which(tool))
        environment=dict(os.environ,HOME=str(home),XDG_DATA_HOME=str(home/'data'),PATH=str(minimal))
        environment.pop('VOID_STDLIB_ROOT',None);environment.pop('VOID_NATIVE_CATALOG',None)
        init=download/'bin'/('voidup-init'+suffix)
        def initializer(path):return [path] if windows else ['/bin/sh',path.parent.parent/'Install VOID.command'] if macos else ['/bin/sh',installer]
        install=a.installation_root.resolve() if a.installation_root else work/"private user 雪 ' $() # %" if windows else home/'Library/Application Support/voidup' if macos else home/'data/voidup'
        initial_options=['--root',install,*([] if a.qualify_user_path else ['--no-path'])] if windows else []
        run([*initializer(init),*initial_options,'--ca-file',service.ca],env=environment)
        control=json.loads((install/'control.json').read_text())
        assert control['current']==release['id'] and control['serial']==100
        assert not any('zzz-older.zip' in request or 'preview.zip' in request for request in service.requests)
        if not windows:assert (home/'.bashrc').read_text().startswith('# existing user setting\nexport G96_PRESERVED=yes\n')
        if profile_acl is not None:assert os.getxattr(home/'.bashrc','system.posix_acl_access')==profile_acl
        # A new login terminal discovers stable proxies, with no Python, CMake or
        # ambient LLVM in PATH. Move the downloaded package away first: retained
        # manager/resources, not the Downloads folder, must service these tools.
        moved=work/'download-no-longer-used';download.rename(moved)
        if windows:
            if a.qualify_user_path:
                with winreg.OpenKey(winreg.HKEY_CURRENT_USER,'Environment') as key:registered=winreg.QueryValueEx(key,'Path')[0]
                assert str(install/'bin').lower() in [x.lower() for x in registered.split(';')]
            terminal=run([Path(os.environ['SYSTEMROOT'])/'System32/cmd.exe','/d','/c','voidc --version'],env=environment|{'PATH':str(install/'bin')})
        else:
            terminal=run(['/bin/bash','--noprofile','--norc','-c','. "$HOME/.profile"; command -v voidc; command -v python3 && exit 91; voidc --version'],env=environment)
            assert str(install/'bin/voidc').encode() in terminal.stdout
        assert release['build_identity'].encode() in terminal.stdout
        compiler_proxy=install/'bin'/('voidc'+suffix);manager_proxy=install/'bin'/('voidup'+suffix)
        if not windows:
            # The installer and verifier must accept ordinary shared storage,
            # without chmod instructions or a private-home assumption.
            for member in install.rglob('*'):
                if member.is_dir():member.chmod(0o775)
                elif member.is_file():member.chmod(0o775 if member.stat().st_mode & 0o111 else 0o664)
            install.chmod(0o775)
            run([manager_proxy,'verify',release['id']],env=environment)
        project=cases/'hello';run([compiler_proxy,'new',project],env=environment)
        if not windows:project.chmod(0o775)
        run([compiler_proxy,'run','.'],env=environment,cwd=project)
        run([compiler_proxy,'build','.'],env=environment,cwd=project)
        binary=project/'.void/build'/('main'+suffix)
        if not binary.exists():binary=project/'.void/build'/('app'+suffix)
        run([binary],env=environment,cwd=project)
        # A real LSP handshake through the persistent native proxy.
        from ci.install_smoke import frames,unframe
        wire=frames([{'jsonrpc':'2.0','id':1,'method':'initialize','params':{'capabilities':{}}},{'jsonrpc':'2.0','method':'initialized','params':{}},{'jsonrpc':'2.0','id':2,'method':'shutdown'},{'jsonrpc':'2.0','method':'exit'}])
        lsp=run([install/'bin'/('voidls'+suffix),'--project',project],env=environment,data=wire)
        assert any(x.get('id')==1 and x.get('result',{}).get('serverInfo',{}).get('name')=='VOID' for x in unframe(lsp.stdout))
        if sys.platform=='linux':
            # Exercise the accepted F5 consent protocol through the new proxy,
            # including its XDG approval store, phase seal and CI refusal.
            run([sys.executable,'-B',ROOT/'tests/project/native_stdio_approval.py',compiler_proxy],env=environment)
        profile_before={p.name:p.read_bytes() for p in home.iterdir() if p.is_file()}
        # Idempotent setup, then authenticated update using the retained manager.
        reinit=moved/'bin'/('voidup-init'+suffix)
        run([*initializer(reinit),*initial_options,'--ca-file',service.ca],env=environment)
        assert profile_before=={p.name:p.read_bytes() for p in home.iterdir() if p.is_file()}
        if not windows:
            assert (home/'.bashrc').stat().st_mode & 0o777 == 0o664
            assert install.stat().st_mode & 0o777 == 0o775
            if profile_acl is not None:assert os.getxattr(home/'.bashrc','system.posix_acl_access')==profile_acl
        # Roll out a genuinely different authenticated release identity/payload.
        # It intentionally reuses this compiler build; this tests the manager's
        # version transaction, rather than claiming a second compiler build.
        next_release=produce(a.build.resolve(),'stable-next',fixtures,a.sdk.resolve(),host=release['host'])
        next_release.update(selection={'channel':'stable','version':'1.11.0'})
        newer=copy.deepcopy(catalog);newer['serial']=101;newer['releases'].append(next_release);service.catalog(newer)
        sentinel=project/'user-notes.txt';sentinel.write_text('Preserve developer files\n')
        service.stall='/packages/stable-next.zip';service.stall_after=1048576
        owner=subprocess.Popen(list(map(str,[manager_proxy,'--ca-file',service.ca,'update'])),env=environment,stdout=subprocess.PIPE,stderr=subprocess.PIPE,preexec_fn=None if windows else stack)
        try:
            assert service.started.wait(60),'Update never reached the controlled partial transfer'
            run([manager_proxy,'doctor'],env=environment,expected=1)
            owner.terminate();out,err=owner.communicate(timeout=30)
            rows.append({'case':'interrupted actual 1 MiB stable-next update','exit':owner.returncode,'stdout':out.decode(errors='replace'),'stderr':err.decode(errors='replace')})
            assert owner.returncode!=0 and json.loads((install/'control.json').read_text())['current']==release['id']
            abandoned={p.name for p in (install/'transactions').iterdir()};assert abandoned
        finally:
            service.resume.set();service.stall=None
            if owner.poll() is None:owner.kill();owner.communicate(timeout=30)
        run([manager_proxy,'--ca-file',service.ca,'update'],env=environment)
        state=json.loads((install/'control.json').read_text());assert state['current']==next_release['id'] and state['previous']==release['id'] and state['serial']==101
        assert abandoned<={p.name for p in (install/'transactions').iterdir()}
        run([manager_proxy,'rollback'],env=environment)
        assert json.loads((install/'control.json').read_text())['current']==release['id']
        run([compiler_proxy,'run','.'],env=environment,cwd=project)
        run([manager_proxy,'--ca-file',service.ca,'update'],env=environment)
        run([manager_proxy,'--ca-file',service.ca,'install',next_release['id'],'--activate'],env=environment)
        run([manager_proxy,'doctor'],env=environment)
        assert json.loads((install/'control.json').read_text())['current']==next_release['id']
        assert sentinel.read_text()=='Preserve developer files\n'
        # An unauthenticated first run must not publish tools or touch profiles.
        service.catalog(catalog,signers=(1,))
        rejected=cases/'bad-signature-root'
        run([*initializer(reinit),'--root',rejected,'--no-path','--ca-file',service.ca],env=environment,expected=1)
        assert not list((rejected/'toolchains').iterdir()) and not (rejected/'bin').exists()
        assert profile_before=={p.name:p.read_bytes() for p in home.iterdir() if p.is_file()}
        # New commands still use the existing authenticated freshness floor.
        service.catalog(catalog)
        run([manager_proxy,'--ca-file',service.ca,'update'],env=environment,expected=1)
        assert json.loads((install/'control.json').read_text())['serial']==101
        ambiguous=copy.deepcopy(newer);ambiguous['serial']=102;ambiguous['releases'].append(dict(release,id='ambiguous'))
        service.catalog(ambiguous)
        run([*initializer(reinit),'--root',cases/'ambiguous-root','--no-path','--ca-file',service.ca],env=environment,expected=1)
        absent=copy.deepcopy(newer);absent['serial']=103;absent['releases']=[preview,wrong_host]
        service.catalog(absent)
        run([*initializer(reinit),'--root',cases/'unsupported-root','--no-path','--ca-file',service.ca],env=environment,expected=1)
        # Unsafe shell profiles remain unchanged; native installation can be
        # retried with --no-path and does not weaken root/inventory admission.
        service.catalog(newer)
        if not windows:
            unsafe=work/'unsafe-home';unsafe.mkdir(mode=0o700);sentinel=work/'sentinel';sentinel.write_text('preserve me\n');(unsafe/'.profile').symlink_to(sentinel)
            unsafe_env=environment|{'HOME':str(unsafe),'XDG_DATA_HOME':str(unsafe/'data')}
            run([*initializer(reinit),'--ca-file',service.ca],env=unsafe_env,expected=1)
            assert sentinel.read_text()=='preserve me\n' and (unsafe/'.profile').is_symlink()
            run([*initializer(reinit),'--ca-file',service.ca,'--no-path'],env=unsafe_env)
            assert sentinel.read_text()=='preserve me\n'
        # Retained schema-1 catalogs have no selection object. Their exact signed
        # bytes must still install/verify/run explicitly, but never gain a guessed
        # automatic version merely because the release ID looks like one.
        legacy_release={k:v for k,v in release.items() if k!='selection'}
        legacy=dict(catalog,serial=90,min_manager=1,releases=[legacy_release]);service.catalog(legacy)
        native_manager=moved/'bin'/('voidup'+suffix);legacy_root=cases/'legacy-root'
        legacy_options=[native_manager,'--root',legacy_root,'--ca-file',service.ca]
        run([*legacy_options,'setup'],env=environment,expected=1)
        run([*legacy_options,'install',release['id'],'--activate'],env=environment)
        run([*legacy_options,'verify',release['id']],env=environment)
        run([*legacy_options,'run','default','voidc','--','--version'],env=environment)
        # Public distribution can never enroll the engineering roots.
        from voidup_release import read_public_config
        invalid=json.loads(config.read_text());invalid.update(trust='production',catalog='https://github.com/Shmellyorc/Void.Language/releases/download/voidup-stable/catalog.json');bad_config=work/'invalid-production.json';bad_config.write_bytes(encoded(invalid))
        try:read_public_config(bad_config)
        except ValueError:pass
        else:raise AssertionError('engineering roots admitted as production trust')
        summary={'status':'PASS','platform':'Windows x86-64; disposable CI user registry PATH' if a.qualify_user_path else 'Windows x86-64; explicit test root, user registry PATH untouched' if windows else 'macOS ARM64; actual producer OS only, no minimum-OS/Gatekeeper claim' if macos else 'Linux x86-64; actual producer userspace, kernel recorded separately','consumer_cases':len(rows)-2,'hard_soft_stack_bytes':1048576,'automatic_selection':'1.10.0 over 1.9.0, other-host and preview excluded','installed_compiler_identity':release['build_identity'],'python_cmake_llvm_absent_from_consumer_path':True,'new_terminal_discovery':True,'download_folder_independence':True,'lsp_handshake':True,'native_build_and_run':True,'signature_replay_ambiguity_missing_stable_refusal':True,'safe_profile_preservation':not windows,'user_registry_path_qualified':a.qualify_user_path,'engineering_production_root_separation':True,'bootstrap_files':receipt['files'],'bootstrap_file_bytes':sum(x['bytes'] for x in receipt['files'].values()),'requests':len(service.requests)}
        summary.update(download_bytes=download_metadata['archive_bytes'],download_expanded_bytes=download_metadata['expanded_bytes'],download_dependencies=download_metadata['dependencies'],native_dependency_audit=receipt['native_dependency_audit'],bootstrap_id=receipt['bootstrap_id'],compiled_manager_source_sha256=receipt['compiled_source_sha256'],producer_source_sha256=receipt['source_sha256'],profile_named_acl_preserved=profile_acl is not None,proxy_editor_consent_qualified=sys.platform=='linux',toolchain_archive_bytes=release['artifact_bytes'],toolchain_expanded_bytes=json.loads((fixtures/'www/inventories/stable-current.json').read_bytes())['expanded_bytes'],host=__import__('platform').uname()._asdict(),os_release=__import__('platform').freedesktop_os_release() if sys.platform=='linux' else None)
        summary.update(distinct_stable_release_update='1.10.0 -> 1.11.0; same compiler build, different release/payload identities',rollback_reinstall_recovery=True,interrupted_download_bytes=1048576,abandoned_transactions_preserved=True,user_files_preserved=True)
        if not a.no_evidence:(a.evidence/'first-run-summary.json').write_text(json.dumps(summary,indent=2)+'\n')
        # Retain selected outputs and exact commands with ephemeral paths made
        # explicit. Private keys and disposable packages are never archived.
        if not a.no_evidence:(a.evidence/'first-run-cases.json').write_text(json.dumps(rows,indent=2)+'\n')
print('Authenticated native first install, terminal discovery, retained manager, compiler/LSP, update and refusal cases passed')
