from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import hashlib, json, os, platform, re, shutil, subprocess, sys, time, traceback

out=Path(r'\\vboxsvr\VM\project-slice3-final-preflight')
previous=out.parent/'project-slice3'
home=Path.home()
base=home/'VoidCI42377496702-Native'
old=home/'VoidSlice3-Final-Preflight-20261108'
root=home/'VoidSlice3-Preflight-Qualified-20261108'
sdk=base/'.toolchains/llvm-23.1.4'
cmake=Path(r'C:\gcc\bin\cmake.exe')
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def save(name,value):(out/name).write_text(json.dumps(value,indent=2)+'\n')
def qualify(profile):
    record={'status':'running','profile':profile,'platform':platform.platform(),'os_build':sys.getwindowsversion().build,'commands':[]}
    env=dict(os.environ,VOID_CLANG=str(sdk/'bin/clang.exe'),TEMP=str(root/('tmp-'+profile)),TMP=str(root/('tmp-'+profile)))
    env['PATH']=str(sdk/'bin')+os.pathsep+env['PATH']
    filename=profile+'-qualified.json'
    def run(label,args,cwd=root):
        argv=list(map(str,args));record['stage']=label;save(filename,record);start=time.monotonic()
        with (out/(profile+'-qualified-'+label+'.log')).open('wb') as log:
            process=subprocess.run(argv,cwd=cwd,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=420)
        record['commands'].append({'label':label,'argv':argv,'cwd':str(cwd),'returncode':process.returncode,'seconds':time.monotonic()-start})
        save(filename,record)
        assert process.returncode==0,(label,process.returncode)
    try:
        b=base/'build'/profile;old_obj=old/'build'/profile;dest=root/'build'/profile
        dest.mkdir(parents=True);Path(env['TEMP']).mkdir()
        flags=['/nologo','/std:c++20','/EHsc','/MT','/GR-','/W4','/permissive-','/D_ITERATOR_DEBUG_LEVEL=0','/DWIN32_LEAN_AND_MEAN','/DNOMINMAX','/D_WIN32_WINNT=0x0A00','/DVOID_COMPILER_VERSION="0.1.0"','/DVOID_STDLIB_ROOT="'+(root/'stdlib').as_posix()+'"','/I'+str(root/'include'),'/I'+str(root/'third_party/tomlplusplus'),'/imsvc'+str(sdk/'include')]
        flags+=['/O2','/DNDEBUG'] if profile=='Release' else ['/Od','/Zi']
        run('compiler-compile',[sdk/'bin/clang-cl.exe',*flags,'/c',root/'src/frontend/compiler.cpp','/Fo'+str(dest/'compiler.obj'),'/Fd'+str(dest/'compiler.pdb')])
        rc=re.search(r'^CMAKE_RC_COMPILER:FILEPATH=(.*)$',(previous/(profile+'-CMakeCache.txt')).read_text(),re.M)[1]
        def link(name,old_target,target,old_pdb,new_pdb):
            assert not target.exists()
            rsp=dest/(name+'.rsp')
            text=(out/(profile+'-'+name+'.rsp')).read_text()
            assert str(old_target) in text and str(old_obj/'compiler.obj') in text
            text=text.replace(str(old_target),str(target)).replace(str(old_obj/'compiler.obj'),str(dest/'compiler.obj')).replace(str(old_obj/old_pdb),str(dest/new_pdb))
            rsp.write_text(text);shutil.copyfile(rsp,out/(profile+'-qualified-'+name+'.rsp'))
            work=dest/name;work.mkdir()
            run(name,[cmake,'-E','vs_link_exe','--msvc-ver=1944','--intdir='+str(work),'--rc='+rc,'--mt='+str(sdk/'bin/llvm-mt.exe'),'--manifests',base/'cmake/LongPathAware.manifest','--',sdk/'bin/lld-link.exe','@'+str(rsp)],b)
        server=b/'voidls-slice3-preflight-qualified.exe'
        sessions=dest/'sessions.exe'
        link('server-link',b/'voidls-slice3-preflight-review.exe',server,'lsp-preflight-link.pdb','lsp-qualified-link.pdb')
        link('sessions-link',old_obj/'sessions.exe',sessions,'sessions-link.pdb','sessions-link.pdb')
        record['binaries']={str(p):sha(p) for p in (server,sessions)}
        run('sessions',[sessions])
        run('project-protocol',[sys.executable,root/'tests/project/protocol.py',server,out/(profile+'-qualified-protocol.json')])
        for name in ('protocol','stage7','stage8','sprint2'):
            args=[sys.executable,root/('tests/lsp/'+name+'.py'),server]
            if name=='protocol':args.append(dest/'legacy-fixtures')
            if name=='stage8':args.append(b/'voidc-slice3.exe')
            run('legacy-'+name,args)
        record['status']='PASS'
    except BaseException:record['status']='FAIL';record['error']=traceback.format_exc()
    finally:save(filename,record)
    return record
summary={'status':'running','platform':platform.platform(),'scope':'Incremental compiler rebuild and LSP/session relinks. Readability admission opens the file without a new file-size limit. Final boundary, session and legacy LSP tests.'}
save('qualified-native-results.json',summary)
try:
    prior=json.loads((out/'native-results.json').read_text())
    assert prior['status']=='PASS'
    for path,digest in prior['reused_inputs'].items():assert sha(path)==digest,path
    for path,digest in prior['binaries'].items():assert sha(path)==digest,path
    inherited=json.loads((previous/'final-native-results.json').read_text())
    for profile in ('Release','Debug'):
        for field in ('_new_binary_sha256','_baseline_binary_sha256'):
            for path,digest in inherited[profile+field].items():assert sha(path)==digest,path
    root.mkdir()
    inputs=prior['source_inputs_sha256'].copy()
    for name,digest in inputs.items():
        assert sha(old/name)==digest,name
        target=root/name;target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(old/name,target)
    inc='src/frontend/project_selection.inc'
    shutil.copyfile(out/'final-project_selection.inc',root/inc)
    inputs[inc]=(out/'final-project_selection.sha256').read_text().strip()
    for name,digest in inputs.items():assert sha(root/name)==digest,name
    summary['source_inputs_sha256']=inputs;save('qualified-native-results.json',summary)
    with ThreadPoolExecutor(max_workers=2) as pool:summary['profiles']=list(pool.map(qualify,('Release','Debug')))
    for path,digest in prior['reused_inputs'].items():assert sha(path)==digest,path
    summary['status']='PASS' if all(p['status']=='PASS' for p in summary['profiles']) else 'FAIL'
except BaseException:summary['status']='FAIL';summary['error']=traceback.format_exc()
finally:save('qualified-native-results.json',summary)
