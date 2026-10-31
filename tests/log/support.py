"""Logging receipts using the shared text/encoding crash recorder."""
from pathlib import Path
import hashlib
import json
import os
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from test_platform import native_text, native_manifest
EVIDENCE = ROOT / 'docs/log/evidence'
EVIDENCE.mkdir(parents=True, exist_ok=True)
os.environ['VOID_CRASH_EVIDENCE'] = str(EVIDENCE / 'crashes')
sys.path.insert(0, str(ROOT / 'tests/text_encoding'))
import crash


def stack():
    import resource
    resource.setrlimit(resource.RLIMIT_STACK, (1048576, 1048576))


def link(compiler, obj, executable, **kwargs):
    compiler = Path(compiler)
    manifest = native_manifest(compiler)
    clang, linker, *resources = manifest[1:5]
    resources = [str((compiler.parent / path).resolve()) if not Path(path).is_absolute() else path
                 for path in resources]
    flags = ['--driver-mode=g++', *manifest[5:]]
    if linker:
        flags += ['--ld-path=' + linker]
    if sys.platform.startswith('linux'):
        flags += ['-ldl']
    return run([clang, obj, *resources, *flags, '-o', executable], **kwargs)


def run(args, cwd=None, status=0, expected=None, small=False, timeout=600,
        sources=(), context=None, **kwargs):
    args = list(map(str, args))
    working = Path(cwd or ROOT)
    source_inputs = [ROOT / 'tests/log', ROOT / 'examples/log', ROOT / 'tests/growable_arrays',
                     ROOT / 'src/runtime', ROOT / 'include', ROOT / 'src/frontend/compiler.cpp',
                     ROOT / 'src/backend/llvm/backend.cpp', *sources]
    if working != ROOT:
        source_inputs += list(working.rglob('*.void'))
    provenance = [*Path(__file__).parent.glob('*.py'), ROOT / 'CMakeLists.txt']
    for directory in source_inputs:
        directory = Path(directory)
        if directory.is_dir():
            provenance += list(directory.rglob('*.cpp'))
            provenance += list(directory.rglob('*.txt'))
    metadata = {'profile': str(Path(args[0]).parent), 'mode': args[1] if len(args) > 1 else 'runner',
                'expected_status': status, 'expected_stdout_hex': expected.hex() if expected is not None else None,
                'hard_stack_bytes': 1048576 if small else None, **(context or {})}
    started = time.perf_counter()
    capture = kwargs.pop('capture_output', True)
    result = crash.run(args, cwd=working, capture_output=capture, timeout=timeout,
                       preexec_fn=stack if small and os.name == 'posix' else None,
                       expected_returncode=status, expected_stdout=expected,
                       source_inputs=source_inputs, provenance_inputs=provenance + [p for p in source_inputs if Path(p).is_dir()],
                       evidence_context=metadata, **kwargs)
    receipt = Path(tempfile.mkdtemp(prefix='run-', dir=EVIDENCE))
    (receipt / 'stdout.bin').write_bytes(result.stdout or b'')
    (receipt / 'stderr.bin').write_bytes(result.stderr or b'')
    inputs = [p for p in (ROOT / 'stdlib').rglob('*.void')]
    inputs += [p for p in Path(__file__).parent.glob('*') if p.is_file()]
    inputs += [Path(arg) for arg in args if Path(arg).suffix in ('.void', '.o', '.a') and Path(arg).is_file()]
    metadata.update(command=args, cwd=str(working), returncode=result.returncode,
                    seconds=time.perf_counter() - started,
                    source_hashes={str(p): crash.digest(p) for p in inputs},
                    executable_sha256=crash.digest(Path(args[0])) if Path(args[0]).is_file() else None)
    (receipt / 'receipt.json').write_text(json.dumps(metadata, indent=2) + '\n', encoding='utf-8')
    assert not result.expectation_failed, (receipt, args, result.returncode, result.stdout, result.stderr.decode(errors='replace'))
    return result
