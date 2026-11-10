from pathlib import Path
import hashlib, json, os, platform, re, shutil, subprocess, sys, time, traceback

out=Path(r'\\vboxsvr\VM\project-slice4')
home=Path.home()
baseline=home/'VoidCI42377496702-Native'
source=home/'VoidS4-source2'
sdk=baseline/'.toolchains/llvm-23.1.4'
ninja=Path(r'C:\gcc\bin\ninja.exe')
root=home/'VoidS4-Allocator-Final'
artifacts=out/'allocator-local-artifacts'
def sha(path):
    with Path(path).open('rb') as file:return hashlib.file_digest(file,'sha256').hexdigest()
def save():(out/'allocator-resumed-results.json').write_text(json.dumps(record,indent=2)+'\n')
record={'status':'running','platform':platform.platform(),'commands':[]}
def run(profile,label,args,env,cwd):
    log=out/(profile+'-allocator-'+label+'.log')
    with log.open('wb') as file:
        process=subprocess.run(args,cwd=cwd,env=env,stdout=file,stderr=subprocess.STDOUT,timeout=420)
    record['commands'].append({'profile':profile,'label':label,'argv':args,'cwd':str(cwd),'status':process.returncode})
    save()
    assert process.returncode==0,(profile,label,process.returncode)
try:
    original=json.loads((out/'allocator-final-results.json').read_text())
    assert original['status']=='FAIL' and original['profiles']['Release']
    assert any(c['profile']=='Debug' and c['label']=='compile' and c['status']==0 for c in original['commands'])
    assert 'Windows error 112' in (out/'Debug-disk-full-process.log').read_text()
    record=original.copy()
    record['status']='running'
    record.pop('error')
    record['prior_disk_full_receipt']='allocator-final-results.json'
    test=root/'application.cpp';process_test=root/'application.py'
    assert sha(test)==record['test_sha256'] and sha(process_test)==record['process_test_sha256']
    assert sha(source/'stdlib/os.void')==record['source_stdlib_sha256']
    for profile in ('Debug',):
        build=home/('VoidS4-Final-'+profile)
        dest=root/profile
        output=artifacts/profile
        env=dict(os.environ,VOID_STDLIB_ROOT=str(source/'stdlib'),TEMP=str(dest),TMP=str(dest))
        obj=output/'application.obj'
        record['debug_object_sha256']=sha(obj)
        cli=build/'voidc.exe'
        assert sha(cli)==record['compiler_sha256'][profile]
        record['guest_free_bytes_before_debug_rerun']=shutil.disk_usage(root).free
        save()
        ninja_file=(build/'build.ninja').read_text()
        block=re.search(r'^build void_project_application\.exe:.*?(?=^#|^build |\Z)',ninja_file,re.M|re.S)[0]
        libraries=re.search(r'^  LINK_LIBRARIES = (.*)$',block,re.M)[1]
        libraries=re.sub(r'\$([ $:])',lambda m:m[1],libraries)
        link_flags=re.search(r'^  LINK_FLAGS = (.*)$',block,re.M)[1]
        rc=re.search(r'^CMAKE_RC_COMPILER:[^=]+=(.*)$',(build/'CMakeCache.txt').read_text(),re.M)[1]
        cmake=Path(r'C:\gcc\bin\cmake.exe')
        executable=output/'void_project_application-allocator.exe'
        response=dest/'link.rsp'
        response.write_text('"'+str(obj)+'" '+link_flags+' '+libraries+' /out:"'+str(executable)+'" /pdb:"'+str(output/'linked.pdb')+'"')
        work=dest/'link';work.mkdir()
        argv=[cmake,'-E','vs_link_exe','--msvc-ver=1944','--intdir='+str(work),'--rc='+rc,'--mt='+str(sdk/'bin/llvm-mt.exe'),'--manifests',baseline/'cmake/LongPathAware.manifest','--',sdk/'bin/lld-link.exe','@'+str(response)]
        run(profile,'link',list(map(str,argv)),env,build)
        run(profile,'test',[str(executable)],env,build)
        receipt=(out/(profile+'-allocator-test.log')).read_text()
        assert '63 checks passed' in receipt,receipt
        record['profiles'][profile]={'binary_sha256':sha(executable),'stdout':receipt,'guest_free_bytes':shutil.disk_usage(root).free}
    assert shutil.disk_usage(root).free>=500*1024*1024,'C: needs at least 500 MiB free for the protected local AOT staging'
    run('Debug','process',[sys.executable,str(process_test),str(cli),str(out/'Debug-corrected-process.json')],env,build)
    record['guest_free_bytes_after_debug_rerun']=shutil.disk_usage(root).free
    record['status']='PASS'
except BaseException:record['status']='FAIL';record['error']=traceback.format_exc()
finally:save()
