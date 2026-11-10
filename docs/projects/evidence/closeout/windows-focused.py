from pathlib import Path
import os,json,subprocess,time,traceback,hashlib,re,platform,sys
out=Path(r'\\vboxsvr\VM\project-slice1-closeout');old=Path.home()/'VoidCI42377496702-Native';source=Path.home()/'VoidSlice1Closeout-20261108-r2';sdk=old/'.toolchains/llvm-23.1.4';r={'status':'running','scope':'New parser and isolated header compiled from exact current inputs, linked with preexisting baseline void_core/LLVM dependency closure; not a full current-tree native compiler rebuild','platform':platform.platform(),'commands':[]}
env=dict(os.environ,VOID_CLANG=str(sdk/'bin/clang.exe'));env['PATH']=str(sdk/'bin')+os.pathsep+env['PATH'];env['TEMP']=env['TMP']=str(source/'native-private-temp')
def save():(out/'focused-results.json').write_text(json.dumps(r,indent=2)+'\n')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(label,args,cwd=source):
 r['stage']=label;save();start=time.monotonic()
 with (out/(label+'.log')).open('wb') as f:p=subprocess.run(list(map(str,args)),cwd=cwd,env=env,stdout=f,stderr=subprocess.STDOUT)
 r['commands'].append({'label':label,'argv':list(map(str,args)),'cwd':str(cwd),'returncode':p.returncode,'seconds':time.monotonic()-start});save();assert p.returncode==0,(label,p.returncode)
def expand(value):
 return re.sub(r'\$([ $:])',lambda m:m[1],value)
try:
 expected=json.loads((out/'source-hashes.json').read_text())
 names=['src/frontend/project.cpp','include/void/project.hpp','tests/project/manifest.cpp','tests/project/header.cpp','third_party/tomlplusplus/toml.hpp','third_party/tomlplusplus/void-bounds-and-locations.patch','third_party/tomlplusplus/upstream.json','third_party/tomlplusplus/LICENSE']
 r['current_inputs_sha256']={n:sha(source/n) for n in names}
 for n,h in r['current_inputs_sha256'].items():assert h==expected[n],n
 # No dependency verifier, receipt or original native source is changed.
 for profile in ['Release','Debug']:
  b=old/'build'/profile;dest=source/'focused'/profile;dest.mkdir(parents=True,exist_ok=True)
  run(profile+'-http-verified',[sys.executable,old/'tools/verify_http_dependencies.py',old/'.toolchains/http/prefix/windows-schannel-x86_64'/profile,profile,'Windows','x86_64'],old)
  run(profile+'-host-verified',[sys.executable,old/'tools/verify_host_deps.py','--prefix',old/'.toolchains/host/prefix/windows-x86_64'/profile,'--profile',profile,'--platform','Windows','--architecture','x86_64'],old)
  flags=['/nologo','/std:c++20','/EHsc','/MT','/GR-','/W4','/permissive-','/D_ITERATOR_DEBUG_LEVEL=0','/DWIN32_LEAN_AND_MEAN','/DNOMINMAX','/I'+str(source/'include'),'/I'+str(source/'third_party/tomlplusplus')]
  flags+=['/O2','/DNDEBUG'] if profile=='Release' else ['/Od','/Zi']
  run(profile+'-header-build',[sdk/'bin/clang-cl.exe',*flags,source/'tests/project/header.cpp','/Fe'+str(dest/'header.exe'),'/Fo'+str(dest/'header.obj'),'/Fd'+str(dest/'header.pdb'),'/link','/INCREMENTAL:NO'])
  run(profile+'-header',[dest/'header.exe'])
  for name,path in [('project','src/frontend/project.cpp'),('manifest','tests/project/manifest.cpp')]:
   run(profile+'-'+name+'-compile',[sdk/'bin/clang-cl.exe',*flags,'/c',source/path,'/Fo'+str(dest/(name+'.obj')),'/Fd'+str(dest/(name+'.pdb'))])
  ninja=(b/'build.ninja').read_text();block=re.search(r'^build voidc\.exe:.*?(?=^#|^build |\Z)',ninja,re.M|re.S)[0]
  libraries=expand(re.search(r'^  LINK_LIBRARIES = (.*)$',block,re.M)[1])
  rsp=dest/'manifest-link.rsp';rsp.write_text('"'+str(dest/'project.obj')+'" "'+str(dest/'manifest.obj')+'" '+libraries+' /out:"'+str(dest/'manifest.exe')+'" /subsystem:console /machine:x64 /INCREMENTAL:NO'+(' /debug' if profile=='Debug' else ''))
  r[profile+'_baseline_inputs_sha256']={'void_core.lib':sha(b/'void_core.lib'),'build.ninja':sha(b/'build.ninja')};save()
  run(profile+'-manifest-link',[sdk/'bin/lld-link.exe','@'+str(rsp)],b)
  run(profile+'-manifest',[dest/'manifest.exe'])
  r[profile+'_binaries_sha256']={n:sha(dest/n) for n in ['header.exe','manifest.exe']};save()
 run('provenance',[sys.executable,source/'tests/project/provenance.py',source])
 r['status']='PASS'
except BaseException:r['status']='FAIL';r['error']=traceback.format_exc()
finally:save()
