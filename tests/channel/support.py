"""Use the established crash recorder and retain every engineering invocation."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from test_platform import native_text, native_manifest
EVIDENCE = ROOT / 'docs/channel/evidence/implementation'
EVIDENCE.mkdir(parents=True, exist_ok=True)
os.environ['VOID_CRASH_EVIDENCE'] = str(EVIDENCE / 'crashes')
sys.path.insert(0, str(ROOT / 'tests/text_encoding'))
import crash
crash.install()


def stack():
    import resource
    resource.setrlimit(resource.RLIMIT_STACK, (1048576, 1048576))


def link(compiler, obj, executable):
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
    return run([clang, obj, *resources, *flags, '-o', executable])


def run(args, cwd=None, status=0, expected=None, small=False, timeout=180, **kwargs):
    args = list(map(str, args))
    receipt = Path(tempfile.mkdtemp(prefix='run-', dir=EVIDENCE))
    for source in (ROOT / 'stdlib/channel.void', *sorted((ROOT / 'tests/channel').glob('*.*'))):
        if source.suffix in ('.void', '.cpp', '.py'):
            target = receipt / 'inputs' / source.relative_to(ROOT)
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(source.read_bytes())
    for index, arg in enumerate(args):
        source = Path(arg)
        if source.suffix == '.void' and source.is_file():
            (receipt / ('argument-' + str(index) + '.void')).write_bytes(source.read_bytes())
    configured = kwargs.get('env', os.environ).get('VOID_STDLIB_ROOT')
    if configured and (Path(configured) / 'channel.void').is_file():
        (receipt / 'configured-channel.void').write_bytes((Path(configured) / 'channel.void').read_bytes())
    started = time.perf_counter()
    result = crash.run(args, cwd=cwd, capture_output=True, timeout=timeout,
                       preexec_fn=stack if small and os.name == 'posix' else None,
                       source_inputs=tuple((ROOT / 'tests/channel').glob('*.void')),
                       provenance_inputs=tuple((ROOT / 'tests/channel').glob('*.py')), **kwargs)
    (receipt / 'stdout').write_bytes(result.stdout)
    (receipt / 'stderr').write_bytes(result.stderr)
    (receipt / 'receipt.json').write_text(json.dumps({
        'args': args, 'cwd': str(cwd or Path.cwd()), 'status': result.returncode,
        'seconds': time.perf_counter() - started, 'hard_stack_bytes': 1048576 if small else None,
        'configured_standard_library': configured,
        'executable_sha256': hashlib.sha256(Path(args[0]).read_bytes()).hexdigest() if Path(args[0]).is_file() else None,
        'object_hashes': {arg: hashlib.sha256(Path(arg).read_bytes()).hexdigest() for arg in args
                          if Path(arg).suffix in ('.o', '.a', '.obj', '.lib') and Path(arg).is_file()},
        'output_hashes': {args[i + 1]: hashlib.sha256(Path(args[i + 1]).read_bytes()).hexdigest()
                         for i, arg in enumerate(args[:-1]) if arg == '-o' and Path(args[i + 1]).is_file()},
    }, indent=2) + '\n', encoding='utf-8')
    assert result.returncode == status, (receipt, args, result.returncode, result.stderr.decode(errors='replace'))
    if expected is not None:
        assert result.stdout == expected, (receipt, result.stdout, expected)
    return result
