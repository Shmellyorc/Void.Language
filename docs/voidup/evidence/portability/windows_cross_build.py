"""Validation-only MinGW cross build. Never a production/native provider receipt."""
from pathlib import Path
import json,os,subprocess,sys,time
root=Path.cwd();sys.path.insert(0,str(root/'tools'))
from verify_host_deps import source_tree,sha
base=root/'build/voidup-cross';prefix=base/'providers';build=base/'provider-build';build.mkdir(parents=True,exist_ok=True)
evidence=root/'docs/voidup/evidence/portability';record={'scope':'Windows x64 MinGW cross compilation/link only; no native acceptance','commands':[]}
def run(args,name,cwd=build,env=None):
 start=time.monotonic()
 with (evidence/(name+'.log')).open('wb') as log:r=subprocess.run(list(map(str,args)),cwd=cwd,env=env,stdout=log,stderr=subprocess.STDOUT)
 record['commands'].append({'command':list(map(str,args)),'cwd':str(cwd),'status':r.returncode,'seconds':time.monotonic()-start})
 (evidence/'windows-cross-providers.json').write_text(json.dumps(record,indent=2)+'\n')
 if r.returncode:raise SystemExit(name+' failed')
pins=json.loads((root/'third_party/host-versions.json').read_text())
for name in ['libsodium','libarchive']:
 pin=pins[name];source_tree(root/'.toolchains/host/downloads'/pin['url'].rsplit('/',1)[-1],root/'.toolchains/host/sources'/(name+'-'+pin['version']))
sodium=build/'sodium';sodium.mkdir(exist_ok=True)
env=dict(os.environ,CC='x86_64-w64-mingw32-gcc',AR='x86_64-w64-mingw32-ar',RANLIB='x86_64-w64-mingw32-ranlib',CFLAGS='-O2')
run([root/'.toolchains/host/sources/libsodium-1.0.22/configure','--host=x86_64-w64-mingw32','--prefix='+str(prefix),'--enable-static','--disable-shared','--enable-minimal','--disable-dependency-tracking'],'cross-sodium-configure',sodium,env)
run(['make','-j6'],'cross-sodium-build',sodium,env);run(['make','install'],'cross-sodium-install',sodium,env)
common=['-G','Ninja','-DCMAKE_SYSTEM_NAME=Windows','-DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc','-DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres','-DCMAKE_INSTALL_PREFIX='+str(prefix),'-DCMAKE_INSTALL_LIBDIR=lib','-DCMAKE_BUILD_TYPE=Release','-DBUILD_SHARED_LIBS=OFF']
zsource=root/'build/voidup-ubuntu-source/.toolchains/http/build/linux-aws-lc-5.12.0/Release/source/zlib-1.3.2'
run(['cmake','-S',zsource,'-B',build/'zlib',*common,'-DZLIB_BUILD_SHARED=OFF','-DZLIB_BUILD_STATIC=ON','-DZLIB_BUILD_TESTING=OFF'],'cross-zlib-configure')
run(['cmake','--build',build/'zlib','-j6'],'cross-zlib-build');run(['cmake','--install',build/'zlib'],'cross-zlib-install')
zlib=prefix/'lib/libzs.a'
if not zlib.is_file():raise SystemExit('Expected cross zlib archive absent: '+str(zlib))
options=['-DENABLE_'+x+'=OFF' for x in ['WERROR','MBEDTLS','NETTLE','OPENSSL','MD','LIBB2','LZ4','LZO','LZMA','ZSTD','BZip2','LIBXML2','BSDXML','EXPAT','WIN32_XMLLITE','PCREPOSIX','PCRE2POSIX','LIBGCC','CNG','TAR','CPIO','CAT','UNZIP','XATTR','ACL','ICONV','TEST']]
run(['cmake','-S',root/'.toolchains/host/sources/libarchive-3.8.11','-B',build/'archive',*common,'-DENABLE_ZLIB=ON','-DPOSIX_REGEX_LIB=NONE','-DZLIB_INCLUDE_DIR='+str(prefix/'include'),'-DZLIB_LIBRARY='+str(zlib),*options],'cross-archive-configure')
run(['cmake','--build',build/'archive','-j6'],'cross-archive-build');run(['cmake','--install',build/'archive'],'cross-archive-install')
record['files']={p.relative_to(prefix).as_posix():sha(p) for p in prefix.rglob('*') if p.is_file()}
(evidence/'windows-cross-providers.json').write_text(json.dumps(record,indent=2)+'\n')
print('Cross providers built; native MSVC/Windows qualification remains unavailable')
