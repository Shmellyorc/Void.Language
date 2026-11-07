"""Relocated bootstrap and toolchain execution with checkout denied by Landlock."""
from pathlib import Path
import argparse,json,os,shutil,sys
from support import ROOT,EVIDENCE,Runner
parser=argparse.ArgumentParser();parser.add_argument('prefix',type=Path);parser.add_argument('work',type=Path)
parser.add_argument('--name',default='installed-1');options=parser.parse_args()
runner=Runner(options.name);work=options.work.resolve();old=work/'private root';root=work/'relocated private root'
assert old.is_dir() and not root.exists();old.rename(root)
destination=work/'relocated bootstrap';shutil.copytree(options.prefix,destination,symlinks=False)
app=destination/'bin/voidup';source=work/'arguments.void'
launcher=ROOT/'tests/host_capabilities/isolated_exec.py'
def isolated(args,**kwargs):return runner.run([sys.executable,launcher,work,*args],cwd=work,**kwargs)
env=dict(os.environ,PATH='/hostile',VOID_STDLIB_ROOT='/absent',LD_LIBRARY_PATH='/absent')
isolated([app,'--help'],env=env)
base=[app,'--root',root]
isolated([*base,'doctor'],env=env)
for release in ('engineering-a','engineering-b'):
 isolated([*base,'default',release],env=env)
 isolated([*base,'run','default','voidc','--','--version'],env=env)
 for flags in (['--no-opt'],[]):
  isolated([*base,'run','default','voidc','--','run',source,*flags,'--no-cache','--','','雪😀','literal $()'],env=env,expected=17)
  binary=work/(release+('-isolated-normal' if flags else '-isolated-o2'))
  isolated([*base,'run','default','voidc','--','build',source,'-o',binary,*flags],env=env)
  independent=work/(release+'-independent-'+str(bool(flags)))
  independent.mkdir();shutil.copy2(binary,independent/'application');(independent/'lib').mkdir()
  for item in (root/'toolchains'/release/'lib/void').glob('*.so*'):
   shutil.copy2(item,independent/'lib'/item.name)
  native_env=dict(env,LD_LIBRARY_PATH=str(independent/'lib'))
  # Even installed compilers and all toolchain resources are now denied.
  runner.run([sys.executable,launcher,independent,independent/'application','','雪😀','literal $()'],cwd=independent,env=native_env,expected=17)
isolated([*base,'rollback'],env=env)
(EVIDENCE/(options.name+'-summary.json')).write_text(json.dumps(dict(schema=1,bootstrap=str(destination),root=str(root),checkout_denied=True,relocated=True,caller_path_and_stdlib_override_ignored=True,independently_linked_aot_toolchains_denied=True,hard_stack_bytes=1048576),indent=2)+'\n')
print('Relocated bootstrap and A/B compiler JIT/AOT passed with checkout denied; independent programs also denied installed toolchains')
