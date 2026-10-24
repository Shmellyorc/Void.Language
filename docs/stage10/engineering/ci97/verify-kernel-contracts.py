"""Source-extracted Darwin predicate checks; not native macOS execution."""
from pathlib import Path
import importlib.util,json,subprocess,sys,tempfile
root=Path(__file__).resolve().parents[4];base=Path(__file__).resolve().parent
text=(root/'src/runtime/console_keyboard.inc').read_text()
body=text[text.index('    static bool same_mode'):text.index('\n#endif\n    Error save_mode')]
source='''#include <algorithm>
#include <termios.h>
#include "tests/runtime/console_posix.hpp"
'''+body+'''
int main(){
 termios original{};original.c_lflag=ICANON;auto pending=original;pending.c_lflag|=PENDIN;
 if(!same_mode(original,pending))return 1;
 auto changed=pending;changed.c_lflag^=ECHO;if(same_mode(original,changed))return 2;
 changed=pending;changed.c_iflag^=ICRNL;if(same_mode(original,changed))return 3;
 changed=pending;changed.c_cc[VMIN]=7;if(same_mode(original,changed))return 4;
 using namespace console_native_test;
 Flags before{},after{};before.values={0,0,0,1,0,1};after=before;after.values[3]|=0x10000;
 if(!same_flags(before,after))return 5;
 auto bad=after;bad.values[2]^=FD_CLOEXEC;if(same_flags(before,bad))return 6;
 bad=after;bad.values[3]^=O_NONBLOCK;if(same_flags(before,bad))return 7;
 bad=after;bad.values[3]^=O_APPEND;if(same_flags(before,bad))return 8;
 bad=after;bad.values[3]^=O_ACCMODE;if(same_flags(before,bad))return 9;
 bad=after;bad.errors[1]=EBADF;if(same_flags(before,bad))return 10;
 if(same_flags(after,before))return 11;
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='void-ci97-contract-') as temp:
 path=Path(temp);(path/'probe.cpp').write_text(source)
 compiler=root/'.toolchains/ci-investigation/LLVM-19.1.7-Linux-X64/bin/clang++'
 command=[str(compiler),'-std=c++20','-D__APPLE__','-I'+str(root),str(path/'probe.cpp'),'-o',str(path/'probe')]
 subprocess.run(command,check=True);subprocess.run([path/'probe'],check=True)
 # The original byte-for-byte predicate rejects XNU's documented transition.
 old=body.replace('        local_difference&=~static_cast<tcflag_t>(PENDIN);','')
 (path/'probe.cpp').write_text(source.replace(body,old))
 subprocess.run(command,check=True)
 assert subprocess.run([path/'probe']).returncode==1
sys.path.insert(0,str(root/'tests/runtime'))
import console_posix
mode=[0,0,0,termios_value:=256,0,0,[b'\0']*32]
# Use Python's native constant in the model, without claiming its Darwin ABI value.
import termios
other=[*mode[:6],list(mode[6])];other[3]|=termios.PENDIN
platform=sys.platform
try:
 sys.platform='darwin';assert console_posix.same_termios(mode,other)
 for field,bit in [(0,termios.ICRNL),(1,termios.OPOST),(2,termios.CLOCAL),(3,termios.ECHO)]:
  bad=[*other[:6],list(other[6])];bad[field]^=bit;assert not console_posix.same_termios(mode,bad)
 bad=[*other[:6],list(other[6])];bad[6][0]=b'X';assert not console_posix.same_termios(mode,bad)
 sys.platform='linux';assert not console_posix.same_termios(mode,other)
finally:sys.platform=platform
print('PASS: original mode predicate rejects PENDIN; repaired predicate tolerates only that kernel state; descriptor access/nonblock/append/cloexec/errors and Python attributes remain checked.')
print('These are source-extracted/model checks, not native Darwin observations.')
