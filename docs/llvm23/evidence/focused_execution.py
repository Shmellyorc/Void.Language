"""Reproduce the six focused LLVM migration execution checks."""
from pathlib import Path
import json, os, resource, sys
ROOT = Path(__file__).resolve().parents[3]
OUT = Path(__file__).resolve().parent
os.environ['VOID_CRASH_EVIDENCE'] = str(OUT / 'crashes')
sys.path.insert(0, str(ROOT / 'tests/text_encoding'))
import crash

def stack():
    resource.setrlimit(resource.RLIMIT_STACK, (1048576, 1048576))

records = []
source = ROOT / 'examples/architecture.void'
compiler = ROOT / 'build/llvm23-release/voidc'
for mode in ['normal', 'O2']:
    flags = ['--no-opt'] if mode == 'normal' else []
    native = ROOT / 'build/llvm23-probes' / ('architecture-' + mode)
    commands = [([compiler, 'run', source, '7', '--no-cache', *flags], b'49\n'),
                ([compiler, 'build', source, '-o', native, *flags],
                 ('Built native executable: ' + str(native) + '\n').encode()),
                ([native, '7'], b'49\n')]
    for argv, expected in commands:
        argv = list(map(str, argv))
        r = crash.run(argv, capture_output=True, preexec_fn=stack, timeout=120,
                      expected_returncode=0, expected_stdout=expected,
                      source_inputs=[source], provenance_inputs=[Path(__file__)])
        records.append(dict(argv=argv, returncode=r.returncode,
                            stdout=r.stdout.decode(), stderr=r.stderr.decode(),
                            hard_stack_bytes=1048576))
        (OUT / 'focused-execution-corrected.json').write_text(json.dumps(records, indent=2) + '\n')
        assert not r.expectation_failed, records[-1]
print('6/6 hard-stack normal/O2 JIT, native build and independent native execution checks passed.')
