"""Build only changed console Runtime object and focused native test drivers.
Reuse the accepted configuration's other archive members; never alter CI96/Step41
builds. This is a focused native harness, not full CMake/JIT/SDK acceptance.
"""
from pathlib import Path
import hashlib,json,shutil,subprocess,sys,time
root=Path(__file__).resolve().parents[4];base=Path(__file__).resolve().parent
name=sys.argv[1];assert name in ('debug','san')
sdk=root/'.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64'
old=root/'build'/('step42-ci96-'+name);new=root/'build'/('step42-ci97-'+name);new.mkdir(exist_ok=True)
flags=['-std=c++20','-fno-rtti','-Wall','-Wextra','-Wpedantic','-I'+str(root/'include'),'-g']
flags+=['-O2','-DNDEBUG','-fsanitize=address,undefined','-fno-omit-frame-pointer'] if name=='san' else []
records=[]
def run(label,args):
 start=time.monotonic()
 with (base/(name+'-'+label+'.log')).open('wb') as log:r=subprocess.run(list(map(str,args)),cwd=root,stdout=log,stderr=subprocess.STDOUT)
 records.append(dict(label=label,command=list(map(str,args)),returncode=r.returncode,seconds=time.monotonic()-start))
 (base/(name+'-build-commands.json')).write_text(json.dumps(records,indent=2)+'\n')
 print(name,label,r.returncode,flush=True)
 if r.returncode:raise SystemExit(r.returncode)
for library in ('libvoid_runtime.a','libvoid_native_fixture.a'):shutil.copy2(old/library,new/library)
run('runtime-object',[sdk/'bin/clang++',*flags,'-c',root/'src/runtime/console.cpp','-o',new/'console.cpp.o'])
run('runtime-archive',[sdk/'bin/llvm-ar','r',new/'libvoid_runtime.a',new/'console.cpp.o'])
for stem in ('console','console_transfers','console_input','console_keyboard','console_terminal','console_wide'):
 run(stem,[sdk/'bin/clang++',*flags,root/'tests/runtime'/(stem+'.cpp'),new/'libvoid_runtime.a',new/'libvoid_native_fixture.a','-pthread','-ldl','--ld-path='+str(sdk/'bin/ld.lld'),'-o',new/('void_runtime_'+stem)])
(base/(name+'-binary-hashes.json')).write_text(json.dumps({str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in new.iterdir() if p.is_file()},indent=2)+'\n')
