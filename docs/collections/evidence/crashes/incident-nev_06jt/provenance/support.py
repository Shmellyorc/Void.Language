"""Collections receipts using the shared text/encoding crash recorder."""
from pathlib import Path
import hashlib
import json
import os
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
EVIDENCE = ROOT / 'docs/collections/evidence'
EVIDENCE.mkdir(parents=True, exist_ok=True)
os.environ['VOID_CRASH_EVIDENCE'] = str(EVIDENCE / 'crashes')
sys.path.insert(0, str(ROOT / 'tests/text_encoding'))
import crash


def stack():
    import resource
    resource.setrlimit(resource.RLIMIT_STACK, (1048576, 1048576))


def run(args, cwd=None, status=0, expected=None, small=False, timeout=600,
        sources=(), context=None, **kwargs):
    args = list(map(str, args))
    working = Path(cwd or ROOT)
    source_inputs = [ROOT / 'tests/collections_stdlib', ROOT / 'examples/collections_stdlib', *sources]
    if working != ROOT:
        source_inputs += list(working.rglob('*.void'))
    provenance = [*Path(__file__).parent.glob('*.py'), ROOT / 'CMakeLists.txt']
    for directory in source_inputs:
        directory = Path(directory)
        if directory.is_dir():
            provenance += list(directory.rglob('*.cpp'))
    metadata = {'profile': str(Path(args[0]).parent), 'mode': args[1] if len(args) > 1 else 'runner',
                'expected_status': status, 'expected_stdout_hex': expected.hex() if expected is not None else None,
                'hard_stack_bytes': 1048576 if small else None, **(context or {})}
    started = time.perf_counter()
    result = crash.run(args, cwd=working, capture_output=True, timeout=timeout,
                       preexec_fn=stack if small and os.name == 'posix' else None,
                       expected_returncode=status, expected_stdout=expected,
                       source_inputs=source_inputs, provenance_inputs=provenance,
                       evidence_context=metadata, **kwargs)
    receipt = Path(tempfile.mkdtemp(prefix='run-', dir=EVIDENCE))
    (receipt / 'stdout.bin').write_bytes(result.stdout)
    (receipt / 'stderr.bin').write_bytes(result.stderr)
    inputs = [p for p in (ROOT / 'stdlib').rglob('*.void')]
    inputs += [p for p in Path(__file__).parent.glob('*') if p.is_file()]
    inputs += [Path(arg) for arg in args if Path(arg).suffix in ('.void', '.o', '.a') and Path(arg).is_file()]
    metadata.update(command=args, cwd=str(working), returncode=result.returncode,
                    seconds=time.perf_counter() - started,
                    source_hashes={str(p): crash.digest(p) for p in inputs},
                    executable_sha256=crash.digest(Path(args[0])) if Path(args[0]).is_file() else None)
    (receipt / 'receipt.json').write_text(json.dumps(metadata, indent=2) + '\n')
    assert not result.expectation_failed, (receipt, args, result.returncode, result.stdout, result.stderr.decode(errors='replace'))
    return result
