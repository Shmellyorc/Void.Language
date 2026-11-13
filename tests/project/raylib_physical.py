"""Real package bytes, physical declaration verification and editable local source."""
from pathlib import Path
import json,os,re,shutil,subprocess,sys,tempfile
compiler,catalog=map(lambda p:Path(p).resolve(),sys.argv[1:3]);repo=Path(__file__).resolve().parents[2]
env=dict(os.environ,VOID_NATIVE_CATALOG=str(catalog));windows=os.name=='nt'
with tempfile.TemporaryDirectory(prefix='VOID editable raylib ') as temporary:
 root=Path(temporary);project=root/'consumer'
 def run(*args,status=0,text=None):
  p=subprocess.run(list(map(str,args)),env=env,cwd=root,capture_output=True,timeout=240)
  output=(p.stdout+p.stderr).decode(errors='replace')
  assert p.returncode==status,(args,p.returncode,output)
  assert text is None or text in output,(args,output)
  assert 'AddressSanitizer' not in output and 'UndefinedBehaviorSanitizer' not in output,output
  return output
 # G27 library authors acquire source through the same add/publication path.
 library=root/'engine';application=root/'nativeapp'
 run(compiler,'new',library,'--kind','library','--package-name','engine')
 run(compiler,'add','raylib','--path',repo/'verified-packages/raylib','--project',library,
     text="source declares native provider 'org.voidlang.raylib'")
 (library/'src/engine.void').write_text('''use pkg/raylib as rl
public func answer() -> i32:
    return rl.get_random_value(41, 41)
''')
 run(compiler,'tidy','--check','--project',library)
 run(compiler,'new',application)
 run(compiler,'add','engine','--path',library,'--project',application)
 (application/'src/app.void').write_text('''use pkg/engine
func main() -> i32:
    print("LIBRARY:", engine.answer())
    return 0
''')
 run(compiler,'check','--project',application,status=1,text='matching admitted descriptor')
 with (application/'void.project.toml').open('a') as f:f.write('''
[native-requirements.raylib]
id="org.voidlang.raylib"
version="=6.0.0"
abi=3
''')
 run(compiler,'tidy','--project',application)
 run(compiler,'tidy','--check','--project',application)
 run(compiler,'check','--project',application)
 run(compiler,'run','--project',application,status=2,text='--allow-native')
 for profile in ['debug','release']:
  run(compiler,'run','--no-cache','--project',application,'--allow-native','--profile',profile,text='LIBRARY: 41')
 image=root/('library-native.exe' if windows else 'library-native')
 run(compiler,'build','--project',application,'--allow-native','-o',image);run(image,text='LIBRARY: 41')
 output=root/'library-package';moved=root/'library-relocated'
 run(compiler,'package','--project',application,'--allow-native','-o',output)
 run(compiler,'package','--verify',output);shutil.move(output,moved)
 shutil.move(application,root/'parked-nativeapp');shutil.move(library,root/'parked-engine')
 run(compiler,'package','--verify',moved);run(moved/('app.exe' if windows else 'app'),text='LIBRARY: 41')
 run(compiler,'new',project)
 package=project/'deps/raylib';shutil.copytree(repo/'verified-packages/raylib',package,ignore=shutil.ignore_patterns('__pycache__','.bind-*'))
 manifest=project/'void.project.toml'
 run(compiler,'add','raylib','--path','deps/raylib','--project',project,text="source declares native provider 'org.voidlang.raylib'")
 manifest.write_text(manifest.read_text()+'''\n[native-requirements.raylib]
id="org.voidlang.raylib"
version="=6.0.0"
abi=3
''')
 entry=next((project/'src').glob('*.void'));entry.write_text('''use pkg/raylib as rl
func main() -> i32:
    print("PHYSICAL:", rl.get_random_value(41, 41))
    return 0
''')
 api=package/'src/raylib.void';native=package/'src/raylib_native.void'
 assert 'entry = "src/raylib.void"' in (package/'void.project.toml').read_text()
 assert not list(package.rglob('*.void.inc'))
 assert 'raylib_checked.void' not in api.read_text()
 inventory=json.loads((package/'inventory.json').read_text());names=set(re.findall(r'^func (\w+)',api.read_text(),re.M))
 assert len(inventory['functions'])==600 and all(f['void_name'] in names for f in inventory['functions'])
 run(compiler,'tidy','--project',project);run(compiler,'tidy','--check','--project',project)
 for profile in ['debug','release']:run(compiler,'run','--project',project,'--allow-native','--profile',profile,text='PHYSICAL: 41')
 before=api.read_bytes();body=before.decode();at=body.index('func get_random_value(');end=body.index('\nfunc ',at+1)
 part=body[at:end];assert 'return value' in part;body=body[:at]+part.replace('return value','return value + 1')+body[end:]
 api.write_text(body)
 run(compiler,'check','--project',project,status=1,text='Stale or tampered void.lock')
 run(compiler,'tidy','--project',project)
 for profile in ['debug','release']:
  run(compiler,'run','--project',project,'--allow-native','--profile',profile,text='PHYSICAL: 42')
  image=root/profile/('app.exe' if windows else 'app');run(compiler,'build','--project',project,'--allow-native','--profile',profile,'-o',image);run(image,text='PHYSICAL: 42')
 # No generator was invoked. New wrappers are ordinary editable VOID too.
 with api.open('a') as f:f.write('\nfunc editable_wrapper() -> i32:\n    return get_random_value(41, 41) + 1\n')
 entry.write_text('''use pkg/raylib as rl
func main() -> i32:
    print("WRAPPER:", rl.editable_wrapper())
    return 0
''')
 run(compiler,'tidy','--project',project);run(compiler,'run','--project',project,'--allow-native',text='WRAPPER: 43')
 original=native.read_bytes()
 for changed,message in [
  (original.replace(b'func api_error() -> i32:',b'func api_error() -> i64:'),'Physical native signature differs'),
  (original.replace(b'    a: u8',b'    a: u32',1),'Physical native signature differs'),
  (original.replace(b'func api_error() -> i32:\n    pass',b'func api_error() -> i32:\n    return 0'),'exact descriptor declarations'),
  (original.replace(b'org.voidlang.raylib',b'org.invalid.provider'),'matching admitted descriptor')]:
  assert changed!=original
  native.write_bytes(changed);run(compiler,'tidy','--project',project)
  run(compiler,'check','--project',project,status=1,text=message)
 native.write_bytes(original);run(compiler,'tidy','--project',project)
 run(compiler,'check','--project',project)
 # The approved catalog is still mandatory; physical stubs cannot become no-ops.
 saved=manifest.read_text();manifest.write_text(saved.split('[native-requirements.raylib]')[0]);run(compiler,'tidy','--project',project)
 run(compiler,'check','--project',project,status=1,text='matching admitted descriptor')
print('Raylib physical package, all 600 APIs, edited normal/O2 JIT/AOT, new wrapper, lock and native signature/body/provider guards passed')
