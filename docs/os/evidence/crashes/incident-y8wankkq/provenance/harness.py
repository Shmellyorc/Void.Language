"""All generated compiler/JIT/native subprocesses retain exact source on abnormal exit."""
from pathlib import Path
import os
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tests/text_encoding'))
from crash import run as preserving_run, crashed, preserve
os.environ.setdefault('VOID_CRASH_EVIDENCE', str(ROOT / 'docs/random/evidence/crashes'))
checks = 0

def stack():
    import resource
    resource.setrlimit(resource.RLIMIT_STACK, (1024 * 1024, 1024 * 1024))

def run(command, cwd, expected=None, status=0, sources=(), small=False, timeout=180, **kwargs):
    global checks
    args = list(map(str, command))
    result = preserving_run(args, cwd=cwd, capture_output=True, timeout=timeout,
                            preexec_fn=stack if small else None,
                            source_inputs=sources,
                            provenance_inputs=tuple(Path(__file__).parent.glob('*.*')), **kwargs)
    unexpected_internal = result.returncode != status and b'VOID IR:' in result.stderr
    if (result.returncode not in (0, 1) and not crashed(result.returncode)) or unexpected_internal:
        preserve(args, cwd, result, sources, tuple(Path(__file__).parent.glob('*.*')))
    assert result.returncode == status, (args, result.returncode, result.stdout, result.stderr)
    assert b'warning' not in result.stderr.lower(), result.stderr
    if expected is not None:
        assert result.stdout == expected, (args, result.stdout[:4000], expected[:4000], result.stderr)
    checks += 1
    return result
