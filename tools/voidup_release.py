"""Production preparation using the existing VOIDUP producer and signature domain.

No publication, installation into a user prefix, or network signing service.
Private-key operations are explicit offline commands. CI uses public inputs only.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import sys
import time
import tarfile
import tempfile
import zipfile
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey, Ed25519PublicKey
from cryptography.hazmat.primitives.serialization import Encoding, PublicFormat, PrivateFormat, NoEncryption, load_pem_private_key
from package_voidup_fixtures import ROOT, DOMAIN, encoded, produce, sha

OFFICIAL_CATALOG = 'https://github.com/Shmellyorc/Void.Language/releases/download/voidup-stable/catalog.json'
OFFICIAL_ASSETS = 'https://github.com/Shmellyorc/Void.Language/releases/download/'
VERSION = re.compile(r'(0|[1-9][0-9]{0,8})\.(0|[1-9][0-9]{0,8})\.(0|[1-9][0-9]{0,8})\Z')
IDENTIFIER = re.compile(r'[A-Za-z0-9][A-Za-z0-9._-]{0,79}\Z')

def read_public_config(path):
    config = json.loads(Path(path).read_text())
    if set(config) != {'schema', 'trust', 'catalog', 'keys', 'approval'} or config['schema'] != 1:
        raise ValueError('Invalid public root configuration')
    if config['trust'] not in ('production', 'engineering') or len(config['keys']) != 3:
        raise ValueError('Three independently controlled public roots are required')
    if not isinstance(config['approval'], str) or not config['approval'].strip():
        raise ValueError('An explicit public enrollment review reference is required')
    ids, public = set(), set()
    for key in config['keys']:
        if set(key) != {'id','public'} or not IDENTIFIER.fullmatch(key['id']) or not re.fullmatch('[0-9a-f]{64}', key['public']):
            raise ValueError('Invalid public key record')
        if key['id'] in ids or key['public'] in public:
            raise ValueError('Public root IDs and key bytes must be distinct')
        ids.add(key['id']); public.add(key['public'])
    if config['trust'] == 'production':
        test_keys = set(re.findall(r'"([0-9a-f]{64})"',(ROOT/'apps/voidup/trust_keys.void').read_text()))
        if any(k.startswith('test-') for k in ids) or public & test_keys or config['catalog'] != OFFICIAL_CATALOG:
            raise ValueError('Production enrollment must use independent roots and the official release source')
    else:
        from urllib.parse import urlsplit
        address=urlsplit(config['catalog'])
        if address.scheme!='https' or address.username or address.password or address.query or address.fragment:
            raise ValueError('Engineering catalog must be an explicit HTTPS endpoint')
    return config

def authenticate(raw, envelope, config):
    if not 0<len(raw)<=8388608 or not 0<len(envelope)<=4096:raise ValueError('Signature input exceeds native bounds')
    proof=json.loads(envelope); known={k['id']:k['public'] for k in config['keys']}
    if set(proof)!={'schema','signatures'} or proof.get('schema')!=1 or not 2<=len(proof.get('signatures',[]))<=3:
        raise ValueError('A 2-of-3 signature envelope is required')
    seen=set()
    for signature in proof['signatures']:
        if set(signature)!={'key_id','signature'} or not re.fullmatch('[0-9a-fA-F]{128}',signature['signature']):raise ValueError('Malformed endorsement')
        key_id=signature['key_id']
        if key_id in seen or key_id not in known:raise ValueError('Duplicate or unknown signing key')
        Ed25519PublicKey.from_public_bytes(bytes.fromhex(known[key_id])).verify(bytes.fromhex(signature['signature']),DOMAIN+raw)
        seen.add(key_id)
    return json.loads(raw)

def exclusive(path, data, mode=0o600):
    fd=os.open(path,os.O_WRONLY|os.O_CREAT|os.O_EXCL,mode)
    with os.fdopen(fd,'wb') as f:f.write(data);f.flush();os.fsync(f.fileno())

def keygen(args):
    # One custodian runs this once on their own offline system. This command
    # deliberately does not create all three release keys in one CI job.
    if not IDENTIFIER.fullmatch(args.key_id) or args.key_id.startswith('test-'):
        raise ValueError('Choose a distinct production key ID')
    directory=args.directory.absolute()
    if os.name=='nt':raise ValueError('Offline key generation is currently qualified on POSIX private storage; native Windows key custody requires a separate ACL qualification')
    if directory.resolve()!=directory or directory.is_relative_to(ROOT) or directory.is_relative_to(ROOT.parent/'Void.Language-archives'):
        raise ValueError('Private keys must be outside checkout, evidence and linked paths')
    for parent in directory.parents:
        state=parent.stat()
        if state.st_uid not in (0,os.geteuid()) or state.st_mode&0o022:raise ValueError('Private key ancestors must not be writable by another account')
    directory.mkdir(mode=0o700,exist_ok=False)
    key=Ed25519PrivateKey.generate()
    exclusive(directory/'private.pem',key.private_bytes(Encoding.PEM,PrivateFormat.PKCS8,NoEncryption()))
    exclusive(directory/'public.json',encoded({'id':args.key_id,'public':key.public_key().public_bytes(Encoding.Raw,PublicFormat.Raw).hex()}))
    print('Key generated privately. Transfer only public.json for enrollment; keep private.pem offline.')

def package(args):
    if not VERSION.fullmatch(args.version):raise ValueError('A numeric stable version is required')
    if not IDENTIFIER.fullmatch(args.release_id):raise ValueError('A safe release identifier is required')
    if sys.platform not in ('linux','win32'):raise ValueError('Native macOS complete-toolchain packaging remains unqualified; no substitute payload is produced')
    host='windows-x86_64' if os.name=='nt' else 'linux-x86_64-ubuntu24.04'
    record=produce(args.build.resolve(),args.release_id,args.output.resolve(),args.sdk.resolve(),host=host,configuration=args.configuration)
    record.update(selection={'version':args.version,'channel':'stable'})
    for role,folder,extension in [('artifact','packages','zip'),('inventory','inventories','json')]:
        record[role+'_url']=OFFICIAL_ASSETS+'v'+args.version+'/'+args.release_id+'.'+extension
    exclusive(args.output/(args.release_id+'.release.json'),encoded(record))
    print('Complete payload, exact inventory and unsigned release descriptor prepared.')

def catalog(args):
    config=read_public_config(args.public_config)
    revoked=[]
    if args.previous:
        old=authenticate(args.previous.read_bytes(),Path(str(args.previous)+'.sig').read_bytes(),config)
        if args.serial<=old['serial']:raise ValueError('Catalog serial must increase')
        revoked=old['revoked']
    revoked=sorted(set(revoked+args.revoke))
    records=[json.loads(p.read_text()) for p in args.release]
    identities=set();versions=set()
    for r in records:
        if not VERSION.fullmatch(r['selection']['version']) or r['selection']['channel']!='stable' or not IDENTIFIER.fullmatch(r['id']):raise ValueError('Invalid stable release descriptor')
        identity=(r['host'],r['id']);version=(r['host'],r['selection']['version'])
        if identity in identities or version in versions:raise ValueError('Duplicate stable release identity/version')
        identities.add(identity);versions.add(version)
        if config['trust']=='production':
            for role,extension in [('artifact','zip'),('inventory','json')]:
                expected=OFFICIAL_ASSETS+'v'+r['selection']['version']+'/'+r['id']+'.'+extension
                if r[role+'_url']!=expected:raise ValueError('Production assets must use their exact immutable version and release ID')
    if not 0<args.serial or not 0<args.valid_days<=30 or not 1<=len(records)<=64 or len(revoked)>1024:raise ValueError('Invalid catalog bounds, serial or validity interval')
    issued=int(time.time())-60
    raw=encoded(dict(schema=1,domain='voidup-catalog-v1',serial=args.serial,issued=issued,expires=issued+args.valid_days*86400,min_manager=2,llvm_version='23.1.4',revoked=revoked,releases=records))
    exclusive(args.output,raw);print('Exact unsigned catalog prepared. Two custodians must endorse these bytes offline.')

def sign_catalog(args):
    if os.name=='nt':raise ValueError('Offline private-key signing currently requires qualified POSIX private storage; Windows key-custody ACL qualification is separate')
    config=read_public_config(args.public_config);known={k['id']:k['public'] for k in config['keys']}
    raw=args.catalog.read_bytes();signatures=[];seen=set()
    for specification in args.key:
        key_id,filename=specification.split('=',1)
        if key_id in seen or key_id not in known:raise ValueError('Duplicate or unenrolled signer')
        path=Path(filename);mode=path.stat()
        if path.is_symlink() or (os.name!='nt' and (mode.st_uid!=os.geteuid() or mode.st_mode&0o077 or mode.st_nlink!=1)):raise ValueError('Private key ownership/permissions rejected')
        key=load_pem_private_key(path.read_bytes(),None)
        if not isinstance(key,Ed25519PrivateKey) or key.public_key().public_bytes(Encoding.Raw,PublicFormat.Raw).hex()!=known[key_id]:raise ValueError('Private key does not match its enrolled public identity')
        signatures.append({'key_id':key_id,'signature':key.sign(DOMAIN+raw).hex()});seen.add(key_id)
    if not 1<=len(signatures)<=3:raise ValueError('One to three distinct offline endorsements are required')
    envelope=encoded({'schema':1,'signatures':signatures})
    if len(signatures)>=2:authenticate(raw,envelope,config)
    exclusive(args.output,envelope);print('Offline endorsement prepared. A single endorsement is not an installable catalog signature.')

def combine(args):
    config=read_public_config(args.public_config);signatures=[]
    for path in args.endorsement:
        proof=json.loads(path.read_bytes())
        if proof['schema']!=1:raise ValueError('Invalid endorsement schema')
        signatures.extend(proof['signatures'])
    envelope=encoded({'schema':1,'signatures':signatures})
    authenticate(args.catalog.read_bytes(),envelope,config)
    exclusive(args.output,envelope);print('Independent endorsements combined; 2-of-3 verification passed.')

def enroll(args):
    config={'schema':1,'trust':'production','catalog':OFFICIAL_CATALOG,'approval':args.approval,'keys':[json.loads(p.read_text()) for p in args.public]}
    # Validate the public-only enrollment before publishing its candidate file.
    temporary=args.output.with_suffix('.checking')
    exclusive(temporary,encoded(config))
    try:read_public_config(temporary)
    finally:temporary.unlink()
    exclusive(args.output,encoded(config));print('Public-only enrollment candidate prepared for Boss review; no private keys were read.')

def verify(args):
    config=read_public_config(args.public_config)
    if config['trust']!='production':raise ValueError('Public release requires approved production roots')
    raw=args.catalog.read_bytes();document=authenticate(raw,Path(str(args.catalog)+'.sig').read_bytes(),config)
    now=int(time.time())
    if document['domain']!='voidup-catalog-v1' or document['schema']!=1 or document['min_manager']!=2 or not document['issued']<=now<document['expires'] or document['expires']-document['issued']>2592000:
        raise ValueError('Catalog identity or freshness rejected')
    if not 1<=len(document['releases'])<=64 or len(document['revoked'])>1024:raise ValueError('Publication catalog exceeds native bounds')
    seen=set()
    for release in document['releases']:
        selection=release.get('selection') or {}
        if selection.get('channel')!='stable' or not VERSION.fullmatch(selection.get('version','')) or not IDENTIFIER.fullmatch(release['id']):raise ValueError('Invalid stable publication identity')
        key=(release['host'],selection['version'])
        if key in seen or args.version and selection['version']!=args.version:raise ValueError('Ambiguous or mismatched publication version')
        seen.add(key)
        for role in ('artifact','inventory'):
            extension='zip' if role=='artifact' else 'json'
            expected=OFFICIAL_ASSETS+'v'+selection['version']+'/'+release['id']+'.'+extension
            if release[role+'_url']!=expected:raise ValueError('Publication reference differs from its immutable version and release ID')
            name=release[role+'_url'].rsplit('/',1)[-1];file=args.assets/name
            if file.stat().st_size!=release[role+'_bytes'] or sha(file)!=release[role+'_sha256']:raise ValueError('Publication asset differs from signed content')
    print('Production public roots, threshold, freshness and exact publication assets verified. Publication remains a separate action.')

def bootstrap(args):
    prefix=args.prefix.resolve();receipt=json.loads((prefix/'build.json').read_text())
    system=receipt['host']['system'];machine=receipt['host']['machine'].lower()
    actual={('Linux','x86_64'):'linux-x86_64',('Windows','amd64'):'windows-x86_64',('Windows','x86_64'):'windows-x86_64',('Darwin','arm64'):'macos-aarch64'}.get((system,machine))
    if actual!=args.host:raise ValueError('Bootstrap host label differs from the actual native producer')
    if args.production and not receipt['production_trust']:raise ValueError('Engineering bootstraps cannot become public production installers')
    suffix='.exe' if args.host=='windows-x86_64' else ''
    for name in ['voidup','voidup-init']:
        if not (prefix/'bin'/(name+suffix)).is_file():raise ValueError('Missing native bootstrap executable')
    # Only the runnable payload and required notices enter consumer downloads.
    paths=[p for base in ['bin','lib','share'] for p in (prefix/base).rglob('*') if p.is_file()]
    if any(p.is_symlink() for p in paths):raise ValueError('Bootstrap aliases must be materialized regular files')
    metadata={'schema':1,'host':args.host,'production_trust':receipt['production_trust'],'public_config_sha256':receipt['trust_config_sha256'],'files':{p.relative_to(prefix).as_posix():{'bytes':p.stat().st_size,'sha256':sha(p)} for p in paths}}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    if args.output.exists():raise ValueError('Refusing to replace a prepared bootstrap')
    if args.host=='linux-x86_64':
        header='''#!/bin/sh
set -eu
umask 077
work=$(/usr/bin/mktemp -d "${TMPDIR:-/tmp}/voidup.XXXXXXXX")
trap '/usr/bin/rm -rf -- "$work"' EXIT HUP INT TERM
/usr/bin/tail -n +PAYLOAD_LINE "$0" | /usr/bin/tar --use-compress-program=/usr/bin/gzip -xf - -C "$work"
"$work/bin/voidup-init" "$@"
exit $?
__VOIDUP_PAYLOAD__
'''
        header=header.replace('PAYLOAD_LINE',str(len(header.splitlines())+1))
        with args.output.open('xb') as target:
            target.write(header.encode())
            with tarfile.open(fileobj=target,mode='w|gz',format=tarfile.PAX_FORMAT) as archive:
                for path in paths:archive.add(path,arcname=path.relative_to(prefix).as_posix(),recursive=False)
        args.output.chmod(0o700)
    else:
        if args.host not in ('windows-x86_64','macos-aarch64'):raise ValueError('Unsupported bootstrap host')
        with zipfile.ZipFile(args.output,'x',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as archive:
            for path in paths:archive.write(path,path.relative_to(prefix).as_posix())
            if args.host=='windows-x86_64':
                archive.writestr('Install VOID.cmd','@echo off\r\n"%~dp0bin\\voidup-init.exe" %*\r\nif errorlevel 1 pause\r\n')
            else:
                info=zipfile.ZipInfo('Install VOID.command');info.create_system=3;info.external_attr=0o100700<<16
                archive.writestr(info,'#!/bin/sh\nset -eu\ncd "$(dirname "$0")"\nexec ./bin/voidup-init "$@"\n')
    metadata.update(archive_bytes=args.output.stat().st_size,archive_sha256=sha(args.output),expanded_bytes=sum(p.stat().st_size for p in paths),dependencies={'linux-x86_64':['POSIX sh','mktemp','tail','tar with gzip','ELF dependencies measured separately'],'windows-x86_64':['Windows cmd','native PE imports measured separately'],'macos-aarch64':['POSIX sh','native Mach-O dependencies measured separately']}[args.host])
    exclusive(Path(str(args.output)+'.json'),encoded(metadata));print('Bootstrap prepared; actual compressed and expanded sizes recorded. No publication performed.')

def assemble(args):
    config=read_public_config(args.public_config)
    if config['trust']!='production':raise ValueError('Only approved production roots may enter public payload assembly')
    config_hash=sha(args.public_config);prepared=list(args.inputs.rglob('voidup-production-prepared.json'))
    if not prepared:raise ValueError('Missing successful native production-preparation receipts')
    hosts=set()
    for path in prepared:
        receipt=json.loads(path.read_text())
        if receipt['version']!=args.version or receipt['production_public_config_sha256']!=config_hash:raise ValueError('Production preparation differs from the selected version/public roots')
        if receipt['host'] in hosts:raise ValueError('Duplicate prepared native host')
        if receipt['host']=='windows-x86_64' and not receipt['windows_registry_path_qualified']:raise ValueError('Windows PATH qualification missing')
        hosts.add(receipt['host'])
    args.output.mkdir(mode=0o700,exist_ok=False)
    selected=[]
    for path in args.inputs.rglob('*'):
        if not path.is_file():continue
        bootstrap_metadata=path.name in ('voidup-linux-x86_64.run.json','voidup-windows-x86_64.zip.json')
        if path.suffix=='.zip' or path.suffix=='.run' or path.name.endswith('.release.json') or path.parent.name=='inventories' or bootstrap_metadata:
            if path.name.startswith('voidup-') and path.name.endswith('.json'):
                receipt=json.loads(path.read_text())
                if 'production_trust' in receipt:
                    if not receipt['production_trust'] or receipt['public_config_sha256']!=config_hash:raise ValueError('Bootstrap has different or nonproduction public roots')
                    asset=path.with_suffix('')
                    if not asset.is_file() or sha(asset)!=receipt['archive_sha256']:raise ValueError('Bootstrap archive differs from preparation receipt')
            if (args.output/path.name).exists():raise ValueError('Publication asset basename collision')
            import shutil
            shutil.copyfile(path,args.output/path.name);selected.append(path.name)
    exclusive(args.output/'prepared-assets.json',encoded({'schema':1,'hosts':sorted(hosts),'version':args.version,'files':selected}))
    print('Qualified unsigned assets assembled. Signed catalog verification is still required before publication.')

def main():
    p=argparse.ArgumentParser(description=__doc__);s=p.add_subparsers(dest='command',required=True)
    k=s.add_parser('keygen');k.add_argument('--key-id',required=True);k.add_argument('--directory',type=Path,required=True);k.set_defaults(action=keygen)
    e=s.add_parser('enroll');e.add_argument('--public',type=Path,action='append',required=True);e.add_argument('--approval',required=True);e.add_argument('--output',type=Path,required=True);e.set_defaults(action=enroll)
    b=s.add_parser('package');b.add_argument('--build',type=Path,required=True);b.add_argument('--sdk',type=Path,required=True);b.add_argument('--release-id',required=True);b.add_argument('--version',required=True);b.add_argument('--configuration',choices=['Release','Debug']);b.add_argument('--output',type=Path,required=True);b.set_defaults(action=package)
    c=s.add_parser('catalog');c.add_argument('--public-config',type=Path,required=True);c.add_argument('--serial',type=int,required=True);c.add_argument('--valid-days',type=int,default=7);c.add_argument('--release',type=Path,action='append',required=True);c.add_argument('--previous',type=Path);c.add_argument('--revoke',action='append',default=[]);c.add_argument('--output',type=Path,required=True);c.set_defaults(action=catalog)
    g=s.add_parser('sign');g.add_argument('--public-config',type=Path,required=True);g.add_argument('--catalog',type=Path,required=True);g.add_argument('--key',action='append',required=True,help='Enrolled ID=private PEM path; offline use only');g.add_argument('--output',type=Path,required=True);g.set_defaults(action=sign_catalog)
    m=s.add_parser('combine');m.add_argument('--public-config',type=Path,required=True);m.add_argument('--catalog',type=Path,required=True);m.add_argument('--endorsement',type=Path,action='append',required=True);m.add_argument('--output',type=Path,required=True);m.set_defaults(action=combine)
    v=s.add_parser('verify');v.add_argument('--public-config',type=Path,required=True);v.add_argument('--catalog',type=Path,required=True);v.add_argument('--assets',type=Path,required=True);v.add_argument('--version');v.set_defaults(action=verify)
    b=s.add_parser('bootstrap');b.add_argument('--prefix',type=Path,required=True);b.add_argument('--host',required=True);b.add_argument('--production',action='store_true');b.add_argument('--output',type=Path,required=True);b.set_defaults(action=bootstrap)
    f=s.add_parser('assemble');f.add_argument('--inputs',type=Path,required=True);f.add_argument('--public-config',type=Path,required=True);f.add_argument('--version',required=True);f.add_argument('--output',type=Path,required=True);f.set_defaults(action=assemble)
    a=p.parse_args();a.action(a)

if __name__=='__main__':main()
