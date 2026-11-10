from ctypes import Structure, byref, c_void_p, c_ubyte, c_char_p, sizeof, WinDLL
from ctypes.wintypes import DWORD, HINSTANCE, HMENU, HWND, LPCWSTR, LPVOID, INT, HDC, WORD, BOOL, BYTE, UINT
from pathlib import Path
import json

user = WinDLL('user32', use_last_error=True)
gdi = WinDLL('gdi32', use_last_error=True)
gl = WinDLL('opengl32', use_last_error=True)
def bind(lib, name, result, *args):
    f = getattr(lib,name)
    f.restype=result
    f.argtypes=args
    return f
create=bind(user,'CreateWindowExW',HWND,DWORD,LPCWSTR,LPCWSTR,DWORD,INT,INT,INT,INT,HWND,HMENU,HINSTANCE,LPVOID)
destroy=bind(user,'DestroyWindow',BOOL,HWND)
getdc=bind(user,'GetDC',HDC,HWND)
releasedc=bind(user,'ReleaseDC',INT,HWND,HDC)
class PixelFormat(Structure):
    _fields_=[('size',WORD),('version',WORD),('flags',DWORD)]+[(name,BYTE) for name in
      'pixel color red reds green greens blue blues alpha alphas accum accumred accumgreen accumblue accumalpha depth stencil aux layer reserved'.split()]+[(name,DWORD) for name in ('layerMask','visibleMask','damageMask')]
choose=bind(gdi,'ChoosePixelFormat',INT,HDC,c_void_p)
setfmt=bind(gdi,'SetPixelFormat',BOOL,HDC,INT,c_void_p)
context=bind(gl,'wglCreateContext',c_void_p,HDC)
current=bind(gl,'wglMakeCurrent',BOOL,HDC,c_void_p)
delete=bind(gl,'wglDeleteContext',BOOL,c_void_p)
string=bind(gl,'glGetString',c_char_p,UINT)
hwnd=create(0,'STATIC','VOID WGL preflight',0x00cf0000,0,0,32,32,None,None,None,None)
assert hwnd
dc=getdc(hwnd)
ctx=None
try:
    assert dc
    pf=PixelFormat(size=sizeof(PixelFormat),version=1,flags=0x25,pixel=0,color=24,depth=24)
    idx=choose(dc,byref(pf))
    assert idx and setfmt(dc,idx,byref(pf))
    ctx=context(dc)
    assert ctx and current(dc,ctx)
    output={name:string(value).decode('ascii') for name,value in [('vendor',0x1f00),('renderer',0x1f01),('version',0x1f02)]}
    (Path(r'\\vboxsvr\VM\slice7-raylib')/'wgl.json').write_text(json.dumps(output,indent=2)+'\n')
finally:
    if ctx:
        current(None,None)
        delete(ctx)
    if dc:
        releasedc(hwnd,dc)
    destroy(hwnd)
