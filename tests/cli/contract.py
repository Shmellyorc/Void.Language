"""Public command contract, real JIT/native results, paths and install diagnostics.

Runs on the native host in CTest; Windows/macOS coverage requires those hosts.
"""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile

compiler = Path(sys.argv[1]).resolve()
checks = 0
environment = os.environ.copy()
environment.pop('VOID_STDLIB_ROOT', None)


def run(args, status=0, out=None, error=None, *, cwd=None, tool=compiler, env=None):
    global checks
    result = subprocess.run([str(tool), *map(str, args)], cwd=cwd, env=environment if env is None else env,
                            capture_output=True, timeout=90)
    checks += 1
    assert result.returncode == status, (args, result.returncode, result.stdout, result.stderr)
    if out is not None:
        assert out in result.stdout.decode('utf-8'), (args, result.stdout)
    if error is not None:
        assert error in result.stderr.decode('utf-8'), (args, result.stderr)
    return result


for args in (['--help'], ['-h'], ['help']):
    result = run(args, out='Commands:')
    assert not result.stderr
    lines = result.stdout.decode('utf-8').splitlines()
    assert lines[:2] == ['VOID Language Compiler v0.1.0 (development)', 'LLVM 23.1.4'], lines
    assert lines.index('Commands:') < lines.index('Options:'), lines
    for option in ('--help', '--version', '--no-opt', '--cache', '--no-cache', '-o PATH'):
        assert option in result.stdout.decode('utf-8')
    for command in ('check', 'run', 'build', 'emit'):
        assert command in result.stdout.decode()
version = run(['--version'], out='LLVM 23.1.4')
assert b'VOID Language Compiler v0.1.0 (development)' in version.stdout and b'Target:' in version.stdout
for command in ('check', 'run', 'build', 'emit'):
    for args in ([command, '--help'], [command, '-h'], ['help', command]):
        result = run(args, out='Usage: voidc ' + command)
        assert b'Example' in result.stdout and not result.stderr
        assert result.stdout.decode('utf-8').splitlines()[:2] == ['VOID Language Compiler v0.1.0 (development)', 'LLVM 23.1.4']

invalid = [([], 'Expected a command'), (['unknown'], 'Unknown command'),
           (['--version', 'extra'], 'Unknown command'), (['help', 'unknown'], 'Expected help'),
           (['help', 'run', 'extra'], 'Expected help'), (['--app'], 'removed'),
           (['run', '--app'], 'removed'), (['run', '--help', '--app'], 'removed'),
           (['run', '--bogus'], 'Unknown option'), (['check'], 'Expected a file'),
           (['emit'], 'requires -o'), (['run', '-o'], 'output path'),
           (['build', '-o', '--no-opt'], 'output path'), (['build', '-o', ''], 'output path'),
           (['run', '--no-opt', '--no-opt'], 'Duplicate'),
           (['run', '--cache', '--cache'], 'Duplicate'),
           (['run', '--no-cache', '--no-cache'], 'Duplicate'),
           (['run', '--cache', '--no-cache'], 'cannot be combined'),
           (['emit', '-o', 'a.o', '-o', 'b.o'], 'output path'),
           (['check', 'a', 'b'], 'Too many operands'),
           (['run', 'a', 'b', 'c'], 'Too many operands'),
           (['run', 'a', '2147483648'], 'decimal i32'),
           (['run', 'a', '-2147483649'], 'decimal i32'),
           (['run', 'a', '1x'], 'decimal i32')]
for command in ('check', 'build', 'emit'):
    for option in ('--cache', '--no-cache'):
        invalid.append(([command, option], 'only'))
for command in ('check', 'run'):
    invalid.append(([command, '-o', 'a'], 'only for emit/build'))
for args, message in invalid:
    result = run(args, 2, error=message)
    assert not result.stdout and b'--help' in result.stderr and b'Usage:' in result.stderr

with tempfile.TemporaryDirectory(prefix='VOID CLI é # % ') as directory:
    root = Path(directory)
    project = root / 'hello world 雪'
    project.mkdir()
    source = project / 'main.void'
    source.write_text('use console\nfunc main() -> i32:\n    print("CLI hello")\n    return 7\n', encoding='utf-8')
    legacy = project / 'numeric.void'
    legacy.write_text('func entry(value: i32) -> i32:\n    return value\n', encoding='utf-8')
    bad = project / 'bad.void'
    bad.write_text('func main() -> i32:\n    return unknown\n', encoding='utf-8')
    run(['check', source], out='Source checked successfully')
    run(['check', project, '--no-opt'], out='Source checked successfully')
    run(['check', bad], 1)
    run(['check', project / 'absent.void'], 1)
    for args in (['run', source], ['run', project], ['run'], ['run', '.', '--no-cache'],
                 ['run', source, '--no-opt'], ['run', source, '--cache'],
                 ['run', source, '--no-opt', '--no-cache']):
        assert run(args, 7, out='CLI hello', cwd=project).stdout == b'CLI hello\n'
    run(['run', source, '1'], 2, error='do not accept arguments')
    for value in ('-2147483648', '-1', '0', '2147483647'):
        assert run(['run', legacy, value]).stdout == (value + os.linesep).encode()
    assert run(['run', legacy]).stdout == ('0' + os.linesep).encode()
    dash = project / '-source.void'
    dash.write_bytes(legacy.read_bytes())
    run(['run', '--', dash.name, '-9'], out='-9', cwd=project)
    run(['run', dash.name], 2, error='Unknown option', cwd=project)
    suffix = '.exe' if os.name == 'nt' else ''
    output = root / 'output folder é' / ('hello # %' + suffix)
    run(['build', '-o', output, source], out='Built native executable:')
    native = subprocess.run([str(output)], capture_output=True, timeout=30)
    assert native.returncode == 7 and native.stdout == b'CLI hello\n', native
    run(['build', '--no-opt'], cwd=project)
    assert (project / 'build' / ('main' + suffix)).is_file()
    obj = root / ('output folder é') / ('source.obj' if os.name == 'nt' else 'source.o')
    run(['emit', source, '-o', obj], out='Wrote native source object:')
    assert obj.stat().st_size > 0
    original = source.read_bytes()
    run(['build', source, '-o', source], 1, error='collision')
    run(['emit', source, '-o', source], 1, error='collision')
    assert source.read_bytes() == original
    run(['build', source, '-o', output.parent], 1, error='ordinary file')
    # Relocated compiler with deliberately absent resources: an actionable error,
    # no ambient/cwd manifest fallback, and no damage to an existing output.
    isolated = root / 'broken installation'
    isolated.mkdir()
    copied = isolated / compiler.name
    shutil.copy2(compiler, copied)
    # Metadata must work with only the executable: no stdlib/native manifests,
    # invalid source override, empty profile and no source/cache/output creation.
    profile = isolated / 'empty profile'
    profile.mkdir()
    metadata_env = environment | {'VOID_STDLIB_ROOT': 'invalid-relative-source-root',
                                  'HOME': str(profile), 'USERPROFILE': str(profile),
                                  'XDG_CACHE_HOME': str(profile)}
    original_files = {p.relative_to(isolated): p.read_bytes() for p in isolated.rglob('*') if p.is_file()}
    assert run(['--version'], tool=copied, cwd=isolated, env=metadata_env).stdout == version.stdout
    run(['--help'], tool=copied, cwd=isolated, env=metadata_env, out='Options:')
    assert {p.relative_to(isolated): p.read_bytes() for p in isolated.rglob('*') if p.is_file()} == original_files
    assert list(profile.iterdir()) == []
    before = output.read_bytes()
    run(['build', source, '-o', output], 1, error='Reinstall the matching', tool=copied)
    assert output.read_bytes() == before

print(f'{checks} CLI process checks passed on {sys.platform}; native output and collision preservation passed')
