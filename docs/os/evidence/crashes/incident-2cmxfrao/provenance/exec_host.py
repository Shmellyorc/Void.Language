"""Independent process identity: POSIX exec or a Windows child PID capture/handshake."""
import os
from pathlib import Path
import sys
import time
exe=Path(sys.argv[1]).resolve()
os.environ['VOID_OS_EXPECT_IMAGE']=str(exe)
os.environ['VOID_OS_EXPECT_CWD']=os.getcwd().rstrip(os.sep)+os.sep

def gate(process_id):
    if 'VOID_OS_PID_GATE' not in os.environ:return
    marker=Path(os.environ['VOID_OS_PID_MARKER']);marker.write_text(str(process_id))
    deadline=time.monotonic()+30
    while not Path(os.environ['VOID_OS_PID_GATE']).exists():
        if time.monotonic()>deadline:raise RuntimeError('PID fixture start gate timed out')
        time.sleep(0.01)

if os.name=='nt':
    # Windows cannot replace a process while preserving its PID. The source
    # fixture prints a private PID marker, then waits for this parent's newline.
    # Popen.pid is independent of the source query and both real child processes
    # remain alive at the optional two-process gate.
    import subprocess
    import threading
    os.environ['VOID_OS_EXPECT_PID']='__VOID_CAPTURE_PID__'
    command=[str(exe),*sys.argv[2:]]
    child=subprocess.Popen(command,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    errors=[]
    drain=threading.Thread(target=lambda:errors.append(child.stderr.read()));drain.start()
    prefix=[];seen=False;matched=True
    for line in iter(child.stdout.readline,b''):
        if line.startswith(b'__VOID_CAPTURE_PID__ '):
            seen=True
            try:reported=int(line.split()[1])
            except (ValueError,IndexError):reported=None
            matched=reported==child.pid
            prefix.append(b'true\n' if matched else b'false\n')
            try:gate(child.pid)
            finally:
                child.stdin.write(b'\n');child.stdin.flush();child.stdin.close()
            break
        prefix.append(line)
    rest=child.stdout.read();status=child.wait();drain.join()
    stdout=b''.join(prefix)+rest;stderr=b''.join(errors)
    if status or not seen or not matched:
        sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'text_encoding'))
        from crash import preserve
        result=subprocess.CompletedProcess(command,status or 1,stdout,stderr)
        preserve(command,Path.cwd(),result,provenance_inputs=(Path(__file__),))
    sys.stdout.buffer.write(stdout);sys.stderr.buffer.write(stderr)
    raise SystemExit(status if status else 0 if seen and matched else 1)

os.environ['VOID_OS_EXPECT_PID']=str(os.getpid())
gate(os.getpid())
os.execv(str(exe),[str(exe),*sys.argv[2:]])
