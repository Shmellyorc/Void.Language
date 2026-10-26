"""Run the practical example through actual CLI modes and relocated stdlib installs."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

compiler = Path(sys.argv[1]).resolve()
build = Path(sys.argv[2]).resolve()
source = Path(sys.argv[3]).resolve()
configuration = sys.argv[4]
checks = 0


def run(arguments, cwd):
    global checks
    result = subprocess.run(list(map(str, arguments)), cwd=cwd, capture_output=True, timeout=180)
    assert result.returncode == 0, (arguments, result.stdout, result.stderr)
    assert b'warning' not in result.stderr.lower(), result.stderr
    checks += 1


with tempfile.TemporaryDirectory(prefix='VOID io examples install ') as directory:
    root = Path(directory)
    fixture = root / 'main.void'
    fixture.write_bytes(source.read_bytes())
    for mode in ('jit', 'optimized', 'aot', 'aot-optimized'):
        cwd = root / mode
        cwd.mkdir()
        if mode in ('jit', 'optimized'):
            arguments = [compiler, 'run', fixture]
            if mode == 'jit':
                arguments += ['--no-opt']
            run(arguments, cwd)
        else:
            executable = cwd / ('example.exe' if os.name == 'nt' else 'example')
            arguments = [compiler, 'build', fixture, '-o', executable]
            if mode == 'aot':
                arguments += ['--no-opt']
            run(arguments, cwd)
            run([executable], cwd)
        assert not (cwd / 'game data 雪').exists(), mode

    prefix = root / 'installed 雪'
    run(['cmake', '--install', build, '--config', configuration,
         '--component', 'compiler', '--prefix', prefix], root)
    relocated = root / 'relocated 雪'
    prefix.rename(relocated)
    installed = relocated / 'bin' / compiler.name
    for module in ('io.void', 'path.void', 'io/memory.void', 'encoding/binary.void'):
        assert (relocated / 'share/void/stdlib' / module).is_file(), module
    run([installed, 'check', fixture], root)
    run([installed, 'run', fixture], root)
    executable = root / ('installed-example.exe' if os.name == 'nt' else 'installed-example')
    run([installed, 'build', fixture, '-o', executable], root)
    run([executable], root)
print(f'{checks} example/install checks passed: Unicode config, game save/load, binary asset, '
      'memory/file wire interoperability, JIT/O2/linked AOT and relocated stdlib')
