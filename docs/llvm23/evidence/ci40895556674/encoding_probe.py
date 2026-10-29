"""Run the complete real combined fixture with a cp1252 default for VOID files.

PYTHONUTF8=0 python3 docs/llvm23/evidence/ci40895556674/encoding_probe.py build/llvm23-release
This controls Python file encoding on Linux; it does not emulate the Windows CRT.
"""
from pathlib import Path
from unittest.mock import patch
import ast
import json
import subprocess
import sys

root = Path(__file__).resolve().parents[4]
script = root / 'tests/combined/execution.py'
compiler = Path(sys.argv[1]).resolve() / 'voidc'
baseline = subprocess.check_output([
    'git', 'show', '75c31fe23a24144bf5d2f2796f4d404fa9897ce0:tests/combined/execution.py'
], cwd=root).decode('utf-8')
fixed = script.read_text(encoding='utf-8')
original_open, original_run = Path.open, subprocess.run
accesses, processes = [], []


def cp1252_open(path, mode='r', buffering=-1, encoding=None, errors=None, newline=None):
    if path.suffix == '.void' and 'b' not in mode:
        accesses.append({'file': path.name, 'mode': mode, 'encoding': encoding})
        if encoding in (None, 'locale'):
            encoding = 'cp1252'
    return original_open(path, mode, buffering, encoding, errors, newline)


def observed_run(*args, **kwargs):
    result = original_run(*args, **kwargs)
    processes.append({'command': list(map(str, args[0])), 'status': result.returncode})
    return result


def execute(source):
    namespace = {'__name__': '__main__', '__file__': str(script)}
    with patch.object(Path, 'open', cp1252_open), patch.object(subprocess, 'run', observed_run), \
            patch.object(sys, 'argv', [str(script), str(compiler)]):
        exec(compile(source, str(script), 'exec'), namespace)
    return namespace


try:
    execute(baseline)
except AssertionError as error:
    assert b'Invalid UTF-8 source' in str(error).encode('utf-8'), error
    baseline_failure = str(error)
else:
    raise AssertionError('baseline unexpectedly accepted cp1252 source')
baseline_processes = len(processes)
accesses.clear()
processes.clear()
namespace = execute(fixed)
assert len(accesses) == 11 and all(a['encoding'] == 'utf-8' for a in accesses), accesses
assert namespace['checks'] == len(processes) == 74, processes
assert len(namespace['scenarios']) == 5 and len(namespace['negatives']) == 6


def wire_expressions(source):
    return {node.targets[0].id: ast.dump(node.value) for node in ast.walk(ast.parse(source))
            if isinstance(node, ast.Assign) and isinstance(node.targets[0], ast.Name)
            and node.targets[0].id in ('console_input', 'console_output', 'expected', 'byte_text')}


assert wire_expressions(baseline) == wire_expressions(fixed)
import test_platform
with patch.object(test_platform.os, 'linesep', '\r\n'):
    assert test_platform.native_text('1\n') == b'1\r\n'
with patch.object(test_platform.os, 'linesep', '\n'):
    assert test_platform.native_text('1\n') == b'1\n'
print(json.dumps({'native_host': sys.platform, 'controlled_default': 'cp1252',
                  'baseline_failure': baseline_failure, 'baseline_processes': baseline_processes,
                  'fixed_processes': namespace['checks'], 'parsing_assertions': namespace['number'],
                  'fixed_VOID_writes': accesses, 'wire_and_binary_oracles_unchanged': True,
                  'CRT_text_expectations_checked': ['LF', 'CRLF'], 'passed': True}, indent=2))
