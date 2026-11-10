from pathlib import Path
import os,sys,json,subprocess,hashlib,re,time,traceback,shutil
out=Path(r'\\vboxsvr\VM\project-slice3');old=Path.home()/'VoidCI42377496702-Native';root=Path.home()/'VoidSlice3-Corrected-20261108';sdk=old/'.toolchains/llvm-23.1.4';cmake=Path(r'C:\gcc\bin\cmake.exe')
r={'status':'running','platform':sys.platform,'scope':'Final LSP constructor correction: rebuild/relink LSP main only, verify/reuse qualified frontend/Runtime/provider objects, execute project and legacy LSP suites in both profiles','commands':[]}
env=dict(os.environ,VOID_CLANG=str(sdk/'bin/clang.exe'),TEMP=str(root/'native-private-temp'),TMP=str(root/'native-private-temp'));env['PATH']=str(sdk/'bin')+os.pathsep+env['PATH']
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save():(out/'final-server-native-results.json').write_text(json.dumps(r,indent=2)+'\n')
def run(label,args,cwd=root):
 r['stage']=label;save();start=time.monotonic()
 with (out/('server-final-'+label+'.log')).open('wb') as log:p=subprocess.run(list(map(str,args)),env=env,cwd=cwd,stdout=log,stderr=subprocess.STDOUT,timeout=240)
 r['commands'].append({'label':label,'argv':list(map(str,args)),'cwd':str(cwd),'returncode':p.returncode,'seconds':time.monotonic()-start});save();assert p.returncode==0,(label,p.returncode)
try:
 prior=json.loads((out/'final-native-results.json').read_text());assert prior['status']=='PASS';r['prior_native_receipt']='final-native-results.json'
 expected=json.loads((out/'source-hashes.json').read_text());before=json.loads((out/'before-final-server-source-hashes.json').read_text());changed=[n for n in expected if expected[n]!=before[n]];assert set(changed)=={'src/lsp/main.cpp','tools/lsp_client.py','tests/project/protocol.py'}
 for n in changed:shutil.copyfile(out/('server-final-'+n.replace('/','_')),root/n)
 for n,h in expected.items():assert sha(root/n)==h,n
 r['source_inputs_sha256']=expected;r['changed_since_main_qualification']=changed;save()
 for profile in ['Release','Debug']:
  b=old/'build'/profile;dest=root/'build'/profile
  for field in ['_new_binary_sha256','_baseline_binary_sha256']:
   for p,h in prior[profile+field].items():assert sha(p)==h,p
  flags=['/nologo','/std:c++20','/EHsc','/MT','/GR-','/W4','/permissive-','/D_ITERATOR_DEBUG_LEVEL=0','/DWIN32_LEAN_AND_MEAN','/DNOMINMAX','/D_WIN32_WINNT=0x0A00','/I'+str(root/'include'),'/imsvc'+str(sdk/'include')]+(['/O2','/DNDEBUG'] if profile=='Release' else ['/Od','/Zi'])
  run(profile+'-compile',[sdk/'bin/clang-cl.exe',*flags,'/c',root/'src/lsp/main.cpp','/Fo'+str(dest/'lsp-final.obj'),'/Fd'+str(dest/'lsp-final.pdb')])
  server=b/'voidls-slice3-final.exe';assert not server.exists()
  rsp=(dest/'lsp-link.rsp').read_text().replace(str(dest/'lsp.obj'),str(dest/'lsp-final.obj')).replace(str(b/'voidls-slice3.exe'),str(server)).replace(str(dest/'lsp-link.pdb'),str(dest/'lsp-final-link.pdb'));target=dest/'lsp-final-link.rsp';target.write_text(rsp);shutil.copyfile(target,out/(profile+'-final-server.rsp'))
  cache=(out/(profile+'-CMakeCache.txt')).read_text();rc=re.search(r'^CMAKE_RC_COMPILER:FILEPATH=(.*)$',cache,re.M)[1];work=dest/'lsp-final-link';work.mkdir()
  run(profile+'-link',[cmake,'-E','vs_link_exe','--msvc-ver=1944','--intdir='+str(work),'--rc='+rc,'--mt='+str(sdk/'bin/llvm-mt.exe'),'--manifests',old/'cmake/LongPathAware.manifest','--',sdk/'bin/lld-link.exe','@'+str(target)],b)
  r[profile+'_server_sha256']=sha(server);save()
  run(profile+'-project-protocol',[sys.executable,root/'tests/project/protocol.py',server,out/(profile+'-final-project-protocol.json')])
  for name in ['protocol','stage7','stage8','sprint2']:
   args=[sys.executable,root/('tests/lsp/'+name+'.py'),server]
   if name=='protocol':args.append(dest/'final-legacy-lsp-fixtures')
   if name=='stage8':args.append(b/'voidc-slice3.exe')
   run(profile+'-legacy-'+name,args)
 r['status']='PASS'
except BaseException:r['status']='FAIL';r['error']=traceback.format_exc()
finally:save()
