"""Run the installed workflow with the previously qualified local raylib build."""
from hashlib import file_digest
from pathlib import Path
import json, os, shutil, subprocess, sys, tempfile, traceback

share=Path(r'\\vboxsvr\VM\slice7-final-integrated')
local=Path.home()/'VS7I2'
baseline=Path.home()/'VoidCI42377496702-Native'
sdk=baseline/'.toolchains/llvm-23.1.4/bin'
cmake=Path(r'C:\gcc\bin\cmake.exe')
raylib=json.loads((share.parent/'slice7-raylib/software-result.json').read_text())
assert raylib['status']=='PASS'
inputs={key:Path(raylib[key]) for key in ('dll','header','import_library')}
def digest(path):
    with path.open('rb') as stream:
        return file_digest(stream,'sha256').hexdigest()
for key,field in [('dll','dll_sha256'),('header','header_sha256'),('import_library','import_sha256')]:
    assert digest(inputs[key])==raylib[field]

receipt=share/'public-results.json'
records=json.loads(receipt.read_text()) if receipt.is_file() else {}
modes=tuple(sys.argv[1:]) or ('Release','Debug')
assert set(modes)<={'Release','Debug'}
for mode in modes:
    if records.get(mode,{}).get('exit')==0 and records[mode].get('removed'):
        continue
    scratch=Path(tempfile.mkdtemp(prefix='public-',dir=local))
    record=records[mode]={'scratch':str(scratch)}
    try:
        identity=subprocess.check_output(['whoami.exe'],text=True).strip()
        acl=subprocess.check_output(['icacls.exe',scratch,'/grant:r',identity+':(OI)(CI)F','/inheritance:r'],text=True)
        subprocess.run(['icacls.exe',scratch,'/remove:g','*S-1-3-4'],check=True,capture_output=True)
        record['acl']=subprocess.check_output(['icacls.exe',scratch],text=True)
        env=dict(os.environ,TEMP=str(scratch),TMP=str(scratch),VOID_CLANG=str(sdk/'clang.exe'),
                 PYTHONDONTWRITEBYTECODE='1',PATH=str(sdk)+os.pathsep+os.environ['PATH'])
        with (share/(mode+'-public.txt')).open('wb') as log:
            p=subprocess.run([sys.executable,share/'public-workflow.py',
                '--cmake',cmake,'--build',local/mode,'--source',local/'src',
                '--clang',sdk/'clang.exe','--llvm-readobj',sdk/'llvm-readobj.exe',
                '--work',local/(mode+'-public'),'--header',inputs['header'],'--library',inputs['dll'],
                '--license',inputs['header'].parents[1]/'LICENSE','--import-library',inputs['import_library'],
                '--config',mode],env=env,stdout=log,stderr=subprocess.STDOUT,timeout=1200)
        record['exit']=p.returncode
    except BaseException:
        record['error']=traceback.format_exc()
    finally:
        record['leftovers']=sorted(p.relative_to(scratch).as_posix() for p in scratch.rglob('*'))
        shutil.rmtree(scratch)
        record['removed']=not scratch.exists()
        receipt.write_text(json.dumps(records,indent=2)+'\n')
sys.exit(0 if all(records[mode].get('exit')==0 and records[mode]['removed'] for mode in modes) else 1)
