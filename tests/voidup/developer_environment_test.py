"""Actual manager environment probe under physical and aliased temp parents.

POSIX symlink cases model macOS temporary-path aliases on Linux; a Linux pass
does not establish native macOS qualification. No installed tools are modified.
"""
import argparse
import errno
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('compiler', type=Path)
a = p.parse_args()
compiler = a.compiler.resolve(strict=True)

with tempfile.TemporaryDirectory(prefix='voidup-environment-regression-') as directory:
    work = Path(directory).resolve(strict=True)
    physical = work / 'physical temp 雪'
    physical.mkdir()
    parents = [('physical', physical)]
    if os.name != 'nt':
        alias = work / 'alias temp 雪'
        alias.symlink_to(physical, target_is_directory=True)
        assert alias.is_symlink() and alias.samefile(physical)
        parents.append(('symlink ancestor', alias))
    for label, parent in parents:
        result = subprocess.run(
            [sys.executable, '-B', str(ROOT / 'tests/voidup/developer_environment.py'), str(compiler)],
            env=dict(os.environ, TMPDIR=str(parent), TEMP=str(parent), TMP=str(parent)),
            capture_output=True, text=True, timeout=180)
        assert result.returncode == 0, (label, result.returncode, result.stdout, result.stderr)
        assert result.stdout.startswith('PASS: CI, XDG and desktop session context retained;'), result.stdout
        assert result.stderr == '', result.stderr
        assert list(physical.iterdir()) == [], 'Qualification fixture did not clean up'
        print('PASS: actual developer environment qualification with ' + label)

    if os.name != 'nt':
        ordinary = work / 'ordinary-file'
        ordinary.write_text('not a directory')
        cases = [('physical', physical, 0), ('raw alias', alias, errno.ENOTDIR),
                 ('missing', work / 'absent', errno.ENOENT), ('wrong type', ordinary, errno.ENOTDIR)]
        for index, (label, path, code) in enumerate(cases):
            source = work / ('admission-' + str(index) + '.void')
            source.write_text('''use io
func main() -> i32:
    var opened = io.open_directory(PROBE_DIRECTORY)
    if opened is Ok(var directory):
        with directory as held:
            print("OPEN")
            return 0
    else if opened is Err(var problem):
        print("{problem.operation}|{problem.os_code}")
    return 1
'''.replace('PROBE_DIRECTORY', json.dumps(str(path), ensure_ascii=False)))
            result = subprocess.run([str(compiler), 'run', '--no-cache', str(source)],
                                    capture_output=True, text=True, timeout=120)
            assert result.returncode == (1 if code else 0), (label, result.returncode, result.stdout, result.stderr)
            assert result.stdout == (f'io.open_directory|{code}\n' if code else 'OPEN\n'), (label, result.stdout)
            assert result.stderr == '', result.stderr
            print('PASS: unchanged Runtime directory admission for ' + label)

print('PASS: developer environment path regression; native platform = ' + sys.platform)
