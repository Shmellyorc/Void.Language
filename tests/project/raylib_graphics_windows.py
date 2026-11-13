"""Windows native pixel/input/module controls reused from the S7 qualification."""
from ctypes import WinDLL, sizeof, c_void_p, c_size_t, byref, create_string_buffer, create_unicode_buffer, string_at
from ctypes.wintypes import HWND, HDC, HANDLE, DWORD, UINT, INT, BOOL, RECT, POINT, LPCWSTR, LPVOID, WCHAR, BYTE, WPARAM, LPARAM
from pathlib import Path
import hashlib, json, os, re, shutil, struct, subprocess, sys, time, zipfile


sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
user=WinDLL('user32',use_last_error=True)
gdi=WinDLL('gdi32',use_last_error=True)
kernel=WinDLL('kernel32',use_last_error=True)

def bind(lib,name,res,*args):
    f=getattr(lib,name)
    f.restype=res
    f.argtypes=args
    return f

get_dc=bind(user,'GetDC',HDC,HWND)
release_dc=bind(user,'ReleaseDC',INT,HWND,HDC)
compatible=bind(gdi,'CreateCompatibleDC',HDC,HDC)
dib=bind(gdi,'CreateDIBSection',HANDLE,HDC,LPVOID,UINT,LPVOID,HANDLE,DWORD)
select=bind(gdi,'SelectObject',HANDLE,HDC,HANDLE)
bitblt=bind(gdi,'BitBlt',BOOL,HDC,INT,INT,INT,INT,HDC,INT,INT,DWORD)
delete=bind(gdi,'DeleteObject',BOOL,HANDLE)
delete_dc=bind(gdi,'DeleteDC',BOOL,HDC)
find=bind(user,'FindWindowW',HWND,LPCWSTR,LPCWSTR)
pid_for=bind(user,'GetWindowThreadProcessId',DWORD,HWND,LPVOID)
cursor=bind(user,'SetCursorPos',BOOL,INT,INT)
rect_for=bind(user,'GetClientRect',BOOL,HWND,LPVOID)
coords=bind(user,'ClientToScreen',BOOL,HWND,LPVOID)
close=bind(user,'PostMessageW',BOOL,HWND,UINT,WPARAM,LPARAM)
focus=bind(user,'SetForegroundWindow',BOOL,HWND)
is_visible=bind(user,'IsWindowVisible',BOOL,HWND)
keyboard=bind(user,'keybd_event',None,BYTE,BYTE,DWORD,c_size_t)
user.SetProcessDPIAware()

open_process=bind(kernel,'OpenProcess',HANDLE,DWORD,BOOL,DWORD)
enum_modules=bind(kernel,'K32EnumProcessModulesEx',BOOL,HANDLE,LPVOID,DWORD,LPVOID,DWORD)
module_path=bind(kernel,'K32GetModuleFileNameExW',DWORD,HANDLE,HANDLE,LPVOID,DWORD)
close_handle=bind(kernel,'CloseHandle',BOOL,HANDLE)

def modules(pid):
    # ToolHelp's module snapshot substituted '?' in the Unicode relocation path.
    process=open_process(0x410,False,pid)
    assert process
    try:
        entries=(HANDLE*1024)()
        size=DWORD()
        assert enum_modules(process,entries,sizeof(entries),byref(size),3)
        assert 0<size.value<=sizeof(entries)
        paths=[]
        for module in entries[:size.value//sizeof(HANDLE)]:
            path=create_unicode_buffer(32768)
            length=module_path(process,module,path,len(path))
            assert 0<length<len(path)
            paths.append(path.value)
        return paths
    finally:
        close_handle(process)


def geometry(hwnd):
    rect, origin=RECT(),POINT()
    assert rect_for(hwnd,byref(rect)) and coords(hwnd,byref(origin))
    return origin.x, origin.y, rect.right, rect.bottom


def capture(hwnd,path):
    x,y,w,h=geometry(hwnd)
    assert (w,h)==(640,360),(w,h)
    header=struct.pack('<IiiHHIIIIII',40,w,-h,1,32,0,w*h*4,0,0,0,0)
    bmi=create_string_buffer(header)
    screen=get_dc(None)
    assert screen
    mem=compatible(screen)
    assert mem
    bits=c_void_p()
    bitmap=dib(mem,bmi,0,byref(bits),None,0)
    assert bitmap and bits.value
    old=select(mem,bitmap)
    try:
        assert bitblt(mem,0,0,w,h,screen,x,y,0x00CC0020)
        pixels=string_at(bits,w*h*4)
        path.write_bytes(struct.pack('<2sIHHI',b'BM',54+len(pixels),0,0,54)+header+pixels)
    finally:
        select(mem,old)
        delete(bitmap)
        delete_dc(mem)
        release_dc(None,screen)
    # Allow small color quantization differences in the guest framebuffer.
    colors={'pink':(255,109,194),'green':(0,158,47),'orange':(255,161,0)}
    answer={}
    for name,(r,g,b) in colors.items():
        points=[(i//4%w,i//4//w) for i in range(0,len(pixels),4)
                if abs(pixels[i]-b)<=10 and abs(pixels[i+1]-g)<=10 and abs(pixels[i+2]-r)<=10]
        assert len(points)>90,(name,len(points))
        answer[name]=[round(sum(p[d] for p in points)/len(points),2) for d in (0,1)]
    return answer


def run(cwd,*args,environment=None,status=0):
    cmd=list(map(str,args))
    print(cmd,flush=True)
    p=subprocess.run(cmd,cwd=cwd,env=environment or env,capture_output=True,timeout=180)
    output=(p.stdout+p.stderr).decode('utf-8',errors='replace')
    print(output,flush=True)
    assert p.returncode==status,(p.returncode,output)
    return output


def visible(label,cwd,command,native,environment,out):
    out.mkdir()
    print(label,list(map(str,command)),flush=True)
    log=out/'process.txt'
    with log.open('wb') as stream:
        p=subprocess.Popen(list(map(str,command)),cwd=cwd,env=environment,stdout=stream,stderr=subprocess.STDOUT)
        hwnd=None
        try:
            for _ in range(300):
                candidate=find(None,'VOID Raylib Gallery')
                owner=DWORD()
                if candidate and pid_for(candidate,byref(owner)) and owner.value==p.pid and is_visible(candidate):
                    hwnd=candidate
                    break
                if p.poll() is not None:
                    raise AssertionError(log.read_text(errors='replace'))
                time.sleep(.10)
            assert hwnd,log.read_text(errors='replace')
            for _ in range(600):
                if 'RAYLIB ready' in log.read_text(errors='replace'):break
                assert p.poll() is None,log.read_text(errors='replace')
                time.sleep(.1)
            assert 'RAYLIB ready' in log.read_text(errors='replace'),log.read_text(errors='replace')
            loaded=[path for path in modules(p.pid) if Path(path).name.lower() in ('raylib.dll','void_raylib.dll')]
            assert {Path(path).resolve() for path in loaded} == {native.resolve()/name for name in ('raylib.dll','void_raylib.dll')}, loaded
            focus(hwnd)
            x,y,w,h=geometry(hwnd)
            assert cursor(x+140,y+125)
            time.sleep(.25)
            before=capture(hwnd,out/'frame-before.bmp')
            assert cursor(x+475,y+235)
            keyboard(0x27,0,0,0)
            try:
                time.sleep(.33)
                after=capture(hwnd,out/'frame-after.bmp')
            finally:
                keyboard(0x27,0,2,0)
            assert close(hwnd,0x10,0,0)
            code=p.wait(timeout=15)
            output=log.read_text(errors='replace')
            assert code==0 and output.count('RAYLIB owner released texture and window')==1,(code,output)
            match=re.search(r'RAYLIB frames (\d+) input frames (\d+)',output)
            assert match and int(match[1])>5 and int(match[2])>0,output
            assert abs(before['pink'][0]-after['pink'][0])>100,(before,after)
            assert abs(before['green'][0]-after['green'][0])>4,(before,after)
            assert abs(before['orange'][0]-after['orange'][0])>4,(before,after)
            (out/'result.json').write_text(json.dumps(dict(exit=code,frames=int(match[1]),input_frames=int(match[2]),
                before=before,after=after,modules=loaded,sha256={p.name:sha(p) for p in native.glob('*.dll')}),
                indent=2)+'\n')
        finally:
            if p.poll() is None:
                if hwnd: close(hwnd,0x10,0,0)
                try: p.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    p.kill()
                    p.wait(timeout=5)



def qualify(command,native,cwd,environment,output,**options):
    visible('raylib',cwd,command,native,environment,output)
    return json.loads((output/'result.json').read_text())
