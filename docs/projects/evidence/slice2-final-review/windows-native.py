from pathlib import Path
import os,sys,json,subprocess,time,traceback,zipfile,hashlib,re,platform,shutil
out=Path(r'\\vboxsvr\VM\project-slice2-final-review');old=Path.home()/'VoidCI42377496702-Native';root=Path.home()/'VoidSlice2-FinalReview-20261108';sdk=old/'.toolchains/llvm-23.1.4';cmake=Path(r'C:\gcc\bin\cmake.exe')
r={'status':'running','platform':platform.platform(),'scope':'Native incremental relink: all changed compiler/main/manifest translation units rebuilt; unchanged core/backend/provider objects and resources retained and inventoried. No full clean CMake or Windows Server qualification.','commands':[]}
env=dict(os.environ,VOID_CLANG=str(sdk/'bin/clang.exe'));env['PATH']=str(sdk/'bin')+os.pathsep+env['PATH']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def save():(out/'native-results.json').write_text(json.dumps(r,indent=2)+'\n')
def run(label,args,cwd=root):
 r['stage']=label;save();start=time.monotonic()
 with (out/(label+'.log')).open('wb') as f:p=subprocess.run(list(map(str,args)),cwd=cwd,env=env,stdout=f,stderr=subprocess.STDOUT)
 r['commands'].append({'label':label,'argv':list(map(str,args)),'cwd':str(cwd),'returncode':p.returncode,'seconds':time.monotonic()-start});save();assert p.returncode==0,(label,p.returncode)
def expand(s):return re.sub(r'\$([ $:])',lambda m:m[1],s)
try:
 assert not root.exists(),'Fresh native source/output directory required'
 root.mkdir();(root/'native-private-temp').mkdir();env['TEMP']=env['TMP']=str(root/'native-private-temp')
 with zipfile.ZipFile(out/'source.zip') as z:z.extractall(root)
 expected=json.loads((out/'source-hashes.json').read_text())
 for n,h in expected.items():assert sha(root/n)==h,n
 r['source_inputs_sha256']=expected;r['source_archive_sha256']=sha(out/'source.zip')
 baseline=json.loads((out/'probe.json').read_text())['baseline_source_sha256'];r['baseline_native_source_sha256']=baseline
 for n,h in baseline.items():assert sha(old/n)==h,('Baseline source changed since probe',n)
 changed=[n for n in baseline if (root/n).exists() and sha(root/n)!=baseline[n]]
 assert set(changed)=={'src/main.cpp','src/frontend/compiler.cpp','include/void/diagnostic.hpp','include/void/frontend.hpp','include/void/path.hpp'},changed
 r['changed_native_baseline_sources']=changed;save()
 for profile in ['Release','Debug']:
  b=old/'build'/profile;dest=root/'build'/profile;dest.mkdir(parents=True)
  run(profile+'-http-verified',[sys.executable,old/'tools/verify_http_dependencies.py',old/'.toolchains/http/prefix/windows-schannel-x86_64'/profile,profile,'Windows','x86_64'],old)
  run(profile+'-host-verified',[sys.executable,old/'tools/verify_host_deps.py','--prefix',old/'.toolchains/host/prefix/windows-x86_64'/profile,'--profile',profile,'--platform','Windows','--architecture','x86_64'],old)
  flags=['/nologo','/std:c++20','/EHsc','/MT','/GR-','/W4','/permissive-','/D_ITERATOR_DEBUG_LEVEL=0','/DWIN32_LEAN_AND_MEAN','/DNOMINMAX','/D_WIN32_WINNT=0x0A00','/DVOID_COMPILER_VERSION="0.1.0"','/I'+str(root/'include'),'/I'+str(root/'third_party/tomlplusplus'),'/imsvc'+str(sdk/'include')]
  flags+=['/O2','/DNDEBUG'] if profile=='Release' else ['/Od','/Zi']
  for name,path in [('compiler','src/frontend/compiler.cpp'),('project','src/frontend/project.cpp'),('main','src/main.cpp'),('selection','tests/project/selection.cpp')]:
   run(profile+'-'+name+'-compile',[sdk/'bin/clang-cl.exe',*flags,'/c',root/path,'/Fo'+str(dest/(name+'.obj')),'/Fd'+str(dest/(name+'.pdb'))])
  inputs=(b/('cache-inputs-'+profile+'.txt')).read_text().splitlines();inputs=[str(dest/'main.obj') if 'voidc_objects.dir' in s and s.replace('\\','/').endswith('src/main.cpp.obj') else s for s in inputs]
  inputs += [str(dest/'compiler.obj'),str(dest/'project.obj')]
  identity_inputs=dest/'cache-inputs.txt';identity_inputs.write_text('\n'.join(inputs)+'\n')
  run(profile+'-identity-generate',[cmake,'-DINPUTS='+str(identity_inputs),'-DOUTPUT='+str(dest/'cache-identity.cpp'),'-P',root/'cmake/CacheBuildIdentity.cmake'])
  run(profile+'-identity-compile',[sdk/'bin/clang-cl.exe',*flags,'/c',dest/'cache-identity.cpp','/Fo'+str(dest/'identity.obj')])
  ninja=(b/'build.ninja').read_text();block=re.search(r'^build voidc\.exe:.*?(?=^#|^build |\Z)',ninja,re.M|re.S)[0];libraries=expand(re.search(r'^  LINK_LIBRARIES = (.*)$',block,re.M)[1]).replace('void_cache_identity.lib','')
  objects=[dest/'main.obj',dest/'compiler.obj',dest/'project.obj',dest/'identity.obj',b/'CMakeFiles/voidc_objects.dir/src/native_build.cpp.obj',b/'CMakeFiles/voidc_objects.dir/examples/proof.cpp.obj']
  exe=b/'voidc-slice2-final-review.exe';assert not exe.exists(), 'Do not overwrite a prior native candidate'
  def link(label,objects,target):
   rsp=dest/(label+'.rsp');rsp.write_text(' '.join('"'+str(p)+'"' for p in objects)+' '+libraries+' /out:"'+str(target)+'" /subsystem:console /machine:x64 /INCREMENTAL:NO'+(' /debug /pdb:"'+str(dest/(label+'.pdb'))+'"' if profile=='Debug' else ''))
   shutil.copyfile(rsp,out/(profile+'-'+label+'.rsp'))
   cache=(out/(profile+'-CMakeCache.txt')).read_text();rc=re.search(r'^CMAKE_RC_COMPILER:FILEPATH=(.*)$',cache,re.M)[1]
   run(profile+'-'+label,[cmake,'-E','vs_link_exe','--msvc-ver=1944','--intdir='+str(dest),'--rc='+rc,'--mt='+str(sdk/'bin/llvm-mt.exe'),'--manifests',old/'cmake/LongPathAware.manifest','--',sdk/'bin/lld-link.exe','@'+str(rsp)],b)
  link('cli-link',objects,exe)
  link('selection-link',[dest/'compiler.obj',dest/'project.obj',dest/'selection.obj',dest/'identity.obj'],dest/'selection.exe')
  r[profile+'_baseline_binary_sha256']={str(p):sha(p) for p in [b/'void_core.lib',b/'void_llvm.lib',b/'CMakeFiles/voidc_objects.dir/src/native_build.cpp.obj',b/'CMakeFiles/voidc_objects.dir/examples/proof.cpp.obj',b/'void_runtime.lib',b/'void_native_fixture.lib',b/'void-native.txt',b/'void-native-resources.txt']}
  r[profile+'_new_binary_sha256']={str(p):sha(p) for p in [exe,dest/'selection.exe',dest/'cache-identity.cpp',identity_inputs]};save()
  run(profile+'-selection',[dest/'selection.exe'])
  run(profile+'-project-cli',[sys.executable,root/'tests/project/discovery.py',exe,out/(profile+'-project-cli.json')])
  run(profile+'-legacy',[sys.executable,root/'tests/project/legacy.py',exe])
 run(profile+'-nested',[sys.executable,root/'tests/project/nested.py',exe,out/(profile+'-nested.json')])
 r['status']='PASS'
except BaseException:r['status']='FAIL';r['error']=traceback.format_exc()
finally:save()
