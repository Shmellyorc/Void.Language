from pathlib import Path
import sys,subprocess,shutil,importlib.util,hashlib
root=Path.cwd();work=root/'.toolchains/ci41307437931-ubuntu';build=work/'build';diag=work/'diagnostic'
sys.path.insert(0,str(root/'tests/net/http'))
spec=importlib.util.spec_from_file_location('installation',root/'tests/net/http/install.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
original_run=subprocess.run
first=True
def control(argv,*args,**kwargs):
 global first
 if str(argv[0]).endswith('/bin/voidc') and first:
  first=False
  print('ORIGINAL CHECKSUM',hashlib.sha256(Path(argv[0]).read_bytes()).hexdigest(),hashlib.sha256((build/'voidc').read_bytes()).hexdigest(),flush=True)
  original_run(['ldd',str(argv[0])])
  for label,binary in [('installed',None),('diagnostic-installed',diag/'voidc-diagnostic'),('build',build/'voidc')]:
   if binary: shutil.copy2(binary,argv[0])
   if label=='diagnostic-installed':
    script=work/'remove-rpath.cmake';script.write_text('file(RPATH_REMOVE FILE "'+str(argv[0])+'")')
    original_run(['cmake','-P',str(script)],check=True)
   r=original_run(argv,*args,**kwargs)
   print(label,r.returncode,r.stdout,r.stderr,flush=True)
  raise SystemExit(0)
 return original_run(argv,*args,**kwargs)
subprocess.run=control
sys.argv=['install.py',str(build/'voidc'),'--evidence',str(work/'install-controls')]
module.main()
