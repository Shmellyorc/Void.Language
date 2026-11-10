from pathlib import Path
from tempfile import TemporaryDirectory
import os
import shutil
import subprocess
import sys


def emit():
    harness, plan, source, output = map(Path, sys.argv[2:6])
    with TemporaryDirectory(prefix='void-g4-source-') as work:
        isolated_source = Path(work) / source.name
        shutil.copyfile(source, isolated_source)
        subprocess.run([harness, '--emit', plan, isolated_source, output, sys.argv[6]],
                       cwd=work, check=True, timeout=90)
        Path(str(output) + '.source.txt').write_text(work + '\n')
    assert not isolated_source.exists()


def qualify():
    exe, native, origin = map(Path, sys.argv[1:4])
    mode = sys.argv[4]
    competitor = Path(sys.argv[5])
    assert not Path(origin.read_text().strip()).exists()
    dynamic = subprocess.check_output(['readelf', '-d', exe], text=True)
    assert 'libvoid_g4_' not in dynamic and '(RPATH)' not in dynamic and '(RUNPATH)' not in dynamic
    assert not any(line.split()[-1:] == ['vd_c_primitive_dispatch_v3']
                   for line in subprocess.check_output(['nm', exe], text=True).splitlines())
    with TemporaryDirectory(prefix='void-g4-installed-') as work:
        root = Path(work)
        install = root/'relocated installation'
        cwd = root/'independent cwd'
        install.mkdir()
        cwd.mkdir()
        program = install/'app'
        shutil.copy2(exe, program)
        shutil.copytree(native, install/'native')
        shutil.copy2(competitor, cwd/competitor.name)

        def invoke(args, returncode, contains=None, env=None):
            process = subprocess.run([program, *args], cwd=cwd, text=True, capture_output=True,
                                     env=env, timeout=20)
            output = process.stdout + process.stderr
            print(f'{mode} {args} exit={process.returncode} {output.strip()}')
            assert process.returncode == returncode, (args, returncode, output)
            if contains is not None:
                assert contains.lower() in output.lower(), (args, contains, output)
            return output.lower()

        if mode == 'startup':
            invoke(['7'], 1, 'division by zero')
        else:
            assert invoke(['7'], 0).strip() == '104'
            invoke(['-1'], 1, 'closed, null or invalid')
            invoke(['-2'], 1, 'closed, null or invalid')
            invoke(['-3'], 1, 'resource operation rejected')
            assert invoke(['-4'], 0).strip() == '22'
            env = dict(os.environ, LD_LIBRARY_PATH=str(cwd))
            invoke(['7'], 1, 'ambient loader environment', env)

        dependency = install/'native'/competitor.name
        shutil.copy2(competitor, dependency)
        assert 'division by zero' not in invoke(['7'], 1, 'C dynamic digest')
        dependency.unlink()
        assert 'division by zero' not in invoke(['7'], 1, 'C dynamic artifact')
        shutil.copy2(native/dependency.name, dependency)
        library_dir = install/'native'
        library_dir.rename(install/'wrong-location')
        assert 'division by zero' not in invoke(['7'], 1, 'C dynamic path')
    print(f'{mode} verified relocated dynamic AOT with its compile source removed')


if sys.argv[1] == '--emit':
    emit()
else:
    qualify()
