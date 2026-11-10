from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import hashlib, json, os, platform, re, shutil, subprocess, sys, time, traceback, zipfile

out=Path(r'\\vboxsvr\VM\project-slice4')
prior=out.parent/'project-slice3'
home=Path.home()
baseline=home/'VoidCI42377496702-Native'
packet=home/'VoidS4-source2'
source=baseline
backup=home/'VoidS4-native-retry-backup'
sdk=baseline/'.toolchains/llvm-23.1.4'
cmake=Path(r'C:\gcc\bin\cmake.exe')
ctest=cmake.with_name('ctest.exe')
ninja=cmake.with_name('ninja.exe')
def sha(path):
    with Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()
def save(name,value):(out/name).write_text(json.dumps(value,indent=2)+'\n')
def options(profile):
    cache=(prior/(profile+'-CMakeCache.txt')).read_text()
    lookup=lambda key:re.search(r'^'+re.escape(key)+r':[^=]+=(.*)$',cache,re.M)[1]
    keys=('CMAKE_AR','CMAKE_CXX_COMPILER','CMAKE_C_COMPILER','CMAKE_LINKER','CMAKE_MAKE_PROGRAM',
          'CMAKE_MT','CMAKE_RC_COMPILER','LLVM_DIR','ZLIB_INCLUDE_DIR','ZLIB_LIBRARY_RELEASE','zstd_DIR','LibXml2_DIR',
          'VOID_HOST_DEPS_ROOT','VOID_HTTP_DEPS_ROOT','VOID_CLANG')
    return ['-D'+key+'='+lookup(key) for key in keys]+['-DCMAKE_BUILD_TYPE='+profile,'-DBUILD_TESTING=ON','-DVOID_SANITIZERS=OFF','-DVOID_LLVM_RESEARCH=OFF','-DPython3_EXECUTABLE='+sys.executable]
def qualify(profile):
    record={'status':'running','profile':profile,'platform':platform.platform(),'os_build':sys.getwindowsversion().build,'commands':[]}
    build=home/('VoidS4-Final-'+profile)
    temp=home/('VoidS4-tmp-'+profile)
    filename=profile+'-result.json'
    def run(label,args,timeout=1800):
        argv=list(map(str,args));record['stage']=label;save(filename,record);start=time.monotonic()
        with (out/(profile+'-'+label+'.log')).open('wb') as log:
            process=subprocess.run(argv,cwd=source,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=timeout)
        record['commands'].append({'label':label,'argv':argv,'returncode':process.returncode,'seconds':time.monotonic()-start})
        save(filename,record)
        assert process.returncode==0,(label,process.returncode)
    try:
        temp.mkdir(exist_ok=True)
        env=dict(os.environ,VOID_CLANG=str(sdk/'bin/clang.exe'),TEMP=str(temp),TMP=str(temp))
        env['PATH']=str(sdk/'bin')+os.pathsep+env['PATH']
        env['VOID_CRASH_EVIDENCE']=str(temp/'crashes')
        run('configure',[cmake,'-S',source,'-B',build,'-G','Ninja',*options(profile)])
        targets=['voidc','voidls','void_project_application','void_project_sessions','void_project_selection','void_project_manifest',
                 'void_tests','void_globals','void_os_host','void_io_runner','void_native_tests','void_native_inventory']
        run('build',[cmake,'--build',build,'--parallel','2','--target',*targets])
        tests=r'^(project_(application|application_process|manifest|manifest_legacy|selection|sessions|discovery_cli|profiles|protocol|nested_cli)|os_(host|errors|frontend|owner)|stage11_globals(_aot)?|lsp_(protocol|os|stage8)|sessions|native_source|stage6_integrated)$'
        xml=out/(profile+'-ctest.xml')
        run('ctest',[ctest,'--test-dir',build,'--output-on-failure','-j','2','-R',tests,'--output-junit',xml],1200)
        for name in ('project-application-evidence.json','project-protocol-evidence.json','project-discovery-evidence.json','project-profiles-evidence.json'):
            path=build/name
            if path.exists():shutil.copyfile(path,out/(profile+'-'+name))
        record['binaries']={n:sha(build/(n+'.exe')) for n in ['voidc','voidls','void_project_application','void_project_sessions']}
        record['status']='PASS'
    except BaseException:record['status']='FAIL';record['error']=traceback.format_exc()
    finally:save(filename,record)
    return record

summary={'status':'running','platform':platform.platform(),'scope':'Native Windows 10 targeted builds at the provider-attested source root. Original source files are backed up and restored. Build outputs and evidence use isolated paths.'}
save('native-results.json',summary)
overlays={}
created_dirs=[]
def parents(path):
    needed=[]
    while not path.exists():
        needed.append(path)
        path=path.parent
    for directory in reversed(needed):
        directory.mkdir()
        created_dirs.append(directory)

def alias(path):
    return path.is_symlink() or path.is_junction()

try:
    receipt=json.loads((out/'native-first-build-results.json').read_text())
    assert receipt['status']=='FAIL' and receipt['restored_original_source'] and all(p['stage']=='build' for p in receipt['profiles'])
    hashes=receipt['source_inputs_sha256']
    changed=json.loads((out/'portable-updates-sha256.json').read_text())
    with zipfile.ZipFile(out/'portable-updates.zip') as archive:
        assert set(archive.namelist())==set(changed)
        for name,digest in changed.items():
            assert sha(packet/name)==hashes[name],name
            (packet/name).write_bytes(archive.read(name))
            assert sha(packet/name)==digest,name
    hashes.update(changed)
    summary['portable_updates_sha256']=changed

    backup.mkdir()
    for name,digest in hashes.items():
        incoming=packet/name
        assert sha(incoming)==digest,name
        target=source/name
        assert not alias(target) and not any(alias(p) for p in target.parents if p.is_relative_to(source)),name
        if target.exists():
            assert target.is_file(),name
            previous=sha(target)
            if previous==digest:continue
            preserved=backup/name
            preserved.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(target,preserved)
            assert sha(preserved)==previous,name
        else:previous=None
        overlays[name]=previous
        parents(target.parent)
        shutil.copyfile(incoming,target)
        assert sha(target)==digest,name
    summary['source_inputs_sha256']=hashes
    summary['before_overlay_sha256']=overlays
    summary['source_packet_sha256']=receipt['source_packet_sha256']
    save('native-results.json',summary)
    with ThreadPoolExecutor(max_workers=2) as pool:summary['profiles']=list(pool.map(qualify,('Release','Debug')))
    summary['status']='PASS' if all(p['status']=='PASS' for p in summary['profiles']) else 'FAIL'
except BaseException:summary['status']='FAIL';summary['error']=traceback.format_exc()
finally:
    try:
        for name,digest in reversed(list(overlays.items())):
            target=source/name
            if digest is None:target.unlink(missing_ok=True)
            else:
                shutil.copy2(backup/name,target)
                assert sha(target)==digest,name
        for directory in reversed(created_dirs):
            if not any(directory.iterdir()):directory.rmdir()
        summary['restored_original_source']=True
    except BaseException:
        summary['status']='FAIL'
        summary['restoration_error']=traceback.format_exc()
    save('native-results.json',summary)
