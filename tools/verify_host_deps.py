#!/usr/bin/env python3
"""Verify pinned host archives, approved source postimages and provider receipts."""
import argparse, hashlib, json, os, pathlib, re, shutil, subprocess, sys, tarfile, tempfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
PATCH_ROOT='third_party/host-patches/libsodium-1.0.22'
RECIPES=('tools/setup_host_deps.py','tools/setup_host_deps_native.py','tools/verify_host_deps.py',
         'tools/ci/llvm_sdk.py','cmake/host-sodium.targets','third_party/host-versions.json',
         PATCH_ROOT+'/PROVENANCE.json',PATCH_ROOT+'/asan-alignment.patch')
SCHEMA=3

def sha(path):
 with pathlib.Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()
def archive_entries(archive):
 expected={}
 with tarfile.open(archive) as tar:
  for member in tar:
   pieces=pathlib.PurePosixPath(member.name).parts
   if not pieces or '..' in pieces or member.name.startswith('/'):raise ValueError('invalid provider source archive path')
   relative=pathlib.Path(*pieces[1:]).as_posix()
   if member.isfile():expected[relative]=('file',hashlib.sha256(tar.extractfile(member).read()).hexdigest())
   elif member.issym():expected[relative]=('symlink',member.linkname)
   elif not member.isdir():raise ValueError('unsupported provider source archive member')
 return expected
def tree_entries(source):
 return {p.relative_to(source).as_posix():('symlink',p.readlink().as_posix()) if p.is_symlink() else ('file',sha(p))
         for p in source.rglob('*') if p.is_symlink() or p.is_file()}
def tree_digest(entries):return hashlib.sha256(json.dumps(entries,sort_keys=True).encode()).hexdigest()
def source_tree(archive,source):
 expected=archive_entries(archive)
 if tree_entries(source)!=expected:raise ValueError('provider source tree differs from pinned archive')
 return {key:value[1] for key,value in expected.items()}
def source_policy(archive_dir=None):
 pins=json.loads((ROOT/'third_party/host-versions.json').read_text());pin=pins['libsodium']
 archive_dir=pathlib.Path(archive_dir) if archive_dir else ROOT/'.toolchains/host/downloads'
 archive=archive_dir/pin['url'].rsplit('/',1)[-1]
 if sha(archive)!=pin['sha256']:raise ValueError('libsodium base archive mismatch')
 metadata=ROOT/PATCH_ROOT/'PROVENANCE.json';policy=json.loads(metadata.read_text());patch=metadata.parent/policy['patch']
 if not patch.resolve().is_relative_to(metadata.parent.resolve()) or policy['schema']!=1 or policy['provider']!='libsodium' or policy['version']!=pin['version'] or policy['archive_sha256']!=pin['sha256'] or policy['upstream_revision']!=pin['revision'] or sha(patch)!=policy['patch_sha256']:raise ValueError('approved source patch policy mismatch')
 expected=archive_entries(archive);patched=dict(expected)
 for name,record in policy['files'].items():
  if pathlib.PurePosixPath(name).is_absolute() or '..' in pathlib.PurePosixPath(name).parts or expected.get(name)!=('file',record['before_sha256']):raise ValueError('approved source preimage mismatch: '+name)
  patched[name]=('file',record['after_sha256'])
 return {'policy_sha256':sha(metadata),'archive_sha256':pin['sha256'],'version':pin['version'],
         'upstream_revision':pin['revision'],'patch':{'path':patch.relative_to(ROOT).as_posix(),'sha256':sha(patch),'upstream_alignment_commit':policy['upstream_alignment_commit']},
         'postimages':{name:record['after_sha256'] for name,record in policy['files'].items()},
         'pristine_tree_sha256':tree_digest(expected),'applied_tree_sha256':tree_digest(patched)}
def prepare_sodium(source,archive,destination):
 source_tree(archive,source);policy=source_policy(pathlib.Path(archive).parent)
 if destination.exists():raise ValueError('refusing to reuse disposable patched source: '+str(destination))
 shutil.copytree(source,destination,symlinks=True)
 # Source application only: no index, object or history mutation, including inside a checkout.
 env=dict(os.environ,GIT_CEILING_DIRECTORIES=str(destination.parent.resolve()))
 command=['git','-c','core.autocrlf=false','apply','--no-index',str(ROOT/policy['patch']['path'])]
 subprocess.run(command[:5]+['--check']+command[5:],cwd=destination,env=env,check=True)
 subprocess.run(command,cwd=destination,env=env,check=True)
 verify_sodium_copy(destination,policy)
 return policy
def verify_sodium_copy(source,policy,generated_version=False):
 entries=tree_entries(source)
 if generated_version:
  header='src/libsodium/include/sodium/version.h'
  if header in entries:
   if '#define SODIUM_VERSION_STRING "1.0.22"' not in (source/header).read_text():raise ValueError('generated sodium version mismatch')
   del entries[header]
 if tree_digest(entries)!=policy['applied_tree_sha256']:raise ValueError('patched provider source/postimages mismatch')
def recipe_hashes():return {name:sha(ROOT/name) for name in RECIPES}
def toolchain():
 sys.path.insert(0,str(ROOT/'tools/ci'));import llvm_sdk
 sdk=ROOT/'.toolchains'/('llvm-'+llvm_sdk.VERSION)
 if json.loads((sdk/'void-sdk-identity.json').read_text())!=llvm_sdk.archive_identity():raise ValueError('host provider SDK identity mismatch')
 if '#define LLVM_VERSION_STRING "23.1.4"' not in (sdk/'include/llvm/Config/llvm-config.h').read_text():raise ValueError('host provider SDK headers mismatch')
 windows=os.name=='nt';names=['clang-cl.exe','lld-link.exe','llvm-lib.exe'] if windows else ['clang','ld64.lld' if sys.platform=='darwin' else 'ld.lld','llvm-ar','llvm-ranlib','llvm-symbolizer']
 tools={}
 for name in names:
  path=sdk/'bin'/name
  if not path.is_file() or path.resolve().parent!= (sdk/'bin').resolve():raise ValueError('pinned host tool missing/escaped: '+name)
  if name!='llvm-lib.exe':
   output=subprocess.check_output([path,'--version'],text=True)
   if not re.search(r'\b23\.1\.4(?:\D|$)',output):raise ValueError('pinned host tool version mismatch: '+name)
  tools[name]={'path':str(path),'sha256':sha(path)}
 if windows:llvm_sdk.windows_clangcl(sdk)
 return {'sdk':json.loads((sdk/'void-sdk-identity.json').read_text()),'tools':tools}
def upstream_results(build):
 policy=json.loads((ROOT/PATCH_ROOT/'PROVENANCE.json').read_text());rows={}
 for path in (build/'test/default').glob('*.trs'):
  match=re.search(r'^:test-result: (\S+)',path.read_text(),re.M);rows[path.stem]=match[1] if match else 'MISSING'
 if set(rows)!=set(policy['upstream_test_targets']) or any(value!='PASS' for value in rows.values()):raise ValueError('full upstream 87-test acceptance failed: '+repr(rows))
 return {'runner':'make check','targets':rows,'total':len(rows),'output_oracle_exceptions':policy['asan_output_oracle_exceptions']}
def build_identity(record):
 return hashlib.sha256(json.dumps({k:v for k,v in record.items() if k not in ['build_identity','commands','build_evidence']},sort_keys=True).encode()).hexdigest()
def verify_prefix(prefix,profile,system,architecture):
 prefix=pathlib.Path(prefix).resolve();pins=json.loads((ROOT/'third_party/host-versions.json').read_text());receipt=json.loads((prefix/'VOID-HOST-DEPENDENCIES.json').read_text())
 if receipt.get('schema')!=SCHEMA:raise ValueError('unpatched/obsolete host receipt schema; rebuild providers')
 if receipt['pins']!=pins or receipt['profile']!=profile or receipt['platform']!=system or receipt['architecture']!=architecture:raise ValueError('provider pin/profile/platform/architecture mismatch')
 if receipt.get('source_provenance')!=source_policy():raise ValueError('provider approved patch/source identity mismatch')
 if receipt.get('recipe_sha256')!=recipe_hashes():raise ValueError('provider recipe/patch inventory mismatch')
 if receipt.get('toolchain')!=toolchain():raise ValueError('provider compiler/SDK/tool identity mismatch')
 if receipt.get('build_identity')!=build_identity(receipt):raise ValueError('provider build identity mismatch')
 for name in ['libsodium','libarchive']:
  pin=pins[name];archive=ROOT/'.toolchains/host/downloads'/pin['url'].rsplit('/',1)[-1]
  if sha(archive)!=pin['sha256']:raise ValueError('provider corresponding source mismatch: '+name)
 for relative,expected in receipt['files'].items():
  path=prefix/relative
  if not path.resolve().is_relative_to(prefix) or not path.is_file() or sha(path)!=expected:raise ValueError('provider file mismatch: '+relative)
 required=['lib/archive.lib','lib/libsodium.lib'] if system=='Windows' else ['lib/libarchive.a','lib/libsodium.a']
 if any(name not in receipt['files'] for name in required):raise ValueError('static provider absent from receipt')
 if sha(pathlib.Path(receipt['zlib']))!=receipt['zlib_sha256']:raise ValueError('provider zlib closure mismatch')
 disabled={'LZMA','ZSTD','BZip2','OPENSSL','LIBB2','LIBXML2','EXPAT','ICONV'}|({'CNG'} if system=='Windows' else set())
 if set(receipt.get('disabled_features',[]))!=disabled:raise ValueError('provider feature inventory mismatch')
 if system!='Windows':
  tests=receipt.get('upstream_tests',{});policy=json.loads((ROOT/PATCH_ROOT/'PROVENANCE.json').read_text())
  if set(tests.get('targets',{}))!=set(policy['upstream_test_targets']) or any(v!='PASS' for v in tests['targets'].values()) or tests.get('output_oracle_exceptions')!=policy['asan_output_oracle_exceptions']:raise ValueError('provider upstream test evidence missing')
 return receipt
def qualify(profile):
 prefix=ROOT/'.toolchains/host/prefix'/profile;receipt=verify_prefix(prefix,profile,'Linux','x86_64')
 return {'profile':profile,'pins':receipt['pins'],'build_identity':receipt['build_identity'],'source_provenance':receipt['source_provenance'],'receipt_sha256':sha(prefix/'VOID-HOST-DEPENDENCIES.json')}
if __name__=='__main__':
 if '--reconstruct-source' in sys.argv:
  parser=argparse.ArgumentParser();parser.add_argument('--reconstruct-source',type=pathlib.Path,required=True);parser.add_argument('--archives',type=pathlib.Path,required=True);args=parser.parse_args()
  pin=json.loads((ROOT/'third_party/host-versions.json').read_text())['libsodium'];archive=args.archives/pin['url'].rsplit('/',1)[-1]
  if sha(archive)!=pin['sha256']:raise SystemExit('base archive hash mismatch')
  with tempfile.TemporaryDirectory() as temporary:
   with tarfile.open(archive) as package:package.extractall(temporary,filter='data')
   policy=prepare_sodium(pathlib.Path(temporary)/('libsodium-'+pin['version']),archive,args.reconstruct_source)
  print(json.dumps(policy,indent=2))
 elif '--prefix' in sys.argv:
  parser=argparse.ArgumentParser();parser.add_argument('--prefix',required=True);parser.add_argument('--profile',required=True);parser.add_argument('--platform',required=True);parser.add_argument('--architecture',required=True);args=parser.parse_args()
  record=verify_prefix(args.prefix,args.profile,args.platform,args.architecture)
  print('Verified approved patched '+args.platform+' host providers: '+args.profile+'; build '+record['build_identity'])
 else:
  results=[qualify(p) for p in (sys.argv[1:] or ['Release','Debug','strict'])]
  print(json.dumps(results,indent=2))
