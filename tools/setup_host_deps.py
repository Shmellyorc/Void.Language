#!/usr/bin/env python3
"""Build tooling only: pinned static host primitives. No installation policy."""
import argparse, hashlib, json, os, pathlib, platform, shutil, subprocess, sys, tarfile, urllib.request
from verify_host_deps import source_tree
ROOT=pathlib.Path(__file__).resolve().parents[1]
def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def run(args, cwd, env=None): subprocess.run(list(map(str,args)),cwd=cwd,env=env,check=True)
def main():
 if platform.system() in ('Windows','Darwin'):
  raise SystemExit(subprocess.call([sys.executable,str(ROOT/'tools/setup_host_deps_native.py'),*sys.argv[1:]]))
 p=argparse.ArgumentParser();p.add_argument('--profile',choices=['Debug','Release','strict'],required=True);p.add_argument('--jobs',default='4');a=p.parse_args()
 if os.uname().sysname!='Linux': raise SystemExit('This dependency builder is qualified only on Linux; native Windows/macOS recipes remain required.')
 pins=json.loads((ROOT/'third_party/host-versions.json').read_text());base=ROOT/'.toolchains/host';prefix=base/'prefix'/a.profile
 cc=ROOT/'.toolchains/llvm-23.1.4/bin/clang';flags='-g -O0' if a.profile=='Debug' else '-g -O2'
 if a.profile=='strict': flags+=' -fsanitize=address,undefined -fno-omit-frame-pointer'
 env=dict(os.environ,CC=str(cc),CFLAGS=flags,LDFLAGS=('-fsanitize=address,undefined' if a.profile=='strict' else ''))
 sources={}
 for name in ['libsodium','libarchive']:
  pin=pins[name];archive=base/'downloads'/pin['url'].rsplit('/',1)[-1];archive.parent.mkdir(parents=True,exist_ok=True)
  if not archive.exists():
   with urllib.request.urlopen(pin['url']) as inp: archive.write_bytes(inp.read())
  if digest(archive)!=pin['sha256']: raise SystemExit('Source hash mismatch: '+name)
  source=base/'sources'/(name+'-'+pin['version']);sources[name]=source
  if not source.exists():
   source.parent.mkdir(parents=True,exist_ok=True)
   with tarfile.open(archive) as tar: tar.extractall(source.parent,filter='data')
 for name,source in sources.items():
  pin=pins[name];source_tree(base/'downloads'/pin['url'].rsplit('/',1)[-1],source)
 build=base/'build'/a.profile;build.mkdir(parents=True,exist_ok=True)
 sodium=build/'sodium';sodium.mkdir(exist_ok=True)
 run([sources['libsodium']/'configure','--prefix='+str(prefix),'--disable-shared','--enable-static','--enable-minimal','--disable-dependency-tracking'],sodium,env)
 run(['make','-j'+a.jobs],sodium,env);run(['make','check','-j'+a.jobs],sodium,env);run(['make','install'],sodium,env)
 archive=build/'archive';zroot=ROOT/'.toolchains/http/prefix/linux-aws-lc-5.12.0'/a.profile
 options=['-DENABLE_'+x+'=OFF' for x in ['WERROR','MBEDTLS','NETTLE','OPENSSL','MD','LIBB2','LZ4','LZO','LZMA','ZSTD','BZip2','LIBXML2','BSDXML','EXPAT','WIN32_XMLLITE','PCREPOSIX','PCRE2POSIX','LIBGCC','CNG','TAR','CPIO','CAT','UNZIP','XATTR','ACL','ICONV','TEST']]
 run(['cmake','-S',sources['libarchive'],'-B',archive,'-GUnix Makefiles','-DCMAKE_INSTALL_PREFIX='+str(prefix),'-DCMAKE_INSTALL_LIBDIR=lib','-DBUILD_SHARED_LIBS=OFF','-DENABLE_ZLIB=ON','-DZLIB_INCLUDE_DIR='+str(zroot/'include'),'-DZLIB_LIBRARY='+str(zroot/'lib/libz.a'),'-DCMAKE_C_COMPILER='+str(cc),'-DCMAKE_C_FLAGS='+flags,'-DCMAKE_BUILD_TYPE='+('Debug' if a.profile=='Debug' else 'Release'),*options],ROOT)
 run(['cmake','--build',archive,'-j',a.jobs],ROOT);run(['cmake','--install',archive],ROOT)
 files={str(f.relative_to(prefix)):digest(f) for f in sorted(prefix.rglob('*')) if f.is_file()}
 receipt={'pins':pins,'profile':a.profile,'compiler':str(cc),'flags':flags,'zlib':str(zroot/'lib/libz.a'),'zlib_sha256':digest(zroot/'lib/libz.a'),'files':files}
 (prefix/'VOID-HOST-DEPENDENCIES.json').write_text(json.dumps(receipt,indent=2)+'\n')
 notices=ROOT/'third_party/host-licenses';notices.mkdir(exist_ok=True)
 for name,source in sources.items():
  dest=notices/name;dest.mkdir(exist_ok=True)
  shutil.copy(source/('LICENSE' if name=='libsodium' else 'COPYING'),dest)
 # Preserve controlling per-file terms for the exact provider source closure.
 for name,source in sources.items():
  records={str(f.relative_to(source)):f.read_text(errors='replace').split('*/',1)[0]+'*/' for f in sorted((source/('src/libsodium' if name=='libsodium' else 'libarchive')).rglob('*')) if f.suffix in ('.c','.h')}
  (notices/name/'SOURCE-NOTICES.json').write_text(json.dumps(records,indent=2)+'\n')
if __name__=='__main__': main()
