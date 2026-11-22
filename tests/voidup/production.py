"""Offline production-boundary regressions using in-memory engineering roots.

No production enrollment, private PEMs, network requests or publication. The
production-only branch is selected through a scoped mock, so no reusable public
production configuration is created even temporarily.
"""
import argparse
from contextlib import redirect_stdout
import io
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import tempfile
import zipfile
from types import SimpleNamespace as Args
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
import voidup_release as r
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives.serialization import Encoding,PublicFormat

checks=[]
def refused(label,action):
    try:action()
    except (ValueError,KeyError,FileNotFoundError):checks.append(label)
    else:raise AssertionError(label+' was admitted')

with tempfile.TemporaryDirectory(prefix='voidup-production-fixture-') as folder,redirect_stdout(io.StringIO()):
    work=Path(folder);prefix=work/'prefix'
    suffix='.exe' if os.name=='nt' else ''
    for name in ['bin/voidup'+suffix,'bin/voidup-init'+suffix,'share/notice']:
        path=prefix/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(b'nonproduction fixture\n');path.chmod(0o755 if name.startswith('bin/') else 0o644)
    receipt={'host':{'system':platform.system(),'machine':platform.machine()},'production_trust':False,'trust_config_sha256':None,
             'files':{p.relative_to(prefix).as_posix():{'bytes':p.stat().st_size,'sha256':r.sha(p)} for p in prefix.rglob('*') if p.is_file()}}
    (prefix/'build.json').write_text(json.dumps(receipt))
    host={('Linux','x86_64'):'linux-x86_64',('Windows','amd64'):'windows-x86_64',('Darwin','arm64'):'macos-aarch64'}[(platform.system(),platform.machine().lower())]
    def bootstrap(name,production=False):
        r.bootstrap(Args(prefix=prefix,host=host,production=production,output=work/name))
    bootstrap('one');
    if os.name=='nt' or sys.platform=='darwin':
        with zipfile.ZipFile(work/'one') as archive:
            assert sum(p.file_size for p in archive.infolist())==json.loads((work/'one.json').read_bytes())['expanded_bytes']
    for p in prefix.rglob('*'):
        if p.is_file():os.utime(p,(1700000000,1700000000))
    bootstrap('two')
    assert (work/'one').read_bytes()==(work/'two').read_bytes();checks.append('Archive bytes independent of wall time and file mtimes')
    refused('Engineering bootstrap refused as production',lambda:bootstrap('production',True))
    (prefix/'share/notice').unlink()
    refused('Missing required payload notice refused',lambda:bootstrap('missing-notice'))
    (prefix/'share/notice').write_bytes(b'nonproduction fixture\n')
    (prefix/'bin'/('voidup'+suffix)).write_bytes(b'tampered\n')
    refused('Payload modification refused against build receipt',lambda:bootstrap('tampered'))
    assert not (work/'tampered').exists()
    from package_voidup_fixtures import write_member
    for name in ['payload-one.zip','payload-two.zip']:
        with zipfile.ZipFile(work/name,'x') as archive:
            write_member(archive,prefix/'bin','bin/',315532800,directory=True)
            write_member(archive,prefix/'share/notice','bin/notice',315532800)
    assert (work/'payload-one.zip').read_bytes()==(work/'payload-two.zip').read_bytes()
    with zipfile.ZipFile(work/'payload-one.zip') as archive:
        directory=archive.getinfo('bin/')
        assert directory.compress_type==zipfile.ZIP_STORED and directory.file_size==directory.compress_size==directory.CRC==0
        assert archive.read('bin/notice')==b'nonproduction fixture\n'
    checks.append('Deterministic payload ZIP preserves native empty-directory geometry')
    private=[Ed25519PrivateKey.generate() for _ in range(3)]
    config={'schema':1,'trust':'engineering','approval':'NONPRODUCTION fixture only','catalog':'https://localhost/catalog.json',
            'keys':[{'id':f'test-{n}','public':k.public_key().public_bytes(Encoding.Raw,PublicFormat.Raw).hex()} for n,k in enumerate(private)]}
    public=work/'public.json';public.write_bytes(r.encoded(config))
    # Actual enrollment must still refuse every one of these engineering IDs.
    invalid=work/'invalid.json';invalid.write_bytes(r.encoded(config|{'trust':'production','catalog':r.OFFICIAL_CATALOG}))
    refused('Fixture trust cannot become production enrollment',lambda:r.read_public_config(invalid))
    with patch.object(r,'read_public_config',return_value=config|{'trust':'production'}):
        root=work/'inputs/linux';root.mkdir(parents=True)
        version='1.0.0';rid='void-'+version+'-linux-x86_64';name='voidup-linux-x86_64.run'
        assets={name:b'fixture bootstrap',rid+'.zip':b'fixture payload',rid+'.json':b'fixture inventory',rid+'.release.json':b'fixture release',rid+'.package.json':b'fixture package'}
        for n,data in assets.items():(root/n).write_bytes(data)
        (root/(name+'.json')).write_bytes(r.encoded({'host':'linux-x86_64','production_trust':True,'public_config_sha256':r.sha(public),'archive_sha256':r.sha(root/name),'archive_bytes':(root/name).stat().st_size}))
        prepared={'status':'UNSIGNED; NOT PUBLISHED','version':version,'host':'linux-x86_64','source_sha':'a'*40,'production_public_config_sha256':r.sha(public),
                  'assets':{p.name:{'bytes':p.stat().st_size,'sha256':r.sha(p)} for p in root.iterdir()}}
        evidence=root/'evidence';evidence.mkdir();(evidence/'voidup-production-prepared.json').write_bytes(r.encoded(prepared))
        def assemble(name,source='a'*40):r.assemble(Args(inputs=work/'inputs',public_config=public,version=version,source_sha=source,output=work/name))
        assemble('accepted');checks.append('Exact native asset set assembled at reviewed source identity')
        refused('Different reviewed source refused',lambda:assemble('wrong-source','b'*40))
        (root/rid).write_bytes(b'unlisted file')
        assemble('unlisted');assert not (work/'unlisted'/rid).exists();checks.append('Unlisted inputs excluded from publication assets')
        (root/name).write_bytes(b'modified fixture bootstrap')
        refused('Modified native asset refused before assembly',lambda:assemble('modified'))
        assert not (work/'modified').exists()
        document={'schema':1,'domain':'voidup-catalog-v1','llvm_version':'23.1.4','min_manager':2,'serial':5,
                  'issued':int(r.time.time())-60,'expires':int(r.time.time())+3600,'revoked':['retired'],'releases':[
            {'id':rid,'host':'linux-x86_64-ubuntu24.04','selection':{'version':version,'channel':'stable'},
             **{key:value for role,extension in [('artifact','zip'),('inventory','json')] for key,value in [
                 (role+'_url',r.OFFICIAL_ASSETS+'v'+version+'/'+rid+'.'+extension),
                 (role+'_bytes',(work/'accepted'/(rid+'.'+extension)).stat().st_size),
                 (role+'_sha256',r.sha(work/'accepted'/(rid+'.'+extension)))]}}]}
        def signed(name,doc):
            path=work/name;raw=r.encoded(doc);path.write_bytes(raw)
            Path(str(path)+'.sig').write_bytes(r.encoded({'schema':1,'signatures':[{'key_id':f'test-{n}','signature':private[n].sign(r.DOMAIN+raw).hex()} for n in (0,1)]}))
            return path
        previous=signed('previous',document|{'serial':4})
        def verify(name,doc):r.verify(Args(public_config=public,catalog=signed(name,doc),assets=work/'accepted',version=version,previous=previous))
        verify('catalog',document);checks.append('Exact threshold/freshness/assets/previous floor verified')
        refused('Catalog serial regression refused',lambda:verify('serial',document|{'serial':4}))
        refused('Lost revocations refused',lambda:verify('revocations',document|{'revoked':[]}))
        refused('Wrong LLVM identity refused',lambda:verify('llvm',document|{'llvm_version':'0.0.0'}))
        refused('Revoked selected payload refused',lambda:verify('revoked-current',document|{'revoked':['retired',rid]}))
    from voidup_macos import driver_wrapper,relocate_manifest
    lines=['VOID native 23.1.4 v2','/producer/clang','','../lib/void/runtime.a','../lib/void/fixture.a','-stdlib=libc++','-nostdinc++','-isystem','/producer/Xcode/include','-isysroot','/producer/Xcode/sdk','-framework','Foundation','-arch','arm64']
    manifest=relocate_manifest(lines)
    assert not any('/producer' in line for line in manifest) and '-framework' in manifest
    checks.append('macOS manifest drops producer SDK/header paths and preserves link inputs')
    if os.name!='nt':
        subprocess.run(['/bin/sh','-n'],input=driver_wrapper().encode(),check=True)
        checks.append('macOS driver wrapper POSIX shell syntax')
subprocess.run([sys.executable,'-B',str(ROOT/'tests/voidup/macos_relocation.py')],check=True)
checks.append('Offline macOS relocation, dependency closure, RPATH and strict signature regressions')
print('PASS: '+str(len(checks))+' offline production-boundary checks')
for label in checks:print('  '+label)
