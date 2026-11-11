"""Install both real CMake components, relocate, and prove installed resolution."""
import os
from pathlib import Path
import platform
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from verify_install import HELLO, verify
from test_platform import relocate

cmake, build, configuration = sys.argv[1:]
environment = os.environ.copy()
environment.pop('VOID_STDLIB_ROOT', None)
package_host = (sys.platform.startswith('linux') and platform.machine().lower() in ('x86_64', 'amd64')
                and platform.libc_ver()[0] == 'glibc')


def run(argv, status=0, *, cwd=None):
    result = subprocess.run(list(map(str, argv)), capture_output=True, env=environment, timeout=180, cwd=cwd)
    assert result.returncode == status, (argv, result.returncode, result.stdout, result.stderr)
    return result


with tempfile.TemporaryDirectory(prefix='VOID CLI installation é ') as directory:
    root = Path(directory)
    initial = root / 'before move'
    for component in ('compiler', 'tooling'):
        run([cmake, '--install', build, '--prefix', initial, '--config', configuration, '--component', component])
    prefix = root / 'after move 雪'
    relocate(initial, prefix)
    expected = {path.relative_to(ROOT / 'stdlib'): path.read_bytes() for path in (ROOT / 'stdlib').rglob('*.void')}
    actual = {path.relative_to(prefix / 'share/void/stdlib'): path.read_bytes() for path in (prefix / 'share/void/stdlib').rglob('*.void')}
    assert expected == actual and expected
    verify(prefix)
    suffix = '.exe' if os.name == 'nt' else ''
    compiler = prefix / 'bin' / ('voidc' + suffix)
    project = root / 'external project'
    project.mkdir()
    run([compiler, 'init', project, '--project-id', 'org.test.installed', '--app-id', 'org.test.installed-app'], cwd=root)
    assert {path.name for path in project.iterdir()} == {'void.project.toml'}
    generated = root / 'created outside checkout'
    run([compiler, 'new', generated, '--source', 'nested/launcher.void'], cwd=root)
    run([compiler, 'run', '--project', generated, '--profile', 'release'], cwd=root)
    run([compiler, 'run', '--project', generated, '--profile', 'debug'], cwd=root)
    generated_output = root / ('installed-app' + suffix)
    run([compiler, 'build', '--project', generated, '-o', generated_output], cwd=root)
    if package_host:
        distribution = root / 'installed package 雪'
        run([compiler, 'package', '--project', generated, '-o', distribution], cwd=root)
        run([compiler, 'package', '--verify', distribution], cwd=root)
    generated.rename(root / 'hidden application sources')
    run([generated_output], cwd=root)
    if package_host:
        run([distribution / 'app'], cwd=root)
    source = project / 'main.void'
    source.write_text(HELLO, encoding='utf-8')
    output = project / ('hello' + suffix)
    run([compiler, 'build', source, '-o', output])
    if sys.platform.startswith('linux'):
        for binary in (compiler, output):
            loaded = run(['ldd', binary]).stdout.decode('utf-8')
            assert 'not found' not in loaded and '.toolchains/http' not in loaded, loaded
            for family in ('libidn2.so.', 'libunistring.so.'):
                rows = [line for line in loaded.splitlines() if line.strip().startswith(family)]
                assert len(rows) == 1, loaded
                resource = Path(rows[0].split(' => ', 1)[1].rsplit(' (', 1)[0]).resolve()
                assert resource.is_relative_to(prefix / 'lib/void'), loaded
    before = output.read_bytes()
    module = prefix / 'share/void/stdlib/console.void'
    hidden = module.with_suffix('.hidden')
    module.rename(hidden)
    try:
        failure = run([compiler, 'check', source], 1)
        assert b'VOID3002' in failure.stderr and b'console.void' in failure.stderr, failure.stderr
    finally:
        hidden.rename(module)
    # Build failure with a missing *installed* Runtime must retain the previous
    # executable and must not resolve the still-present build-tree Runtime.
    runtime = prefix / 'bin' / (prefix / 'bin/void-native.txt').read_text(encoding='utf-8').splitlines()[3]
    hidden = runtime.with_suffix('.hidden')
    runtime.rename(hidden)
    try:
        failure = run([compiler, 'build', source, '-o', output], 1)
        assert b'Reinstall the matching' in failure.stderr, failure.stderr
        assert output.read_bytes() == before
        run([compiler, '--version'])
    finally:
        hidden.rename(runtime)
    run([compiler, 'check', source])
    run([output])
    # Run native output without source/tooling lookup or a valid stdlib override.
    native_environment = environment | {'VOID_STDLIB_ROOT': str(root / 'absent sources')}
    result = subprocess.run([str(output)], cwd=root, env=native_environment, capture_output=True, timeout=30)
    assert result.returncode == 0 and result.stdout == b'Hello, world!\n'

print(f'PASS: {len(expected)} installed stdlib sources, relocated compiler/LSP, missing-module/Runtime controls, native output preservation')
