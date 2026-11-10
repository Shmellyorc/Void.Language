from pathlib import Path
import os,json,subprocess,sys,hashlib,traceback
out=Path(r'\\vboxsvr\VM\project-slice2-final-review');old=Path.home()/'VoidCI42377496702-Native';source=Path.home()/'VoidSlice2-FinalReview-20261108'
r={'status':'running','script_sha256':hashlib.sha256((out/'nested.py').read_bytes()).hexdigest(),'commands':[]}
env=dict(os.environ,VOID_CLANG=str(old/'.toolchains/llvm-23.1.4/bin/clang.exe'),TEMP=str(source/'native-private-temp'),TMP=str(source/'native-private-temp'));env['PATH']=str(old/'.toolchains/llvm-23.1.4/bin')+os.pathsep+env['PATH']
try:
 original=json.loads((out/'native-results.json').read_text());assert original['status']=='FAIL' and original['commands'][-1]['label']=='Debug-nested' and original['commands'][-1]['returncode']==2
 assert all(c['returncode']==0 for c in original['commands'][:-1]);r['original_missing_script_failure']='Preserved in native-results.json and Debug-nested.log'
 assert hashlib.sha256((out/'nested.py').read_bytes()).hexdigest()=='647dad971b57dc880a04c710f4a200fc036dd65477f9ef83dc23e2d86f8fc0f3'
 for profile in ['Release','Debug']:
  exe=old/'build'/profile/'voidc-slice2-final-review.exe';args=[sys.executable,str(out/'nested.py'),str(exe),str(out/(profile+'-nested.json'))]
  with (out/(profile+'-nested-corrective.log')).open('wb') as log:p=subprocess.run(args,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=240)
  r['commands'].append({'argv':args,'returncode':p.returncode,'binary_sha256':hashlib.sha256(exe.read_bytes()).hexdigest()});assert p.returncode==0
 r['status']='PASS'
except BaseException:r['status']='FAIL';r['error']=traceback.format_exc()
(out/'nested-corrective-native-results.json').write_text(json.dumps(r,indent=2)+'\n')
