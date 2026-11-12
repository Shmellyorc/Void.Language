"""Exercise the public application and independently verify the resulting files."""
import os
from pathlib import Path
import re
import subprocess
import sys
from tempfile import TemporaryDirectory

compiler, resources = map(Path, sys.argv[1:])
fixtures = Path(__file__).resolve().parent
example = fixtures.parents[1] / 'examples' / 'atomic_save' / 'test_game.void'
version, clang, linker, *link_resources = resources.read_text(encoding='utf-8').splitlines()
assert version == 'VOID native 23.1.4 v2'
windows = os.name == 'nt'
link_flags = ['--driver-mode=g++']
if windows:
    sdk_linker = Path(clang).with_name('lld-link.exe').resolve()
    link_flags.extend(['-B' + str(sdk_linker.parent), '-fuse-ld=lld'])
elif linker:
    link_flags.append('--ld-path=' + linker)


def run(command, work):
    result = subprocess.run(list(map(str, command)), cwd=work, capture_output=True, timeout=60)
    assert result.returncode == 0 and not result.stderr, (command, result)
    return result.stdout


def link(command, work):
    if windows:
        # On MSVC targets the driver selects lld-link through -fuse-ld.
        # Check its actual plan before allowing the independent link.
        plan = subprocess.run([*map(str, command), '-###'], cwd=work,
                              capture_output=True, encoding='utf-8', timeout=60)
        launch = re.search(r'^\s*"([^"]*lld-link(?:\.exe)?)"', plan.stderr, re.M | re.I)
        assert plan.returncode == 0 and launch, plan.stderr
        launched = Path(launch[1].replace('\\\\', '\\')).with_suffix('.exe').resolve()
        assert launched == sdk_linker, plan.stderr
    run(command, work)


with TemporaryDirectory(prefix='VOID atomic save 雪 ') as directory:
    work = Path(directory).resolve()
    if windows:
        assert run([sdk_linker, '--version'], work).startswith(b'LLD 23.1.4')
    run([compiler, 'check', example], work)
    for mode, flags in [('normal', ['--no-opt']), ('optimized', [])]:
        obj = work / (mode + ('.obj' if windows else '.o'))
        image = obj.with_suffix('.exe' if windows else '.bin')
        run([compiler, 'emit', example, '-o', obj, *flags], work)
        link([clang, *link_flags, obj, *link_resources, '-o', image], work)
        for engine, executable in [('JIT', [compiler, 'run', example, *flags, '--no-cache', '--']), ('AOT', [image])]:
            target = work / f'{mode} {engine} save 雪'
            output = run([*executable, target], work)
            assert output.endswith(b'Collision refused and committed text and binary saves verified\n'), output
            assert (target / 'configuration.txt').read_bytes() == 'volume=85\nplayer=初雪\n'.encode()
            assert (target / 'checkpoint.bin').read_bytes() == bytes([128, 0, 0, 255])
            assert {file.name for file in target.iterdir()} == {'.void-save-lock', 'configuration.txt', 'checkpoint.bin'}
            if not windows:
                assert output == b'Collision refused and committed text and binary saves verified\n'
            print(mode, engine, output.decode().strip())
    limit = 16 * 1024 * 1024
    maximum = b'\xe9' * limit
    (work / 'maximum-input').write_bytes(maximum)
    (work / 'oversized-input').write_bytes(b'a' * (limit + 1))
    run([compiler, 'run', fixtures / 'atomic_save_limits.void', '--no-cache'], work)
    assert (work / 'limits' / 'maximum.bin').read_bytes() == maximum
    assert {file.name for file in (work / 'limits').iterdir()} == {'.void-save-lock', 'maximum.bin'}
    print('Public byte snapshot cap and pre-encoding text cap verified')
