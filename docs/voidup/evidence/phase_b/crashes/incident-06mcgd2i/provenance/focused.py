"""Run preserved source fixtures through actual compiler modes."""
from pathlib import Path
import subprocess
import sys
import tempfile
from crash import run

compiler = Path(sys.argv[1]).resolve()
fixture = Path(sys.argv[2]).resolve()
with tempfile.TemporaryDirectory(prefix='VOID text focused ') as directory:
    cwd = Path(directory)
    for arguments in ([compiler, 'check', fixture], [compiler, 'run', fixture, '--no-opt'],
                      [compiler, 'run', fixture]):
        result = run(arguments, cwd=cwd, capture_output=True, timeout=300)
        assert result.returncode == 0, (arguments, result.returncode, result.stdout, result.stderr)
        assert b'warning' not in result.stderr.lower(), result.stderr
        print('PASS', ' '.join(map(str, arguments)), flush=True)
