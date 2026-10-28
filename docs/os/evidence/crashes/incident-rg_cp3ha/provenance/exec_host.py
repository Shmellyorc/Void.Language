"""Independent pre-exec process identity, preserved by exec into JIT host or AOT app."""
import os
from pathlib import Path
import sys
exe=Path(sys.argv[1]).resolve()
os.environ['VOID_OS_EXPECT_PID']=str(os.getpid())
os.environ['VOID_OS_EXPECT_IMAGE']=str(exe)
# Existing path.absolute appends the empty component and retains a trailing separator.
os.environ['VOID_OS_EXPECT_CWD']=os.getcwd().rstrip(os.sep)+os.sep
os.execv(str(exe),[str(exe),*sys.argv[2:]])
