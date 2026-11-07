"""Reuse the existing subprocess/crash/stack harness for the OS acceptance fixtures."""
from pathlib import Path
import os
import sys
ROOT = Path(__file__).resolve().parents[2]
os.environ.setdefault('VOID_CRASH_EVIDENCE', str(ROOT / 'docs/os/evidence/crashes'))
sys.path.insert(0, str(ROOT / 'tests/random'))
import harness
def run(command, cwd, *args, **kwargs):
    try:
        return harness.run(command, cwd, *args, **kwargs)
    except AssertionError as error:
        # Preserve expected-status/output failures as well as the shared harness's crashes.
        # This does not turn deliberately rejected negative fixtures into failures.
        import subprocess
        detail = error.args[0] if error.args else ()
        status = detail[1] if isinstance(detail, tuple) and len(detail)>1 and isinstance(detail[1], int) else 0
        stdout = detail[2] if status and len(detail)>2 else detail[1] if isinstance(detail, tuple) and len(detail)>1 else b''
        stderr = detail[-1] if isinstance(detail, tuple) else detail
        result = subprocess.CompletedProcess(list(map(str,command)), status,
            stdout if isinstance(stdout,bytes) else b'', stderr if isinstance(stderr,bytes) else repr(stderr).encode())
        harness.preserve(command, cwd, result, kwargs.get('sources',()), tuple(Path(__file__).parent.glob('*.*')))
        raise
stack = harness.stack
preserving_run = harness.preserving_run
preserve = harness.preserve
crashed = harness.crashed
def __getattr__(name):
    return getattr(harness, name)
