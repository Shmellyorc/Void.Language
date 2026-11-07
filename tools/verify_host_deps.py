#!/usr/bin/env python3
"""Verify exact source trees and installed static provider closure for build tooling."""
import hashlib,json,pathlib,subprocess,sys,tarfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def source_tree(archive,source):
 expected={}
 with tarfile.open(archive) as tar:
  for member in tar:
   pieces=pathlib.PurePosixPath(member.name).parts
   if not pieces or '..' in pieces or member.name.startswith('/'):raise ValueError('invalid provider source archive path')
   relative=pathlib.Path(*pieces[1:])
   if member.isfile():expected[str(relative)]=('file',hashlib.sha256(tar.extractfile(member).read()).hexdigest())
   elif member.issym():expected[str(relative)]=('symlink',member.linkname)
   elif not member.isdir():raise ValueError('unsupported provider source archive member')
 actual={}
 for path in source.rglob('*'):
  if path.is_symlink():actual[str(path.relative_to(source))]=('symlink',path.readlink().as_posix())
  elif path.is_file():actual[str(path.relative_to(source))]=('file',sha(path))
 if actual!=expected:
  changed=[key for key in actual.keys()|expected.keys() if actual.get(key)!=expected.get(key)]
  raise ValueError('provider source tree differs from pinned archive: '+repr(changed[:10]))
 return {key:value[1] for key,value in expected.items()}
def qualify(profile):
 pins=json.loads((ROOT/'third_party/host-versions.json').read_text());base=ROOT/'.toolchains/host';prefix=base/'prefix'/profile
 receipt=json.loads((prefix/'VOID-HOST-DEPENDENCIES.json').read_text())
 if receipt['pins']!=pins or receipt['profile']!=profile:raise ValueError('provider pin/profile mismatch')
 trees={}
 for name in ['libsodium','libarchive']:
  pin=pins[name];archive=base/'downloads'/pin['url'].rsplit('/',1)[-1]
  if sha(archive)!=pin['sha256']:raise ValueError('source archive mismatch: '+name)
  trees[name]=source_tree(archive,base/'sources'/(name+'-'+pin['version']))
 for name,digest in receipt['files'].items():
  if sha(prefix/name)!=digest:raise ValueError('installed provider file mismatch: '+name)
 if sha(pathlib.Path(receipt['zlib']))!=receipt['zlib_sha256']:raise ValueError('zlib closure mismatch')
 members={name:subprocess.check_output(['ar','t',prefix/'lib'/name],text=True).splitlines() for name in ['libsodium.a','libarchive.a']}
 cmake=(base/'build'/profile/'archive/CMakeCache.txt').read_text()
 for codec in ['LZMA','ZSTD','BZip2','OPENSSL','LIBB2','LIBXML2','EXPAT','ICONV']:
  if 'ENABLE_'+codec+':BOOL=OFF' not in cmake:raise ValueError('unapproved provider feature: '+codec)
 return {'profile':profile,'pins':pins,'compiler':receipt['compiler'],'compiler_sha256':sha(pathlib.Path(receipt['compiler'])),'flags':receipt['flags'],'zlib_sha256':receipt['zlib_sha256'],'archive_members':members,'source_files':trees,'receipt_sha256':sha(prefix/'VOID-HOST-DEPENDENCIES.json')}
def verify_prefix(prefix,profile,system,architecture):
 prefix=pathlib.Path(prefix).resolve()
 pins=json.loads((ROOT/'third_party/host-versions.json').read_text())
 receipt=json.loads((prefix/'VOID-HOST-DEPENDENCIES.json').read_text())
 if receipt['pins']!=pins or receipt['profile']!=profile:raise ValueError('provider pin/profile mismatch')
 if receipt.get('platform','Linux')!=system or receipt.get('architecture','x86_64')!=architecture:raise ValueError('provider platform/architecture mismatch')
 for relative,expected in receipt['files'].items():
  path=prefix/relative
  if not path.resolve().is_relative_to(prefix) or not path.is_file() or sha(path)!=expected:raise ValueError('provider file mismatch: '+relative)
 for name in ['libsodium','libarchive']:
  pin=pins[name]
  archive=ROOT/'.toolchains/host/downloads'/pin['url'].rsplit('/',1)[-1]
  if sha(archive)!=pin['sha256']:raise ValueError('provider corresponding source mismatch: '+name)
 if system!='Linux':
  required_recipes={'tools/setup_host_deps.py','tools/setup_host_deps_native.py','tools/verify_host_deps.py','cmake/host-sodium.targets'}
  if receipt.get('schema')!=2 or set(receipt.get('recipe_sha256',{}))!=required_recipes:raise ValueError('native provider recipe inventory missing')
  disabled={'LZMA','ZSTD','BZip2','OPENSSL','LIBB2','LIBXML2','EXPAT','ICONV'}|({'CNG'} if system=='Windows' else set())
  if set(receipt.get('disabled_features',[]))!=disabled:raise ValueError('native provider feature inventory mismatch')
 for relative,expected in receipt.get('recipe_sha256',{}).items():
  path=ROOT/relative
  if not path.resolve().is_relative_to(ROOT) or sha(path)!=expected:raise ValueError('provider recipe mismatch: '+relative)
 required=['lib/archive.lib','lib/libsodium.lib'] if system=='Windows' else ['lib/libarchive.a','lib/libsodium.a']
 for relative in required:
  if relative not in receipt['files']:raise ValueError('static provider absent from receipt: '+relative)
 return receipt
if __name__=='__main__':
 if '--prefix' in sys.argv:
  import argparse
  parser=argparse.ArgumentParser();parser.add_argument('--prefix',required=True);parser.add_argument('--profile',required=True);parser.add_argument('--platform',required=True);parser.add_argument('--architecture',required=True);args=parser.parse_args()
  record=verify_prefix(args.prefix,args.profile,args.platform,args.architecture)
  print('Verified pinned '+args.platform+' host providers: '+args.profile+'; '+str(len(record['files']))+' files')
 else:
  results=[qualify(p) for p in (sys.argv[1:] or ['Release','Debug','strict'])]
  out=ROOT/'docs/voidup/evidence/dependencies.json';out.write_text(json.dumps(results,indent=2)+'\n')
  print('Verified pinned source trees, static members, disabled codecs and installed hashes: '+', '.join(x['profile'] for x in results))
