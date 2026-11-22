"""Native /MT launcher arguments, parent-loss cleanup and isolated HKCU PATH.

RegOverridePredefKey confines every registry mutation to a disposable volatile
subkey. This never changes the real user's Environment key or tool installation.
"""
import argparse
import ctypes
from ctypes import wintypes
import json
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import re

ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--sdk',type=Path,required=True)
p.add_argument('--evidence',type=Path)
a=p.parse_args()
assert sys.platform=='win32','Native Windows execution is required'
kernel=ctypes.WinDLL('kernel32',use_last_error=True)
kernel.OpenProcess.argtypes=[wintypes.DWORD,wintypes.BOOL,wintypes.DWORD]
kernel.OpenProcess.restype=wintypes.HANDLE
kernel.WaitForSingleObject.argtypes=[wintypes.HANDLE,wintypes.DWORD]
kernel.CloseHandle.argtypes=[wintypes.HANDLE]
receipts=[]

with tempfile.TemporaryDirectory(prefix='VOIDUP launcher 雪 ') as folder:
    work=Path(folder);source=work/'probe.cpp'
    source.write_text('#define VOIDUP_BOOTSTRAP_ID "test"\n#define wmain bootstrap_wmain\n#include '+json.dumps(str(ROOT/'apps/voidup/launcher.cpp'))+r'''
#undef wmain
int wmain(int argc,wchar_t** argv) try {
    require(argc>=3,"Missing probe arguments");
    if(std::wstring(argv[1])==L"--storage") {
        auto path=fs::path(argv[2]);directory(path);
        for(int i=0;i<7;++i){path/=L"ordinary Unicode storage directory beyond MAX_PATH";directory(path);}
        write_new(path/L"source", "preserved bytes");safe(path/L"source",false);
        replace(path/L"source",path/L"target");
        require(read(path/L"target",100)=="preserved bytes","Extended path copy/replace failed");return 0;
    }
    if(std::wstring(argv[1])==L"--path") {
        auto leaf=L"Software\\VOIDUPQualification-"+std::to_wstring(GetCurrentProcessId());
        HKEY fixture{};require(RegCreateKeyExW(HKEY_CURRENT_USER,leaf.c_str(),0,nullptr,REG_OPTION_VOLATILE,KEY_ALL_ACCESS,nullptr,&fixture,nullptr)==ERROR_SUCCESS,"Cannot create fixture hive");
        struct restore {HKEY key;std::wstring leaf;~restore(){RegOverridePredefKey(HKEY_CURRENT_USER,nullptr);RegCloseKey(key);RegDeleteTreeW(HKEY_CURRENT_USER,leaf.c_str());}} guard{fixture,leaf};
        require(RegOverridePredefKey(HKEY_CURRENT_USER,fixture)==ERROR_SUCCESS,"Cannot isolate HKCU");
        HKEY environment{};require(RegCreateKeyExW(fixture,L"Environment",0,nullptr,REG_OPTION_VOLATILE,KEY_ALL_ACCESS,nullptr,&environment,nullptr)==ERROR_SUCCESS,"Cannot create fixture environment");
        auto type=std::wstring(argv[3])==L"expand"?REG_EXPAND_SZ:REG_SZ;
        std::wstring before=argv[4];
        require(RegSetValueExW(environment,L"Path",0,type,reinterpret_cast<const BYTE*>(before.c_str()),DWORD((before.size()+1)*sizeof(wchar_t)))==ERROR_SUCCESS,"Cannot seed PATH");
        require(RegSetValueExW(environment,L"Unrelated",0,REG_DWORD,reinterpret_cast<const BYTE*>(&type),sizeof type)==ERROR_SUCCESS,"Cannot seed unrelated value");
        integrate_path(fs::path(argv[2]));integrate_path(fs::path(argv[2]));
        DWORD actual_type=0,size=65536;std::wstring after(32768,L'\0');
        require(RegQueryValueExW(environment,L"Path",nullptr,&actual_type,reinterpret_cast<BYTE*>(after.data()),&size)==ERROR_SUCCESS&&actual_type==type,"Registry type changed");
        after.resize(size/sizeof(wchar_t)-1);DWORD unrelated=0;size=sizeof unrelated;
        require(RegQueryValueExW(environment,L"Unrelated",nullptr,nullptr,reinterpret_cast<BYTE*>(&unrelated),&size)==ERROR_SUCCESS&&unrelated==type,"Unrelated value changed");
        RegCloseKey(environment);std::cout<<fs::path(after).u8string();return 0;
    }
    std::vector<std::string> arguments;
    for(int i=3;i<argc;++i){auto size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[i],-1,nullptr,0,nullptr,nullptr);require(size>0,"Invalid probe argument");std::string text(size,'\0');WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,argv[i],-1,text.data(),size,nullptr,nullptr);text.pop_back();arguments.push_back(text);}
    return run(fs::path(argv[2]),arguments);
} catch(const std::exception& e){std::cerr<<e.what();return 1;}
''',encoding='utf-8')
    for profile,flags in [('Debug',['/Od','/Zi']),('Release',['/O2'])]:
        binary=work/(profile+'.exe')
        subprocess.run([str(a.sdk/'bin/clang-cl.exe'),'/nologo','/std:c++17','/EHsc','/MT',*flags,
                        '/clang:-fuse-ld=lld',str(source),'/Fe:'+str(binary),'/link','/STACK:1048576',
                        'advapi32.lib','shell32.lib','ole32.lib','user32.lib'],cwd=work,check=True)
        headers=subprocess.check_output([str(a.sdk/'bin/llvm-readobj.exe'),'--file-headers','--coff-imports',str(binary)],text=True)
        assert 'IMAGE_FILE_MACHINE_AMD64' in headers and 'SizeOfStackReserve: 1048576' in headers,headers
        imports=sorted(set(re.findall(r'^\s*Name: (\S+\.dll)\s*$',headers,re.M|re.I)))
        assert imports and not any(re.fullmatch(r'(?:msvcrt|msvcr\d.*|msvcp\d.*|vcruntime\d.*|ucrtbased?|api-ms-win-crt-.*)\.dll',n,re.I) for n in imports),imports
        arguments=['','雪😀','literal $() & %PATH%', 'a"b', 'ends\\', 'two \\\" quotes']
        result=subprocess.run([binary,'--run',sys.executable,'-c','import json,sys; print(json.dumps(sys.argv[1:]))',*arguments],capture_output=True,check=True)
        assert json.loads(result.stdout)==arguments,result.stdout
        subprocess.run([binary,'--storage',work/(profile+'-long-storage')],capture_output=True,check=True)
        candidate=work/'tools/bin'
        os.environ['VOIDUP_PATH_FIXTURE']=str(candidate)
        for kind,original,expected in [
            ('string','C:\\Unrelated',str(candidate)+';C:\\Unrelated'),
            ('string','"'+str(candidate).replace('\\','/').upper()+'/";C:\\Other',None),
            ('expand','%VOIDUP_PATH_FIXTURE%;C:\\Other',None),
            ('string','%VOIDUP_PATH_FIXTURE%',str(candidate)+';%VOIDUP_PATH_FIXTURE%')]:
            result=subprocess.run([binary,'--path',candidate,kind,original],capture_output=True)
            assert result.returncode==0,(result.returncode,result.stdout,result.stderr)
            assert result.stdout.decode('utf-8')==(original if expected is None else expected),result.stdout
        manager=work/'manager.py';ready=work/(profile+'.ready')
        manager.write_text('import subprocess,sys,os,json\nfrom pathlib import Path\np=subprocess.Popen([sys.executable,"-c","import time;time.sleep(120)"])\nPath(sys.argv[1]).write_text(json.dumps([os.getpid(),p.pid]))\np.wait()\n')
        parent=subprocess.Popen([binary,'--run',sys.executable,manager,ready],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        handles=[]
        try:
            deadline=time.monotonic()+20
            while not ready.exists() or not ready.stat().st_size:
                assert parent.poll() is None and time.monotonic()<deadline
                time.sleep(.01)
            for pid in json.loads(ready.read_text()):
                handle=kernel.OpenProcess(0x00100000,False,pid)
                assert handle,ctypes.get_last_error()
                handles.append(handle)
            parent.kill();parent.wait(timeout=10)
            assert all(kernel.WaitForSingleObject(handle,10000)==0 for handle in handles),'Descendant survived parent loss'
        finally:
            if parent.poll() is None:parent.kill()
            parent.communicate(timeout=15)
            for handle in handles:kernel.CloseHandle(handle)
        print(profile+': PASS literal Unicode arguments, isolated registry preservation/idempotence, long storage paths, native descendant termination')
        receipts.append({'profile':profile,'sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'bytes':binary.stat().st_size,'pe_imports':imports,'stack_bytes':1048576,'static_crt':True})
if a.evidence:
    a.evidence.write_text(json.dumps({'status':'PASS','source_sha256':hashlib.sha256((ROOT/'apps/voidup/launcher.cpp').read_bytes()).hexdigest(),'clang_cl_sha256':hashlib.sha256((a.sdk/'bin/clang-cl.exe').read_bytes()).hexdigest(),'profiles':receipts,'actual_user_environment_modified':False},indent=2),encoding='utf-8')
