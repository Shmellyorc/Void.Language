"""Linux test-only Landlock launcher: the checkout and build tree are inaccessible."""
import ctypes, errno, os, pathlib, sys
libc=ctypes.CDLL(None,use_errno=True)
def syscall(number,*args):
 r=libc.syscall(ctypes.c_long(number),*args)
 if r<0: raise OSError(ctypes.get_errno(),os.strerror(ctypes.get_errno()))
 return r
abi=syscall(444,ctypes.c_void_p(0),ctypes.c_size_t(0),ctypes.c_uint(1))
access=(1<<13)-1
if abi>=2:access|=1<<13
if abi>=3:access|=1<<14
if abi>=5:access|=1<<15
class Ruleset(ctypes.Structure):_fields_=[('access',ctypes.c_uint64)]
class Beneath(ctypes.Structure):_fields_=[('access',ctypes.c_uint64),('parent',ctypes.c_int32)]
attr=Ruleset(access);ruleset=syscall(444,ctypes.byref(attr),ctypes.c_size_t(ctypes.sizeof(attr)),ctypes.c_uint(0))
for path,rights in [('/',1<<3)]+[(p,access) for p in [sys.argv[1],'/usr','/lib','/lib64','/etc','/dev','/proc']]:
 if not os.path.exists(path):continue
 fd=os.open(path,os.O_PATH|os.O_CLOEXEC)
 try:
  rule=Beneath(rights,fd);syscall(445,ctypes.c_int(ruleset),ctypes.c_int(1),ctypes.byref(rule),ctypes.c_uint(0))
 finally:os.close(fd)
if libc.prctl(38,1,0,0,0):raise OSError(ctypes.get_errno(),'no_new_privs')
syscall(446,ctypes.c_int(ruleset),ctypes.c_uint(0));os.close(ruleset)
try:
 with open(pathlib.Path(__file__).resolve().parents[2]/'CMakeLists.txt','rb') as proof:proof.read(1)
except PermissionError:pass
else:raise RuntimeError('checkout was not isolated')
os.execv(sys.argv[2],sys.argv[2:])
