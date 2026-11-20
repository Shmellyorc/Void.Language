"""G88 stable library contracts and canonical specification examples: JIT/AOT O0/O2."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from test_output import output_directory

compiler, report = (Path(p).resolve() for p in sys.argv[1:])
records = []


def limits():
    import resource
    resource.setrlimit(resource.RLIMIT_STACK, (1048576, 1048576))
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def run(args, expected=None, status=0):
    result = subprocess.run(list(map(str, args)), capture_output=True, text=True,
                            timeout=180, preexec_fn=limits if os.name == 'posix' else None)
    row = dict(command=list(map(str, args)), exit=result.returncode,
               stdout=result.stdout, stderr=result.stderr)
    records.append(row)
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(dict(stack_bytes=1048576, compiler_sha256=hashlib.sha256(compiler.read_bytes()).hexdigest(),
                                      records=records), indent=2) + '\n')
    assert result.returncode == status, row
    if expected is not None:
        assert result.stdout == expected, row
    assert not any(word in result.stderr for word in ('AddressSanitizer', 'LeakSanitizer', 'runtime error:')), row


source = Path(__file__).with_name('stdlib.void')
with tempfile.TemporaryDirectory(prefix='g88-source-', dir=output_directory()) as directory:
    work = Path(directory)
    run([compiler, 'check', source])
    for mode, flags in [('O0', ['--no-opt']), ('O2', [])]:
        run([compiler, 'run', source, '--no-cache', *flags], 'G88 standard library contracts passed\n')
        exe = work / ('stdlib-' + mode + ('.exe' if os.name == 'nt' else ''))
        run([compiler, 'build', source, '-o', exe, *flags])
        run([exe], 'G88 standard library contracts passed\n')
    # Every Bible code block is a standalone modern source example. Checking and
    # executing them prevents the canonical narrative retaining invalid syntax.
    bible = (ROOT / 'docs/LANGUAGE_BIBLE.md').read_text()
    examples = re.findall(r'```void\n(.*?)```', bible, re.S)
    assert len(examples) >= 8
    for index, text in enumerate(examples):
        file = work / f'bible-{index}.void'
        file.write_text(text)
        run([compiler, 'check', file])
        run([compiler, 'run', file, '--no-cache'])
    # Independent diagnostics for nominal arguments and checked discriminants.
    invalid = work / 'invalid.void'
    invalid.write_text('use io as io\nconst wrong = 99 as io.open_mode\nfunc main():\n    pass\n')
    run([compiler, 'check', invalid], status=1)
print(f'G88 library and specification source: {len(records)} process checks passed')
