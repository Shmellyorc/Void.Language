"""Rebuild only the changed test driver; preserve accepted Runtime and old builds."""
from pathlib import Path
import hashlib,json,shutil,subprocess,sys,time
root=Path(__file__).resolve().parents[4];base=Path(__file__).resolve().parent
name=sys.argv[1];assert name in ('debug','san');old=root/'build'/('step42-ci97-'+name);new=root/'build'/('step42-ci98-'+name)
new.mkdir(exist_ok=True);sdk=root/'.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64/bin'
receipt=json.loads((root/'docs/stage10/engineering/ci97'/(name+'-final-binary-hashes.json')).read_text())
assert all(hashlib.sha256((root/p).read_bytes()).hexdigest()==h for p,h in receipt.items())
for p in old.glob('void_runtime_console*'):shutil.copy2(p,new/p.name)
flags=['-std=c++20','-g','-fno-rtti','-Wall','-Wextra','-Wpedantic','-I'+str(root/'include')]
if name=='san':flags+=['-O2','-DNDEBUG','-fsanitize=address,undefined','-fno-omit-frame-pointer']
command=[str(sdk/'clang++'),*flags,str(root/'tests/runtime/console_transfers.cpp'),str(old/'libvoid_runtime.a'),str(old/'libvoid_native_fixture.a'),'-pthread','-ldl','--ld-path='+str(sdk/'ld.lld'),'-o',str(new/'void_runtime_console_transfers')]
start=time.monotonic()
with (base/(name+'-build.log')).open('wb') as log:r=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT)
(base/(name+'-build.json')).write_text(json.dumps(dict(command=command,returncode=r.returncode,seconds=time.monotonic()-start,scope='Changed transfer test driver only; unchanged accepted Runtime archive and neighboring test binaries'),indent=2)+'\n')
print(name,'build',r.returncode,flush=True);raise SystemExit(r.returncode)
