"""Check that a terminal enum result cannot mask the original cleanup failure."""
from pathlib import Path
import json
import subprocess
import sys
import tempfile

compiler, evidence = (Path(value).resolve() for value in sys.argv[1:])
source = Path(__file__).resolve().with_name('cleanup-failure.void')
rows = []


def invoke(argv, status, program=False):
    args = list(map(str, argv))
    result = subprocess.run(args, capture_output=True, timeout=120)
    row = {'argv': args, 'status': result.returncode, 'stdout': result.stdout.decode(),
           'stderr': result.stderr.decode()}
    rows.append(row)
    assert result.returncode == status, row
    if program:
        assert result.stdout == b'STORAGE CLEANUP\n', row
        assert b'Division by zero' in result.stderr, row


try:
    with tempfile.TemporaryDirectory(prefix='VOID S6 cleanup failure ') as temporary:
        for name, options in [('normal', ['--no-opt']), ('o2', [])]:
            binary = Path(temporary)/name
            invoke([compiler, 'run', '--no-cache', source, *options], 1, True)
            invoke([compiler, 'build', source, '-o', binary, *options], 0)
            invoke([binary], 1, True)
finally:
    evidence.write_text(json.dumps(rows, indent=2)+'\n')
