from pathlib import Path
import os,json,subprocess,sys,hashlib,traceback
out=Path(r'\\vboxsvr\VM\project-slice2-verified');old=Path.home()/'VoidCI42377496702-Native';source=Path.home()/'VoidSlice2-Verified-20261108'
r={'status':'running','script_sha256':hashlib.sha256((out/'nested.py').read_bytes()).hexdigest(),'commands':[]}
env=dict(os.environ,VOID_CLANG=str(old/'.toolchains/llvm-23.1.4/bin/clang.exe'),TEMP=str(source/'native-private-temp'),TMP=str(source/'native-private-temp'));env['PATH']=str(old/'.toolchains/llvm-23.1.4/bin')+os.pathsep+env['PATH']
try:
 assert json.loads((out/'native-results.json').read_text())['status']=='PASS'
 for profile in ['Release','Debug']:
  exe=old/'build'/profile/'voidc-slice2-verified.exe';args=[sys.executable,str(out/'nested.py'),str(exe),str(out/(profile+'-nested.json'))]
  with (out/(profile+'-nested.log')).open('wb') as log:p=subprocess.run(args,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=240)
  r['commands'].append({'argv':args,'returncode':p.returncode,'binary_sha256':hashlib.sha256(exe.read_bytes()).hexdigest()});assert p.returncode==0
 r['status']='PASS'
except BaseException:r['status']='FAIL';r['error']=traceback.format_exc()
(out/'nested-native-results.json').write_text(json.dumps(r,indent=2)+'\n')
