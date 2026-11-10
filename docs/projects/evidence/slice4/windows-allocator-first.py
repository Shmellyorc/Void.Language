from pathlib import Path
import hashlib, json, os, platform, re, shutil, subprocess, sys, time, traceback

out=Path(r'\\vboxsvr\VM\project-slice4')
home=Path.home()
baseline=home/'VoidCI42377496702-Native'
source=home/'VoidS4-source2'
sdk=baseline/'.toolchains/llvm-23.1.4'
ninja=Path(r'C:\gcc\bin\ninja.exe')
root=home/'VoidS4-Allocator-Rerun'
def sha(path):
    with Path(path).open('rb') as file:return hashlib.file_digest(file,'sha256').hexdigest()
def save():(out/'allocator-native-results.json').write_text(json.dumps(record,indent=2)+'\n')
record={'status':'running','platform':platform.platform(),'commands':[]}
def run(profile,label,args,env,cwd):
    log=out/(profile+'-allocator-'+label+'.log')
    with log.open('wb') as file:
        process=subprocess.run(args,cwd=cwd,env=env,stdout=file,stderr=subprocess.STDOUT,timeout=420)
    record['commands'].append({'profile':profile,'label':label,'argv':args,'cwd':str(cwd),'status':process.returncode})
    save()
    assert process.returncode==0,(profile,label,process.returncode)
try:
    for _ in range(180):
        prior=json.loads((out/'native-results.json').read_text())
        if prior['status']!='running':break
        time.sleep(5)
    assert prior['restored_original_source']
    for name,digest in prior['source_inputs_sha256'].items():assert sha(source/name)==digest,name
    root.mkdir()
    test=root/'application.cpp'
    shutil.copyfile(out/'final-allocator-application.cpp',test)
    assert sha(test)==(out/'final-allocator-application.sha256').read_text().strip()
    record['test_sha256']=sha(test)
    record['source_stdlib_sha256']=sha(source/'stdlib/os.void')
    process_test=root/'application.py'
    shutil.copyfile(out/'final-application-process.py',process_test)
    assert sha(process_test)==(out/'final-application-process.sha256').read_text().strip()
    record['process_test_sha256']=sha(process_test)
    record['profiles']={}
    for profile in ('Release','Debug'):
        build=home/('VoidS4-Final-'+profile);dest=root/profile;dest.mkdir()
        env=dict(os.environ,VOID_STDLIB_ROOT=str(source/'stdlib'),TEMP=str(dest),TMP=str(dest))
        flags=['/nologo','/std:c++20','/EHsc','/MT','/GR-','/W4','/permissive-',
               '/D_ITERATOR_DEBUG_LEVEL=0','/I'+str(source/'include')]
        flags+=['/O2','/DNDEBUG'] if profile=='Release' else ['/Od','/Zi']
        obj=dest/'application.obj'
        run(profile,'compile',[str(sdk/'bin/clang-cl.exe'),*flags,'/c',str(test),'/Fo'+str(obj),'/Fd'+str(dest/'application.pdb')],env,build)
        original=build/'project-application-evidence.json'
        if original.exists():shutil.copyfile(original,out/(profile+'-original-process.json'))
        cli=build/'voidc.exe'
        record.setdefault('compiler_sha256',{})[profile]=sha(cli)
        run(profile,'process',[sys.executable,str(process_test),str(cli),str(out/(profile+'-corrected-process.json'))],env,build)
        ninja_file=(build/'build.ninja').read_text()
        block=re.search(r'^build void_project_application\.exe:.*?(?=^#|^build |\Z)',ninja_file,re.M|re.S)[0]
        libraries=re.search(r'^  LINK_LIBRARIES = (.*)$',block,re.M)[1]
        libraries=re.sub(r'\$([ $:])',lambda m:m[1],libraries)
        link_flags=re.search(r'^  LINK_FLAGS = (.*)$',block,re.M)[1]
        rc=re.search(r'^CMAKE_RC_COMPILER:FILEPATH=(.*)$',(build/'CMakeCache.txt').read_text(),re.M)[1]
        cmake=Path(r'C:\gcc\bin\cmake.exe')
        executable=dest/'void_project_application-allocator.exe'
        response=dest/'link.rsp'
        response.write_text('"'+str(obj)+'" '+link_flags+' '+libraries+' /out:"'+str(executable)+'" /pdb:"'+str(dest/'linked.pdb')+'"')
        work=dest/'link';work.mkdir()
        argv=[cmake,'-E','vs_link_exe','--msvc-ver=1944','--intdir='+str(work),'--rc='+rc,'--mt='+str(sdk/'bin/llvm-mt.exe'),'--manifests',baseline/'cmake/LongPathAware.manifest','--',sdk/'bin/lld-link.exe','@'+str(response)]
        run(profile,'link',list(map(str,argv)),env,build)
        run(profile,'test',[str(executable)],env,build)
        receipt=(out/(profile+'-allocator-test.log')).read_text()
        assert '63 checks passed' in receipt,receipt
        record['profiles'][profile]={'binary_sha256':sha(executable),'stdout':receipt}
    record['status']='PASS'
except BaseException:record['status']='FAIL';record['error']=traceback.format_exc()
finally:save()
