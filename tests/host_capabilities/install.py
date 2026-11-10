from pathlib import Path
import hashlib,json,os,shutil,subprocess,sys,tempfile
import support as h
compiler=Path(sys.argv[1]).resolve();child=Path(sys.argv[2]).resolve();build=compiler.parent
records=[]
def run(args,work,expected=None,env=None):
 r=h.run(args,work,expected,env=env,small=True,sources=(h.ROOT/'tests/host_capabilities/smoke.void',))
 records.append({'command':list(map(str,args)),'stdout':r.stdout.decode(),'stderr':r.stderr.decode()});return r
with tempfile.TemporaryDirectory(prefix='VOID host installed 雪 # % ') as folder:
 work=Path(folder);prefix=work/'original';relocated=work/'relocated'
 r=subprocess.run(['cmake','--install',build,'--prefix',prefix,'--component','compiler'],capture_output=True,check=True)
 records.append({'command':['cmake','--install',str(build),'--component','compiler'],'stdout_sha256':hashlib.sha256(r.stdout).hexdigest()})
 prefix.rename(relocated);tool=relocated/'bin/voidc'
 for name in ['libarchive.a','libsodium.a']:assert (relocated/'lib/void'/name).is_file()
 for name in ['crypto','archive','process','system_error']:assert (relocated/'share/void/stdlib'/(name+'.void')).is_file()
 for name in ['host-versions.json','VOID-HOST-DEPENDENCIES.json','libsodium/LICENSE','libarchive/COPYING','corresponding-source/libsodium-1.0.22.tar.gz','corresponding-source/libarchive-3.8.11.tar.xz']:assert (relocated/'share/void/licenses/host'/name).is_file()
 # Verify the installed source bundle, including all recipe hashes and exact
 # offline reconstruction of the approved source, from outside the checkout.
 sys.path.insert(0,str(h.ROOT/'tools'))
 from verify_host_deps import RECIPES, sha, source_policy
 host=relocated/'share/void/licenses/host';bundle=host/'corresponding-source';repro=bundle/'repro'
 receipt=json.loads((host/'VOID-HOST-DEPENDENCIES.json').read_text())
 assert receipt['schema']==3 and receipt['source_provenance']==source_policy()
 for name in RECIPES:assert sha(repro/name)==receipt['recipe_sha256'][name],name
 for name in ['libsodium','libarchive']:
  pin=receipt['pins'][name];assert sha(bundle/pin['url'].rsplit('/',1)[-1])==pin['sha256']
 assert sha(host/'patches/libsodium-1.0.22/asan-alignment.patch')==receipt['source_provenance']['patch']['sha256']
 reconstructed=work/'reconstructed-source'
 replay=run([sys.executable,repro/'tools/verify_host_deps.py','--archives',bundle,'--reconstruct-source',reconstructed],work)
 assert json.loads(replay.stdout)==receipt['source_provenance']
 for name,digest in receipt['source_provenance']['postimages'].items():assert sha(reconstructed/name)==digest
 # Corrupt a packaged patch and require reconstruction rejection; no real source/prefix changes.
 copied=work/'damaged-bundle';shutil.copytree(bundle,copied)
 patch=copied/'repro/third_party/host-patches/libsodium-1.0.22/asan-alignment.patch';patch.write_bytes(patch.read_bytes()+b'\nchanged\n')
 rejected=subprocess.run([sys.executable,copied/'repro/tools/verify_host_deps.py','--archives',copied,'--reconstruct-source',work/'must-not-reconstruct'],capture_output=True)
 assert rejected.returncode!=0 and b'patch policy mismatch' in rejected.stderr
 fixture=work/'main.void';fixture.write_bytes((h.ROOT/'tests/host_capabilities/smoke.void').read_bytes())
 env=dict(os.environ);env.pop('VOID_STDLIB_ROOT',None)
 for flags in (['--no-opt'],[]):
  for mode,expected in [('args',b'61726773\nargs\n'),('crypto',None)]:run([tool,'run',fixture,*flags,'--no-cache','--',mode],work,expected,env)
  isolated=work/('aot-normal' if flags else 'aot-o2');isolated.mkdir()
  binary=isolated/'application';run([tool,'build',fixture,'-o',binary,*flags],work,env=env)
  lib=isolated/'lib';lib.mkdir()
  for pattern in ['libidn2.so*','libunistring.so*']:
   for path in (relocated/'lib/void').glob(pattern):shutil.copy2(path,lib/path.name)
  native_env=dict(env,PATH='/hostile',VOID_STDLIB_ROOT='/absent',LD_LIBRARY_PATH=str(lib))
  for mode,expected in [('args',b'61726773\nargs\n'),('crypto',None)]:
   run([sys.executable,h.ROOT/'tests/host_capabilities/isolated_exec.py',isolated,binary,mode],isolated,expected,native_env)
result={'schema':1,'build':str(build),'stack_bytes':1048576,'installed_and_relocated':True,'aot_checkout_and_installed_compiler_inaccessible':True,'approved_source_bundle_reconstructed':True,'tampered_bundle_rejected':True,'records':records}
(h.output_directory()/'installed-host-capabilities.json').write_text(json.dumps(result,indent=2)+'\n')
print('Installed and relocated native providers, notices and modules; normal/O2 JIT and isolated AOT passed on 1 MiB stacks')
