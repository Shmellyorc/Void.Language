"""Verify all new modules through a relocated compiler/stdlib installation."""
from pathlib import Path
import os
import sys
import tempfile
from crash import run

compiler = Path(sys.argv[1]).resolve()
build = Path(sys.argv[2]).resolve()
configuration = sys.argv[3]
fixture = Path(__file__).with_name('interop.void').resolve()
checks = 0


def invoke(arguments, cwd):
    global checks
    result = run(arguments, cwd=cwd, capture_output=True, timeout=600, source_inputs=[fixture])
    assert result.returncode == 0, (arguments, result.returncode, result.stdout, result.stderr)
    assert b'warning' not in result.stderr.lower(), result.stderr
    checks += 1


with tempfile.TemporaryDirectory(prefix='VOID text installed 雪 ') as directory:
    root = Path(directory)
    prefix = root / 'installed'
    invoke(['cmake', '--install', build, '--config', configuration, '--component', 'compiler',
            '--prefix', prefix], root)
    moved = root / 'relocated'
    prefix.rename(moved)
    installed = moved / 'bin' / compiler.name
    assert (moved / 'share/void/licenses/UNICODE-LICENSE.txt').is_file()
    for module in ['text.void', 'encoding/hex.void', 'encoding/base64.void', 'encoding/ascii.void']:
        assert (moved / 'share/void/stdlib' / module).read_bytes() == (
            Path(__file__).resolve().parents[2] / 'stdlib' / module).read_bytes()
    invoke([installed, 'check', fixture], root)
    invoke([installed, 'run', fixture, '--no-opt'], root)
    invoke([installed, 'run', fixture], root)
    output = root / ('installed.exe' if os.name == 'nt' else 'installed')
    invoke([installed, 'build', fixture, '-o', output], root)
    invoke([output], root)
print(f'{checks} installed/relocated check/JIT/O2/linked AOT operations passed')
