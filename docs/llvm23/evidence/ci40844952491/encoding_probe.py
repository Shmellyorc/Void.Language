"""Exercise the real I/O fixtures with a cp1252 default for VOID text files.

Run from the repository root with: PYTHONUTF8=0 python3 <this file> <build dir>
This emulates Python's file-encoding default, not native Windows execution.
"""
from pathlib import Path
from unittest.mock import patch
import json
import runpy
import sys
import tempfile

root = Path(__file__).resolve().parents[4]
build = Path(sys.argv[1]).resolve()
script = root / 'tests/io/execution.py'
namespace = runpy.run_path(str(script), run_name='io_encoding_probe')
expected_async = namespace['async_fixture']().encode('utf-8')
error_lines = (script.parent / 'errors.void').read_bytes().decode('utf-8').splitlines()
error_lines = error_lines[:error_lines.index('func entry(input: i32) -> i32:')]
expected_errors = namespace['async_fixture'](error_lines).encode('utf-8')
original_open = Path.open
accesses = []


def cp1252_open(path, mode='r', buffering=-1, encoding=None, errors=None, newline=None):
    if path.suffix == '.void' and 'b' not in mode:
        accesses.append({'file': str(path), 'mode': mode, 'encoding': encoding})
        if encoding in (None, 'locale'):
            encoding = 'cp1252'
    return original_open(path, mode, buffering, encoding, errors, newline)


def invoke(arguments):
    with patch.object(sys, 'argv', [str(script), *map(str, arguments)]):
        namespace['main']()


with tempfile.TemporaryDirectory(prefix='VOID cp1252 fixtures ') as directory:
    work = Path(directory)
    with patch.object(Path, 'open', cp1252_open):
        generated = work / 'io-wire.void'
        invoke(['--generate-async', generated])
        assert generated.read_bytes() == expected_async
        assert '雪'.encode('utf-8') in generated.read_bytes()
        generated_errors = work / 'io-errors-async.void'
        invoke(['--generate-errors-async', generated_errors])
        assert generated_errors.read_bytes() == expected_errors
        # Run the unchanged wire-byte oracles and all three negative fixtures.
        invoke([build / 'voidc'])
        invoke([build / 'voidc', build / 'void_io_runner', '--async'])

assert len(accesses) == 8, accesses  # seven writes and the errors.void read
assert all(access['encoding'] == 'utf-8' for access in accesses), accesses
print(json.dumps({'emulated_default': 'cp1252', 'native_host': sys.platform,
                  'VOID_text_accesses': accesses, 'passed': True}, indent=2))
