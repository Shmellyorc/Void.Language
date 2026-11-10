from hashlib import file_digest
from pathlib import Path
import json
import platform
import shutil
import subprocess
import sys
import traceback

share = Path(r'\\vboxsvr\VM\project-slice7-g4-windows')
home = Path.home().resolve(strict=True)
work = home/'VoidS7-G4W-architecture-replay'
toolchain = home/'VoidCI42377496702-Native/.toolchains/llvm-23.1.4/bin'
clang = toolchain/'clang-cl.exe'
linker = toolchain/'lld-link.exe'
readobj = toolchain/'llvm-readobj.exe'
record = {'status':'RUNNING','os':platform.platform(),'build':sys.getwindowsversion().build,
          'work':str(work),'commands':[],'inputs':{},'artifacts':{}}

def digest(path):
    with path.open('rb') as raw:
        return file_digest(raw,'sha256').hexdigest()

def save():
    (share/'native-results.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8')

def run(label, *argv, required=True):
    args = list(map(str,argv))
    with (share/(label+'.log')).open('wb') as output:
        result = subprocess.run(args,cwd=work,stdout=output,stderr=subprocess.STDOUT,timeout=45)
    record['commands'].append({'label':label,'argv':args,'exit':result.returncode})
    save()
    if required and result.returncode:
        raise RuntimeError(f'{label} failed with {result.returncode}')
    return result.returncode

def build_cpp(source, output, *extra):
    run('build-'+Path(output).stem,clang,'/nologo','/std:c++20','/EHsc','/MT',source,
        '/Fo'+Path(output).stem+'.obj','/Fe'+output,'/link','/manifest:embed',*extra)

try:
    assert record['build']==19045
    work.mkdir()
    for child in ('good','alternate','child-good','child-bad'):
        (work/child).mkdir()
    inputs = json.loads((share/'input-sha256.json').read_text())
    for name, expected in inputs.items():
        assert Path(name).name==name and digest(share/name)==expected,name
        shutil.copyfile(share/name,work/name)
        assert digest(work/name)==expected,name
        record['inputs'][name]=expected
    shutil.copyfile(work/'override-dependency.manifest',work/'alternate/override-dependency.manifest')
    run('toolchain',clang,'--version')
    common = (clang,'/nologo','/TC','/c','/GS-','/Zl')
    run('build-dependency',*common,'dependency.c','/Fodependency.obj')
    run('build-competitor',*common,'/DFIXTURE_VALUE=99','dependency.c','/Focompetitor.obj')
    run('build-provider',*common,'provider.c','/Foprovider.obj')
    run('build-marker',*common,'redirected.c','/Fomarker.obj')
    run('build-explicit-object',*common,'explicit.c','/Foexplicit.obj')
    run('link-good',linker,'/dll','/noentry','/nodefaultlib','/out:good/void_g4_probe_dep.dll',
        '/implib:dep.lib','dependency.obj')
    run('link-alternate',linker,'/dll','/entry:checkpoint_attach','/nodefaultlib',
        '/out:alternate/void_g4_probe_dep.dll','competitor.obj','marker.obj')
    run('link-provider',linker,'/dll','/noentry','/nodefaultlib','/out:good/void_g4_probe_main.dll',
        'provider.obj','dep.lib')
    run('link-explicit',linker,'/dll','/noentry','/nodefaultlib','/out:good/void_g4w_explicit.dll','explicit.obj')
    run('link-forwarder',linker,'/dll','/noentry','/nodefaultlib','/machine:x64',
        '/out:good/void_g4w_forwarder.dll','/def:forwarder.def')
    build_cpp('loader_probe.cpp','loader-probe.exe')
    build_cpp('loader_probe.cpp','alternate/process-default.exe',
              '/manifestinput:alternate/override-dependency.manifest')
    build_cpp('launcher.cpp','launcher.exe')
    run('build-startup',clang,'/nologo','/TC','/MT','startup.c','dep.lib','/Festartup.exe','/link','/manifest:embed')
    for label in ('good','bad'):
        child=work/('child-'+label)
        shutil.copyfile(work/'startup.exe',child/'startup.exe')
        shutil.copyfile(work/('good' if label=='good' else 'alternate')/'void_g4_probe_dep.dll',
                        child/'void_g4_probe_dep.dll')
    run('pe-facts',readobj,'--file-headers','--coff-imports','--coff-exports','--coff-resources',
        'loader-probe.exe','alternate/process-default.exe','launcher.exe','startup.exe',
        'good/void_g4_probe_main.dll','good/void_g4_probe_dep.dll','alternate/void_g4_probe_dep.dll',
        'good/void_g4w_explicit.dll','good/void_g4w_forwarder.dll')
    run('dotlocal-policy','reg.exe','query',
        r'HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options',
        '/v','DevOverrideEnable',required=False)
    statuses = {}
    for mode in ('clean','collision','late','thread','thread-default','thread-deny',
                 'process-default','process-deny','preload-thread','data','forward','explicit'):
        program=work/('alternate/process-default.exe' if mode.startswith('process-') else 'loader-probe.exe')
        statuses[mode]=run('probe-'+mode,program,mode,work,required=False)
    for label in ('good','bad'):
        statuses['startup-'+label]=run('startup-'+label,work/'launcher.exe',
                                      work/('child-'+label)/'startup.exe',required=False)
    record['probes']=statuses
    for path in sorted(work.rglob('*')):
        if path.is_file():
            assert path.resolve(strict=True)==path and path.stat().st_nlink==1,path
            record['artifacts'][str(path.relative_to(work)).replace('\\','/')]=digest(path)
    record['status']='PASS' if all(code==0 for code in statuses.values()) else 'PROBE_FAILURE'
except BaseException:
    record['status']='FAIL'
    record['error']=traceback.format_exc()
finally:
    save()
