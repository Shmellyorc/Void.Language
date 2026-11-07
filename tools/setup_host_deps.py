#!/usr/bin/env python3
"""Build pinned static host providers with the approved libsodium backport."""
import argparse, json, os, pathlib, platform, subprocess, sys, tarfile, time, urllib.request
from verify_host_deps import (SCHEMA, sha, source_tree, prepare_sodium, verify_sodium_copy,
                              recipe_hashes, toolchain, upstream_results, build_identity)
ROOT=pathlib.Path(__file__).resolve().parents[1]
def main():
 if platform.system() in ('Windows','Darwin'):
  raise SystemExit(subprocess.call([sys.executable,str(ROOT/'tools/setup_host_deps_native.py'),*sys.argv[1:]]))
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--profile',choices=['Debug','Release','strict'],required=True);p.add_argument('--jobs',default='3');p.add_argument('--prefix',type=pathlib.Path);p.add_argument('--build-dir',type=pathlib.Path);a=p.parse_args()
 if platform.system()!='Linux' or platform.machine().lower() not in ['x86_64','amd64']:raise SystemExit('Native Linux x64 required')
 base=ROOT/'.toolchains/host';prefix=(a.prefix or base/'prefix'/a.profile).resolve();build=(a.build_dir or base/'build'/a.profile).resolve()
 if prefix.exists() or build.exists():raise SystemExit('Clean provider build required; retain the old prefix/build and select fresh paths')
 evidence=ROOT/'docs/voidup/evidence'/('dependencies-linux-'+a.profile+'-'+str(time.time_ns()));evidence.mkdir(parents=True);build.mkdir(parents=True)
 pins=json.loads((ROOT/'third_party/host-versions.json').read_text());tools=toolchain();cc=pathlib.Path(tools['tools']['clang']['path']);bin=cc.parent
 flags='-g -O0' if a.profile=='Debug' else '-g -O2';link='--ld-path='+str(bin/'ld.lld')
 if a.profile=='strict':flags+=' -fsanitize=address,undefined -fno-omit-frame-pointer';link+=' -fsanitize=address,undefined'
 env=dict(os.environ,CC=str(cc),AR=str(bin/'llvm-ar'),RANLIB=str(bin/'llvm-ranlib'),LD=str(bin/'ld.lld'),CFLAGS=flags,LDFLAGS=link)
 if a.profile=='strict':env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1:exitcode=87',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1:exitcode=86',ASAN_SYMBOLIZER_PATH=str(bin/'llvm-symbolizer'))
 report={'schema':SCHEMA,'platform':'Linux','architecture':'x86_64','profile':a.profile,'pins':pins,'toolchain':tools,'compiler':str(cc),'compiler_sha256':sha(cc),'recipe_sha256':recipe_hashes(),'flags':flags,'link_flags':link,'commands':[],'build_evidence':str(evidence)}
 def run(args,label,cwd=build):
  args=list(map(str,args));start=time.monotonic()
  with (evidence/(label+'.log')).open('wb') as log:r=subprocess.run(args,cwd=cwd,env=env,stdout=log,stderr=subprocess.STDOUT)
  report['commands'].append({'command':args,'cwd':str(cwd),'status':r.returncode,'seconds':time.monotonic()-start});(evidence/'commands.json').write_text(json.dumps(report,indent=2)+'\n')
  if r.returncode:raise SystemExit(label+' failed; retained evidence: '+str(evidence))
 sources={}
 for name in ['libsodium','libarchive']:
  pin=pins[name];archive=base/'downloads'/pin['url'].rsplit('/',1)[-1];archive.parent.mkdir(parents=True,exist_ok=True)
  if not archive.exists():
   with urllib.request.urlopen(pin['url']) as response:archive.write_bytes(response.read())
  if sha(archive)!=pin['sha256']:raise SystemExit('Source archive hash mismatch: '+name)
  source=base/'sources'/(name+'-'+pin['version'])
  if not source.exists():
   source.parent.mkdir(parents=True,exist_ok=True)
   with tarfile.open(archive) as package:package.extractall(source.parent,filter='data')
  source_tree(archive,source);sources[name]=source
 sodium_source=build/'sodium-source';report['source_provenance']=prepare_sodium(sources['libsodium'],base/'downloads'/pins['libsodium']['url'].rsplit('/',1)[-1],sodium_source)
 sodium=build/'sodium';sodium.mkdir()
 run([sodium_source/'configure','--prefix='+str(prefix),'--disable-shared','--enable-static','--enable-minimal','--disable-dependency-tracking'],'sodium-configure',sodium)
 run(['make','-j'+a.jobs],'sodium-build',sodium)
 try:run(['make','check','-j'+a.jobs],'sodium-tests',sodium)
 finally:
  import shutil
  for pattern in ['*.log','*.trs','*.res']:
   for path in (sodium/'test/default').glob(pattern):shutil.copy2(path,evidence/path.name)
 report['upstream_tests']=upstream_results(sodium);verify_sodium_copy(sodium_source,report['source_provenance'])
 run(['make','install'],'sodium-install',sodium)
 archive=build/'archive';zroot=ROOT/'.toolchains/http/prefix/linux-aws-lc-5.12.0'/a.profile
 run([sys.executable,ROOT/'tools/verify_http_dependencies.py',zroot,a.profile],'http-closure')
 options=['-DENABLE_'+x+'=OFF' for x in ['WERROR','MBEDTLS','NETTLE','OPENSSL','MD','LIBB2','LZ4','LZO','LZMA','ZSTD','BZip2','LIBXML2','BSDXML','EXPAT','WIN32_XMLLITE','PCREPOSIX','PCRE2POSIX','LIBGCC','CNG','TAR','CPIO','CAT','UNZIP','XATTR','ACL','ICONV','TEST']]
 run(['cmake','-S',sources['libarchive'],'-B',archive,'-GUnix Makefiles','-DCMAKE_INSTALL_PREFIX='+str(prefix),'-DCMAKE_INSTALL_LIBDIR=lib','-DBUILD_SHARED_LIBS=OFF','-DENABLE_ZLIB=ON','-DZLIB_INCLUDE_DIR='+str(zroot/'include'),'-DZLIB_LIBRARY='+str(zroot/'lib/libz.a'),'-DCMAKE_C_COMPILER='+str(cc),'-DCMAKE_AR='+str(bin/'llvm-ar'),'-DCMAKE_RANLIB='+str(bin/'llvm-ranlib'),'-DCMAKE_C_FLAGS='+flags,'-DCMAKE_EXE_LINKER_FLAGS='+link,'-DCMAKE_BUILD_TYPE='+('Debug' if a.profile=='Debug' else 'Release'),*options],'archive-configure')
 run(['cmake','--build',archive,'-j',a.jobs],'archive-build');run(['cmake','--install',archive],'archive-install')
 disabled=['LZMA','ZSTD','BZip2','OPENSSL','LIBB2','LIBXML2','EXPAT','ICONV'];cache=(archive/'CMakeCache.txt').read_text()
 if any('ENABLE_'+name+':BOOL=OFF' not in cache for name in disabled):raise SystemExit('Unapproved archive codec configuration')
 report.update(disabled_features=disabled,archive_cache_sha256=sha(archive/'CMakeCache.txt'),zlib=str(zroot/'lib/libz.a'),zlib_sha256=sha(zroot/'lib/libz.a'),files={p.relative_to(prefix).as_posix():sha(p) for p in sorted(prefix.rglob('*')) if p.is_file()})
 report['build_identity']=build_identity(report)
 (prefix/'VOID-HOST-DEPENDENCIES.json').write_text(json.dumps(report,indent=2)+'\n');(evidence/'commands.json').write_text(json.dumps(report,indent=2)+'\n')
 print(prefix)
if __name__=='__main__':main()
