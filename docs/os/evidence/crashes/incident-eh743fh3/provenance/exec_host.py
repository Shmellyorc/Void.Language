"""Independent pre-exec process identity, preserved by exec into JIT host or AOT app."""
import os
from pathlib import Path
import sys
exe=Path(sys.argv[1]).resolve()
os.environ['VOID_OS_EXPECT_PID']=str(os.getpid())
os.environ['VOID_OS_EXPECT_IMAGE']=str(exe)
# Existing path.absolute appends the empty component and retains a trailing separator.
os.environ['VOID_OS_EXPECT_CWD']=os.getcwd().rstrip(os.sep)+os.sep
# Test-owned start gate proves two child PIDs are simultaneously live before exec.
if 'VOID_OS_PID_GATE' in os.environ:
    import time
    gate=Path(os.environ['VOID_OS_PID_GATE'])
    marker=Path(os.environ['VOID_OS_PID_MARKER'])
    marker.write_text(str(os.getpid()))
    deadline=time.monotonic()+30
    while not gate.exists():
        if time.monotonic()>deadline:raise RuntimeError('PID fixture start gate timed out')
        time.sleep(0.01)
os.execv(str(exe),[str(exe),*sys.argv[2:]])
