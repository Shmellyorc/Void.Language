#!/usr/bin/env python3
"""Pinned native Windows/macOS host providers. No ambient provider fallback."""
import argparse, hashlib, json, os, pathlib, platform, shutil, subprocess, sys, tarfile, time, urllib.request
from verify_host_deps import source_tree, SCHEMA, prepare_sodium, verify_sodium_copy, recipe_hashes, toolchain, upstream_results, build_identity
ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/ci'))
from llvm_sdk import windows_clangcl
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def verify_static_crt(directives):
 directives=directives.lower()
 # C objects use DEFAULTLIB, unlike C++'s optional RuntimeLibrary mismatch tag.
 if '/defaultlib:libcmt' not in directives or any(value in directives for value in ['md_dynamic','mtd_staticdebug','/defaultlib:msvcrt','/defaultlib:libcmtd']):
  raise ValueError('libsodium static /MT ABI not established by COFF directives')
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--profile',choices=['Release','Debug','strict'],required=True);p.add_argument('--jobs',type=int,default=4);p.add_argument('--prefix',type=pathlib.Path);p.add_argument('--build-dir',type=pathlib.Path);a=p.parse_args()
 system=platform.system();arch={'amd64':'x86_64','aarch64':'arm64'}.get(platform.machine().lower(),platform.machine().lower())
 if (system,arch) not in [('Windows','x86_64'),('Darwin','arm64')]:raise SystemExit('Native Windows x64 or macOS ARM64 required; no ABI substitution')
 base=ROOT/'.toolchains/host';bundle=('windows-' if system=='Windows' else 'macos-')+arch
 prefix=(a.prefix or base/'prefix'/bundle/a.profile).resolve();build=(a.build_dir or base/'build'/bundle/a.profile).resolve()
 if prefix.exists() or build.exists():raise SystemExit('Clean provider build required; retain the old prefix/build and select fresh paths')
 build.mkdir(parents=True)
 evidence=ROOT/'docs/voidup/evidence'/('dependencies-'+bundle+'-'+a.profile+'-'+str(time.time_ns()));evidence.mkdir(parents=True)
 pins=json.loads((ROOT/'third_party/host-versions.json').read_text());sources={}
 report={'schema':SCHEMA,'toolchain':toolchain(),'platform':system,'architecture':arch,'profile':a.profile,'pins':pins,'commands':[],
  'recipe_sha256':recipe_hashes()}
 def run(args,name,cwd=build,env=None):
  args=list(map(str,args));start=time.monotonic()
  with (evidence/(name+'.log')).open('wb') as log:r=subprocess.run(args,cwd=cwd,env=env,stdout=log,stderr=subprocess.STDOUT)
  report['commands'].append({'command':args,'cwd':str(cwd),'status':r.returncode,'seconds':time.monotonic()-start})
  (evidence/'commands.json').write_text(json.dumps(report,indent=2)+'\n')
  if r.returncode:raise SystemExit(name+' failed: '+str(evidence))
 for name in ['libsodium','libarchive']:
  pin=pins[name]
  archive=base/'downloads'/pin['url'].rsplit('/',1)[-1];archive.parent.mkdir(parents=True,exist_ok=True)
  if not archive.exists():
   with urllib.request.urlopen(pin['url']) as response:archive.write_bytes(response.read())
  if sha(archive)!=pin['sha256']:raise SystemExit('Pinned source archive hash mismatch: '+name)
  source=base/'sources'/(name+'-'+pin['version'])
  if not source.exists():
   source.parent.mkdir(parents=True,exist_ok=True)
   with tarfile.open(archive) as tar:tar.extractall(source.parent,filter='data')
  source_tree(archive,source);sources[name]=source
 http_bundle='windows-schannel-x86_64' if system=='Windows' else 'macos-aws-lc-5.12.0-arm64'
 http=ROOT/'.toolchains/http/prefix'/http_bundle/a.profile
 run([os.sys.executable,ROOT/'tools/verify_http_dependencies.py',http,a.profile,system,arch],'http-closure')
 suffix='.exe' if system=='Windows' else '';cc=ROOT/'.toolchains/llvm-23.1.4/bin'/('clang-cl.exe' if system=='Windows' else 'clang')
 report['compiler']=str(cc);report['compiler_sha256']=sha(cc);run([cc,'--version'],'compiler')
 options=['-DENABLE_'+x+'=OFF' for x in ['WERROR','MBEDTLS','NETTLE','OPENSSL','MD','LIBB2','LZ4','LZO','LZMA','ZSTD','BZip2','LIBXML2','BSDXML','EXPAT','WIN32_XMLLITE','PCREPOSIX','PCRE2POSIX','LIBGCC','CNG','TAR','CPIO','CAT','UNZIP','XATTR','ACL','ICONV','TEST']]
 common=['-G','Ninja','-DBUILD_SHARED_LIBS=OFF','-DCMAKE_INSTALL_LIBDIR=lib','-DCMAKE_INSTALL_PREFIX='+str(prefix),'-DCMAKE_C_COMPILER='+str(cc),'-DCMAKE_BUILD_TYPE='+('Release' if a.profile=='Release' else 'Debug')]
 sodium_source=build/'sodium-source';report['source_provenance']=prepare_sodium(sources['libsodium'],base/'downloads'/pins['libsodium']['url'].rsplit('/',1)[-1],sodium_source)
 if system=='Windows':
  if os.environ.get('VSCMD_ARG_TGT_ARCH','').lower() not in ['x64','amd64']:raise SystemExit('Use a native x64 Visual Studio developer environment')
  msbuild=shutil.which('msbuild');
  if not msbuild:raise SystemExit('MSBuild and the Windows SDK are required')
  selected=windows_clangcl(cc.parent.parent);report['clangcl_selection']=selected
  # Upstream MSBuild generates version.h in its source tree. Use an exact
  # disposable copy so the archive-verified source remains unchanged.
  sodium=build/'sodium-source'
  output=build/'sodium-output';intermediate=build/'sodium-objects';output.mkdir(exist_ok=True)
  properties=['/p:Configuration='+('ReleaseLIB' if a.profile=='Release' else 'DebugLIB'),'/p:Platform=x64','/p:PlatformToolset=ClangCL',
   '/p:LLVMInstallDir='+selected['sdk']+os.sep,'/p:LLVMToolsVersion='+selected['LLVMToolsVersion'],
   '/p:CLToolPath='+str(cc.parent)+os.sep,'/p:CLToolExe=clang-cl.exe',
   '/p:LinkToolPath='+str(cc.parent)+os.sep,'/p:LinkToolExe=lld-link.exe',
   '/p:LibToolPath='+str(cc.parent)+os.sep,'/p:LibToolExe=llvm-lib.exe',
   '/p:VoidPinnedResourceDir='+selected['resource'],'/p:VoidPinnedLLVMToolsVersion='+selected['LLVMToolsVersion'],
   '/p:OutDir='+str(output)+os.sep,'/p:IntDir='+str(intermediate)+os.sep,
   '/p:ForceImportBeforeCppTargets='+str(ROOT/'cmake/host-sodium.targets'),'/p:VoidHostStrict='+str(a.profile=='strict').lower()]
  project=sodium/'builds/msvc/vs2022/libsodium/libsodium.vcxproj'
  evaluated=['LLVMToolsVersion','LLVMInstallDir','CLToolPath','CLToolExe','LinkToolPath','LinkToolExe','LibToolPath','LibToolExe',
   'WindowsTargetPlatformVersion','TargetPlatformVersion','WindowsSdkDir','VCToolsInstallDir','VCToolsVersion','IncludePath','LibraryPath']
  run([msbuild,project,*properties,'-getProperty:'+','.join(evaluated)],'sodium-properties')
  actual=json.loads((evidence/'sodium-properties.log').read_text(encoding='utf-8-sig'))['Properties']
  expected={'LLVMToolsVersion':selected['LLVMToolsVersion'],'LLVMInstallDir':selected['sdk']+os.sep,
   'CLToolPath':str(cc.parent)+os.sep,'CLToolExe':'clang-cl.exe','LinkToolPath':str(cc.parent)+os.sep,
   'LinkToolExe':'lld-link.exe','LibToolPath':str(cc.parent)+os.sep,'LibToolExe':'llvm-lib.exe'}
  if any(actual.get(key)!=value for key,value in expected.items()):raise SystemExit('Evaluated MSBuild toolchain mismatch: '+repr(actual))
  # MSBuild resolves the WindowsTargetPlatformVersion "10.0" alias here.
  sdk=pathlib.Path(actual.get('WindowsSdkDir',''));sdk_version=actual.get('TargetPlatformVersion','')
  vc=pathlib.Path(actual.get('VCToolsInstallDir',''))
  if not sdk.is_absolute() or not vc.is_absolute() or not sdk_version or not (sdk/'Include'/sdk_version/'um/Windows.h').is_file() or not (sdk/'Lib'/sdk_version/'um/x64/kernel32.lib').is_file() or not (vc/'include/vcruntime.h').is_file() or not (vc/'lib/x64/libcmt.lib').is_file():
   raise SystemExit('Evaluated native Windows SDK/VC headers and libraries are incomplete: '+repr(actual))
  report['msbuild_properties']=actual
  run([msbuild,project,*properties,'-preprocess:'+str(evidence/'sodium-evaluated.xml')],'sodium-preprocess')
  run([msbuild,project,*properties,'/t:Rebuild','/m:'+str(a.jobs),'/v:normal','/bl:'+str(evidence/'sodium-build.binlog')],'sodium-build')
  run([cc.parent/'llvm-readobj.exe','--coff-directives',output/'libsodium.lib'],'sodium-static-abi')
  verify_static_crt((evidence/'sodium-static-abi.log').read_text())
  verify_sodium_copy(sodium_source,report['source_provenance'],generated_version=True)
  report['upstream_tests']={'runner':'MSBuild','status':'not-run','scope':'Native Windows builds the static provider; upstream Unix 87-test execution is not claimed'}
  (prefix/'lib').mkdir(parents=True,exist_ok=True);shutil.copy2(output/'libsodium.lib',prefix/'lib/libsodium.lib')
  shutil.copytree(sodium/'src/libsodium/include',prefix/'include',dirs_exist_ok=True)
  zlib=http/'lib'/('zs.lib' if a.profile=='Release' else 'zsd.lib')
  flags='/O2' if a.profile=='Release' else '/Od /Zi'
  if a.profile=='strict':flags+=' /clang:-fsanitize=address,undefined /clang:-fno-omit-frame-pointer'
  # libarchive's separate MSVC_USE_STATIC_CRT option selects MTd in Debug.
  # Disable that override and explicitly use the project's MT ABI in all modes.
  common+=['-DCMAKE_LINKER='+str(cc.parent/'lld-link.exe'),'-DCMAKE_AR='+str(cc.parent/'llvm-lib.exe'),'-DCMAKE_POLICY_DEFAULT_CMP0091=NEW','-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded','-DMSVC_USE_STATIC_CRT=OFF','-DCMAKE_C_FLAGS_DEBUG=/Od /Zi /D_DEBUG','-DCMAKE_C_FLAGS='+flags]
 else:
  sdk=os.environ.get('SDKROOT') or subprocess.check_output(['xcrun','--show-sdk-path'],text=True).strip();minimum=os.environ.get('MACOSX_DEPLOYMENT_TARGET','14.0')
  if not pathlib.Path(sdk).is_dir():raise SystemExit('Physical Apple SDK required')
  report.update(sdk=sdk,deployment_target=minimum)
  flags='-g -O2' if a.profile=='Release' else '-g -O0'
  if a.profile=='strict':flags+=' -fsanitize=address,undefined -fno-omit-frame-pointer'
  env=dict(os.environ,CC=str(cc),CFLAGS=flags,LDFLAGS=('-fsanitize=address,undefined' if a.profile=='strict' else ''),SDKROOT=sdk,MACOSX_DEPLOYMENT_TARGET=minimum)
  if a.profile=='strict':env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1:exitcode=87',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1:exitcode=86',ASAN_SYMBOLIZER_PATH=str(cc.parent/'llvm-symbolizer'))
  sodium=build/'sodium';sodium.mkdir(exist_ok=True)
  run([sodium_source/'configure','--prefix='+str(prefix),'--disable-shared','--enable-static','--enable-minimal','--disable-dependency-tracking'],'sodium-configure',sodium,env)
  run(['make','-j'+str(a.jobs)],'sodium-build',sodium,env);run(['make','check','-j'+str(a.jobs)],'sodium-tests',sodium,env)
  report['upstream_tests']=upstream_results(sodium);verify_sodium_copy(sodium_source,report['source_provenance'])
  run(['make','install'],'sodium-install',sodium,env)
  zlib=http/'lib/libz.a';common+=['-DCMAKE_C_FLAGS='+flags,'-DCMAKE_OSX_SYSROOT='+sdk,'-DCMAKE_OSX_DEPLOYMENT_TARGET='+minimum,'-DCMAKE_OSX_ARCHITECTURES=arm64']
 run(['cmake','-S',sources['libarchive'],'-B',build/'archive',*common,'-DENABLE_ZLIB=ON','-DPOSIX_REGEX_LIB=NONE','-DZLIB_INCLUDE_DIR='+(http/'include').as_posix(),'-DZLIB_LIBRARY='+zlib.as_posix(),*options],'archive-configure')
 run(['cmake','--build',build/'archive','--parallel',a.jobs],'archive-build');run(['cmake','--install',build/'archive'],'archive-install')
 cache=(build/'archive/CMakeCache.txt').read_text()
 disabled=['LZMA','ZSTD','BZip2','OPENSSL','LIBB2','LIBXML2','EXPAT','ICONV']+(['CNG'] if system=='Windows' else [])
 for feature in disabled:
  if 'ENABLE_'+feature+':BOOL=OFF' not in cache:raise SystemExit('Unapproved archive feature: '+feature)
 report.update(disabled_features=disabled,archive_cache_sha256=sha(build/'archive/CMakeCache.txt'))
 report.update(zlib=str(zlib),zlib_sha256=sha(zlib),flags=flags,
  files={p.relative_to(prefix).as_posix():sha(p) for folder in ['lib','include'] for p in sorted((prefix/folder).rglob('*')) if p.is_file()})
 report['build_identity']=build_identity(report)
 (prefix/'VOID-HOST-DEPENDENCIES.json').write_text(json.dumps(report,indent=2)+'\n');(evidence/'commands.json').write_text(json.dumps(report,indent=2)+'\n')
 print(prefix)
if __name__=='__main__':main()
