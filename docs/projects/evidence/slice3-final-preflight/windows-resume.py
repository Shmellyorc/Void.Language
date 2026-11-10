from pathlib import Path
import hashlib, json, os, platform, re, shutil, subprocess, sys, time, traceback, zipfile

out=Path(r'\\vboxsvr\VM\project-slice3-final-preflight')
previous=out.parent/'project-slice3'
home=Path.home()
base=home/'VoidCI42377496702-Native'
old=home/'VoidSlice3-Corrected-20261108'
root=home/'VoidSlice3-Final-Preflight-20261108'
sdk=base/'.toolchains/llvm-23.1.4'
cmake=Path(r'C:\gcc\bin\cmake.exe')
record={'status':'running','platform':platform.platform(),'os_build':sys.getwindowsversion().build,'commands':[],'reused_inputs':{},'binaries':{}}
env=dict(os.environ,VOID_CLANG=str(sdk/'bin/clang.exe'),TEMP=str(root/'tmp'),TMP=str(root/'tmp'))
env['PATH']=str(sdk/'bin')+os.pathsep+env['PATH']

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def save():
    (out/'native-results.json').write_text(json.dumps(record,indent=2)+'\n')
def run(label,args,cwd=root,expected=0):
    record['stage']=label;save();start=time.monotonic()
    argv=list(map(str,args))
    with (out/(label+'.log')).open('wb') as log:
        process=subprocess.run(argv,cwd=cwd,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=420)
    record['commands'].append({'label':label,'argv':argv,'cwd':str(cwd),'returncode':process.returncode,'seconds':time.monotonic()-start})
    save()
    assert process.returncode==expected,(label,process.returncode)
try:
    record=json.loads((out/'attempt1-native-results.json').read_text())
    assert record['status']=='FAIL' and record['stage']=='Release-project-protocol'
    assert 'FILE_ATTRIBUTE_NORMAL' in (out/'attempt1-Release-project-protocol.log').read_text()
    record.pop('error')
    record['status']='running'
    record['retained_failure']='attempt1-native-results.json'
    expected=record['source_inputs_sha256']
    for name,digest in expected.items():assert sha(root/name)==digest,name
    target=root/'tests/project/protocol.py'
    shutil.copyfile(out/'corrected-protocol.py',target)
    assert sha(target)==(out/'corrected-protocol.sha256').read_text().strip()
    expected['tests/project/protocol.py']=sha(target)
    for binary,digest in record['binaries'].items():assert sha(binary)==digest,binary
    prior=json.loads((previous/'final-native-results.json').read_text())
    servers=json.loads((previous/'final-server-native-results.json').read_text())
    assert prior['status']==servers['status']=='PASS'
    for profile in ('Release','Debug'):
        b=base/'build'/profile
        old_obj=old/'build'/profile
        dest=root/'build'/profile
        if profile=='Release':
            server=b/'voidls-slice3-preflight-review.exe'
        else:
            dest.mkdir(parents=True)
            for field in ('_new_binary_sha256','_baseline_binary_sha256'):
                for path,digest in prior[profile+field].items():assert sha(path)==digest,path
            baseline_server=b/'voidls-slice3-final.exe'
            assert sha(baseline_server)==servers[profile+'_server_sha256']
            run(profile+'-original-preflight',[sys.executable,out/'probe.py',baseline_server,out/(profile+'-original-preflight.json')])
            run(profile+'-original-regression',[sys.executable,root/'tests/project/protocol.py',baseline_server,out/(profile+'-original-regression.json')],expected=1)
            flags=['/nologo','/std:c++20','/EHsc','/MT','/GR-','/W4','/permissive-','/D_ITERATOR_DEBUG_LEVEL=0','/DWIN32_LEAN_AND_MEAN','/DNOMINMAX','/D_WIN32_WINNT=0x0A00','/DVOID_COMPILER_VERSION="0.1.0"','/DVOID_STDLIB_ROOT="'+(root/'stdlib').as_posix()+'"','/I'+str(root/'include'),'/I'+str(root/'third_party/tomlplusplus'),'/imsvc'+str(sdk/'include')]
            flags+=['/O2','/DNDEBUG'] if profile=='Release' else ['/Od','/Zi']
            for name,source in [('compiler','src/frontend/compiler.cpp'),('sessions','tests/project/sessions.cpp')]:
                run(profile+'-'+name+'-compile',[sdk/'bin/clang-cl.exe',*flags,'/c',root/source,'/Fo'+str(dest/(name+'.obj')),'/Fd'+str(dest/(name+'.pdb'))])
            for name in ('project','source','tooling','backend','identity','lsp-final'):
                path=old_obj/(name+'.obj');record['reused_inputs'][str(path)]=sha(path)
            rc=re.search(r'^CMAKE_RC_COMPILER:FILEPATH=(.*)$',(previous/(profile+'-CMakeCache.txt')).read_text(),re.M)[1]
            def link(name,text):
                rsp=dest/(name+'.rsp');rsp.write_text(text);shutil.copyfile(rsp,out/(profile+'-'+name+'.rsp'))
                work=dest/name;work.mkdir()
                run(profile+'-'+name,[cmake,'-E','vs_link_exe','--msvc-ver=1944','--intdir='+str(work),'--rc='+rc,'--mt='+str(sdk/'bin/llvm-mt.exe'),'--manifests',base/'cmake/LongPathAware.manifest','--',sdk/'bin/lld-link.exe','@'+str(rsp)],b)
            server=b/'voidls-slice3-preflight-review.exe'
            assert not server.exists()
            rsp=(previous/(profile+'-final-server.rsp')).read_text().replace(str(baseline_server),str(server)).replace(str(old_obj/'compiler.obj'),str(dest/'compiler.obj')).replace(str(old_obj/'lsp-final-link.pdb'),str(dest/'lsp-preflight-link.pdb'))
            link('server-link',rsp)
            sessions=dest/'sessions.exe'
            rsp=(old_obj/'sessions-link.rsp').read_text().replace(str(old_obj/'sessions.exe'),str(sessions)).replace(str(old_obj/'compiler.obj'),str(dest/'compiler.obj')).replace(str(old_obj/'sessions.obj'),str(dest/'sessions.obj')).replace(str(old_obj/'sessions-link.pdb'),str(dest/'sessions-link.pdb'))
            link('sessions-link',rsp)
            for binary in (server,sessions):record['binaries'][str(binary)]=sha(binary)
            run(profile+'-sessions',[sessions])
        # The selection and standalone CLI are the same verified native S3 inputs.
        run(profile+'-project-protocol-corrected',[sys.executable,root/'tests/project/protocol.py',server,out/(profile+'-project-protocol-corrected.json')])
        for name in ('protocol','stage7','stage8','sprint2'):
            args=[sys.executable,root/('tests/lsp/'+name+'.py'),server]
            if name=='protocol':args.append(dest/'legacy-fixtures')
            if name=='stage8':args.append(b/'voidc-slice3.exe')
            run(profile+'-legacy-'+name,args)
    for path,digest in record['reused_inputs'].items():assert sha(path)==digest,path
    record['status']='PASS'
except BaseException:
    record['status']='FAIL';record['error']=traceback.format_exc()
finally:save()
