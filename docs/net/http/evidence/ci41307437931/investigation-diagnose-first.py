from pathlib import Path
import json,shlex,subprocess,shutil,os
root=Path.cwd();work=root/'.toolchains/ci41307437931-ubuntu';build=work/'build';diag=work/'diagnostic';diag.mkdir(exist_ok=True)
source=(root/'src/runtime/url_abi.cpp').read_text().replace('#include <cstring>','#include <cstring>\n#include <cstdio>')
source=source.replace('catch (...) { failure->message = "URL boundary invariant failed"; }','catch (const std::exception& e) { std::fprintf(stderr, "URL DIAGNOSTIC: %s\\n", e.what()); failure->message = "URL boundary invariant failed"; }')
(diag/'url_abi.cpp').write_text(source)
entry=next(e for e in json.loads((build/'compile_commands.json').read_text()) if e['file'].endswith('/src/runtime/url_abi.cpp'))
args=shlex.split(entry['command']);args[args.index('-o')+1]=str(diag/'url_abi.cpp.o');args[args.index('-c')+1]=str(diag/'url_abi.cpp')
subprocess.run(args,cwd=build,check=True)
archive=diag/'libvoid_runtime.a';shutil.copy2(build/'libvoid_runtime.a',archive)
subprocess.run([str(root/'.toolchains/llvm-23.1.4/bin/llvm-ar'),'r',str(archive),str(diag/'url_abi.cpp.o')],check=True)
line=subprocess.check_output(['ninja','-C',str(build),'-t','commands','voidc'],text=True).splitlines()[-1]
args=shlex.split(line)[2:-2];assert args[0].endswith('clang++')
args[args.index('-o')+1]=str(diag/'voidc-diagnostic')
args[args.index('libvoid_runtime.a')]=str(archive)
subprocess.run(args,cwd=build,check=True)
prefix=diag/'relocated 雪 # % installation';(prefix/'bin').mkdir(parents=True,exist_ok=True)
shutil.copytree(root/'stdlib',prefix/'share/void/stdlib',dirs_exist_ok=True)
shutil.copy2(diag/'voidc-diagnostic',prefix/'bin/voidc')
project=diag/'project 雪 # %';project.mkdir(exist_ok=True);shutil.copy2(root/'examples/net_http/api_json.void',project/'main.void')
env=os.environ.copy();env.pop('VOID_STDLIB_ROOT',None);env['VOID_HTTP_URL']='http://127.0.0.1:1/json'
r=subprocess.run([str(prefix/'bin/voidc'),'run',str(project/'main.void')],env=env)
print('Diagnostic exit:',r.returncode)
import tempfile
with tempfile.TemporaryDirectory(prefix='VOID URL diagnostic ') as temporary:
    other=Path(temporary); relocated=other/'relocated 雪 # % installation'; (relocated/'bin').mkdir(parents=True)
    shutil.copytree(root/'stdlib',relocated/'share/void/stdlib')
    project=other/'project 雪 # %'; project.mkdir(); shutil.copy2(root/'examples/net_http/api_json.void',project/'main.void')
    for label,binary in [('original',build/'voidc'),('diagnostic',diag/'voidc-diagnostic')]:
        installed=relocated/'bin/voidc';shutil.copy2(binary,installed)
        for cwd in [root,other]:
            print('CONTROL',label,'cwd',cwd,flush=True)
            r=subprocess.run([str(installed),'run',str(project/'main.void')],env=env,cwd=cwd)
            print('EXIT',r.returncode,flush=True)
subprocess.run(['apt-get','install','-y','--no-install-recommends','cmake'],check=True)
import sys,importlib.util
sys.path.insert(0,str(root/'tests/net/http'))
spec=importlib.util.spec_from_file_location('installation',root/'tests/net/http/install.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
original_run=subprocess.run
copied=False
def run_install(argv,*args,**kwargs):
    global copied
    if str(argv[0]).endswith('/bin/voidc') and not copied:
        shutil.copy2(diag/'voidc-diagnostic',argv[0]);copied=True
    return original_run(argv,*args,**kwargs)
subprocess.run=run_install
sys.argv=['install.py',str(build/'voidc'),'--evidence',str(work/'install-diagnostic')]
module.main()
