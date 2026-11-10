"""Collect dependency and cleanup receipts without changing the attested checkout."""
from hashlib import file_digest
from pathlib import Path
import json, os, platform, re, shutil, subprocess, sys

share=Path(r'\\vboxsvr\VM\slice7-final-integrated')
local=Path.home()/'VS7I2'
failed=Path.home()/'VS7I'
baseline=Path.home()/'VoidCI42377496702-Native'
sdk=baseline/'.toolchains/llvm-23.1.4'
pins=json.loads((share/'source-inputs.json').read_text())

def digest(path):
    with path.open('rb') as stream:
        return file_digest(stream,'sha256').hexdigest()

def save(name,data):
    (share/name).write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8')

assert sys.getwindowsversion().build==19045
assert digest(local/'source.zip')==(share/'source-archive.txt').read_text().split()[0]
assert all(digest(local/'src'/path)==expected for path,expected in pins.items())
tools={}
for path in (sdk/'bin/clang.exe',sdk/'bin/clang-cl.exe',sdk/'bin/lld-link.exe',
             sdk/'bin/llvm-lib.exe',sdk/'bin/llvm-readobj.exe',Path(r'C:\gcc\bin\cmake.exe'),
             Path(r'C:\gcc\bin\ninja.exe'),Path(sys.executable)):
    tools[str(path)]=digest(path)
profiles={}
for mode in ('Release','Debug'):
    build=local/mode
    text=(build/'CMakeCache.txt').read_text()
    data={}
    for key,filename in (('VOID_HOST_DEPS_ROOT','VOID-HOST-DEPENDENCIES.json'),
                         ('VOID_HTTP_DEPS_ROOT','VOID-HTTP-DEPENDENCIES.json')):
        prefix=Path(re.search(r'^'+key+r':[^=]+=(.+)$',text,re.M)[1].strip())
        path=prefix/filename
        content=json.loads(path.read_text())
        data[key]={'path':str(path),'receipt_sha256':digest(path),
                   'build_identity':content.get('build_identity'),'linked_payloads':content.get('files')}
    data['compiler']={'path':str(build/'voidc.exe'),'sha256':digest(build/'voidc.exe')}
    data['language_server']={'path':str(build/'voidls.exe'),'sha256':digest(build/'voidls.exe')}
    profiles[mode]=data
    shutil.copy2(build/'CMakeCache.txt',share/(mode+'-CMakeCache.txt'))
save('dependency-inputs.json',dict(
    platform=platform.platform(),sdk_identity=json.loads((sdk/'void-sdk-identity.json').read_text()),
    sdk_tools=tools,configurations=profiles,source_inputs_verified=len(pins),
    toolchain_environment={key:os.environ.get(key) for key in ('VCToolsVersion','WindowsSDKVersion','VCToolsInstallDir','WindowsSdkDir')}
))

assert failed.is_dir() and digest(failed/'source.zip')==digest(local/'source.zip')
junction=failed/'src/.toolchains'
assert junction.is_junction() and junction.resolve()==(baseline/'.toolchains').resolve()
os.rmdir(junction)  # Remove only the junction, never its target.
shutil.rmtree(failed)
assert not failed.exists()
assert digest(baseline/'tools/verify_host_deps.py')==pins['tools/verify_host_deps.py']
assert (local/'src/.toolchains').is_junction()
save('workspace-cleanup.json',dict(
    failed_configure_root_removed=str(failed), verified_absent=not failed.exists(),
    attested_toolchains_preserved=str(baseline/'.toolchains'), retained_root=str(local),
    retained_source_packet_sha256=digest(local/'source.zip'),
    free_bytes=shutil.disk_usage(local).free
))
