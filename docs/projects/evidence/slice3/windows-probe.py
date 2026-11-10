from pathlib import Path
import json,platform,hashlib,shutil
out=Path(r'\\vboxsvr\VM\project-slice3');old=Path.home()/'VoidCI42377496702-Native';sdk=old/'.toolchains/llvm-23.1.4'
r={'platform':platform.platform(),'files':{},'source_sha256':{}}
for profile in ['Release','Debug']:
 b=old/'build'/profile
 for n in ['build.ninja','CMakeCache.txt']:
  shutil.copyfile(b/n,out/(profile+'-'+n))
 for n in ['void_lsp.lib','void_llvm.lib','void_core.lib','voidls.exe','voidc.exe']:
  p=b/n;r['files'][str(p)]={'exists':p.exists(),'sha256':hashlib.sha256(p.read_bytes()).hexdigest() if p.exists() else None}
for n in ['llvm-objdump.exe','clang-cl.exe','lld-link.exe']:r['files'][str(sdk/'bin'/n)]=(sdk/'bin'/n).exists()
(out/'probe.json').write_text(json.dumps(r,indent=2)+'\n')
