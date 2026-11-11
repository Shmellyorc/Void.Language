"""D5 application directories through the existing CLI, AOT and publication path."""

from hashlib import sha256
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile

compiler = Path(sys.argv[1]).resolve()
root = Path(__file__).resolve().parents[2]
environment = dict(os.environ)
environment.pop('VOID_NATIVE_CATALOG', None)


def run(*command, status=0, message=None):
    process = subprocess.run(list(map(str, command)), cwd=cwd, env=environment, capture_output=True, timeout=120)
    text = (process.stdout + process.stderr).decode(errors='replace')
    assert process.returncode == status, (command, process.returncode, text)
    assert message is None or message in text, (command, text)
    assert 'AddressSanitizer' not in text and 'UndefinedBehaviorSanitizer' not in text, text
    return text


def cli(command, *arguments, **options):
    return run(compiler, command, '--project', project, *arguments, **options)


def snapshot(package):
    return {p.relative_to(package).as_posix(): sha256(p.read_bytes()).hexdigest()
            for p in package.rglob('*') if p.is_file()}


with tempfile.TemporaryDirectory(prefix='VOID D5 packages 雪 ') as temporary:
    base = Path(temporary).resolve()
    cwd = base / 'independent cwd'
    cwd.mkdir()
    run(compiler, 'package', '--help', message='voidc package --verify DIR')
    project = base / 'project'
    run(compiler, 'new', project, '--source', 'src/launcher.void')
    manifest = project / 'void.project.toml'
    initial = manifest.read_text()
    if not (sys.platform.startswith('linux') and platform.machine().lower() in ('x86_64', 'amd64')
            and platform.libc_ver()[0] == 'glibc'):
        cli('package', status=1, message='Application packaging requires Linux x86-64 GNU')
        print('D5 unsupported-platform diagnostic passed')
        sys.exit(0)

    source = project / 'src/launcher.void'
    source.write_text('use os\nuse io\n\nfunc main() -> i32:\n'
                      '    if os.resource_dir() is Ok(var directory):\n'
                      '        if io.read_text(directory + "/assets/message.txt") is Ok(var text):\n'
                      '            if text == "D5 resource":\n'
                      '                print("D5 packaged application")\n'
                      '                return 0\n'
                      '    return 23\n', encoding='utf-8')
    manifest.write_text(initial.replace('[application]\n', '[application]\nresources=["assets/message.txt"]\n'))
    assets = project / 'assets'
    assets.mkdir()
    resource = assets / 'message.txt'
    resource.write_text('D5 resource')
    shutil.copyfile(root / 'LICENSE', project / 'LICENSE')
    for declaration in ('"../outside"', '"assets/sub/file"', '"assets/source.void"',
                        '"assets/a", "assets/A"', ','.join(f'"assets/f{i}"' for i in range(33))):
        manifest.write_text(initial.replace('[application]\n', f'[application]\nresources=[{declaration}]\n'))
        cli('package', status=1, message='resource')
    manifest.write_text(initial.replace('[application]\n', '[application]\nresources=["assets/message.txt"]\n'))

    cli('package', '--profile', 'release')
    default = project / 'dist'
    run(compiler, 'package', '--verify', default)
    expected = {'app', 'application.meta', 'void-package.txt', 'assets/message.txt'}
    assert expected <= set(snapshot(default))
    assert (default / 'licenses/VOID-MIT-LICENSE.txt').read_bytes() == (root / 'LICENSE').read_bytes()
    assert (default / 'licenses/APPLICATION-LICENSE.txt').read_bytes() == (project / 'LICENSE').read_bytes()
    assert not any(p.suffix in ('.void', '.o', '.a') for p in default.rglob('*'))
    good = snapshot(default)
    cli('package', status=1, message='already exists')
    cli('package', '--replace')
    assert snapshot(default) == good
    resource.write_text('replacement resource')
    cli('package', '--replace')
    assert (default / 'assets/message.txt').read_text() == 'replacement resource'
    resource.write_text('D5 resource')
    cli('package', '--replace')
    assert snapshot(default) == good
    resource.unlink()
    resource.symlink_to(root / 'LICENSE')
    cli('package', '--replace', status=1, message='aliases')
    resource.unlink()
    resource.write_bytes(b'x' * (16 * 1024 * 1024 + 1))
    cli('package', '--replace', status=1, message='byte limit')
    resource.write_text('D5 resource')
    assert snapshot(default) == good

    copied_compiler = base / 'compiler copy'
    copied_compiler.mkdir()
    for name in ('voidc', 'void-native.txt', 'void-native-resources.txt', 'void-package-notices.txt'):
        shutil.copyfile(compiler.parent / name, copied_compiler / name)
    (copied_compiler / 'voidc').chmod(0o755)
    notices = copied_compiler / 'void-package-notices.txt'
    approved_notices = notices.read_text()
    first = approved_notices.splitlines()[1]
    fields = first.split('\t')
    notices.write_text(approved_notices.replace(first, '\t'.join([fields[0], fields[1], str(base / 'absent notice')])))
    run(copied_compiler / 'voidc', 'package', '--project', project, '-o', base / 'missing notice',
        status=1, message='project input')
    notices.write_text(approved_notices.replace(fields[0], '0' * 64, 1))
    run(copied_compiler / 'voidc', 'package', '--project', project, '-o', base / 'changed notice',
        status=1, message='notice checksum mismatch')
    notices.write_text(approved_notices)
    native_config = copied_compiler / 'void-native.txt'
    native_config.write_text(native_config.read_text().replace('VOID native 23.1.4 v2', 'VOID native 23.1.4 v1', 1))
    run(copied_compiler / 'voidc', 'package', '--project', project, '-o', base / 'unverified link inputs',
        status=1, message='verified v2 native resource inventory')

    missing = base / 'missing package'
    resource.unlink()
    cli('package', '-o', missing, status=1, message='project input')
    assert not missing.exists()
    cli('package', '--replace', status=1, message='project input')
    assert snapshot(default) == good
    resource.write_text('D5 resource')

    destination = base / 'output parent 雪' / 'distribution 雪 with spaces'
    cli('package', '-o', destination)
    assert snapshot(destination) == good
    moved = base / 'moved package 雪'
    destination.rename(moved)
    project.rename(base / 'parked project')
    bwrap = shutil.which('bwrap')
    assert bwrap, 'bwrap is required for independent execution qualification'
    isolated = [bwrap, '--ro-bind', '/', '/', '--dev-bind', '/dev', '/dev', '--proc', '/proc',
                '--tmpfs', root, '--tmpfs', compiler.parent, '--tmpfs', base / 'parked project',
                '--bind', moved, moved, '--chdir', cwd, '--']
    run(*isolated, '/usr/bin/test', '!', '-e', root / 'CMakeLists.txt')
    run(*isolated, '/usr/bin/test', '!', '-e', compiler)
    run(*isolated, moved / 'app', message='D5 packaged application')
    packaged_resource = moved / 'assets/message.txt'
    packaged_resource.write_text('changed')
    run(compiler, 'package', '--verify', moved, status=1, message='inventory mismatch')
    run(*isolated, moved / 'app', status=23)
    packaged_resource.unlink()
    run(compiler, 'package', '--verify', moved, status=1, message='Unexpected empty application package directory')
    packaged_resource.write_text('D5 resource')
    packaged_resource.chmod(0o644)
    run(compiler, 'package', '--verify', moved)
    (base / 'parked project').rename(project)

    unowned = base / 'unowned'
    unowned.mkdir()
    (unowned / 'keep.txt').write_text('preserve')
    cli('package', '-o', unowned, '--replace', status=1, message='project input')
    assert (unowned / 'keep.txt').read_text() == 'preserve'
    alias = base / 'alias'
    alias.symlink_to(moved, target_is_directory=True)
    cli('package', '-o', alias, '--replace', status=1)
    assert alias.is_symlink() and snapshot(moved) == good
    other = base / 'different app'
    run(compiler, 'new', other)
    run(compiler, 'package', '--project', other)
    other_package = other / 'dist'
    other_snapshot = snapshot(other_package)
    cli('package', '-o', other_package, '--replace', status=1, message='different application')
    assert snapshot(other_package) == other_snapshot

    shared = base / 'competing package'
    command = [str(compiler), 'package', '--project', str(project), '-o', str(shared)]
    processes = [subprocess.Popen(command, cwd=cwd, env=environment, stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE) for _ in range(2)]
    results = [process.communicate(timeout=120) for process in processes]
    assert sorted(process.returncode for process in processes) == [0, 1], results
    run(compiler, 'package', '--verify', shared)
    assert snapshot(shared) == good

    helper = project / 'helper'
    helper.mkdir()
    (helper / 'void.project.toml').write_text('schema=1\n[project]\nid="org.example.helper.project"\n'
        'name="Helper"\nversion="1.0.0"\nkind="library"\n[package]\nid="org.example.helper"\n'
        'name="helper"\nentry="api.void"\n')
    (helper / 'api.void').write_text('public func value() -> i32:\n    return 1\n')
    manifest.write_text(manifest.read_text()+'\n[dependencies.helper]\nid="org.example.helper"\n'
                        'path="helper"\nversion="=1.0.0"\n')
    source.write_text('use pkg/helper\n'+source.read_text().replace('    if os.resource_dir()',
                      '    if helper.value() != 1:\n        return 24\n    if os.resource_dir()', 1))
    cli('package', '-o', base / 'locked', status=1, message='Missing void.lock')
    cli('resolve')
    cli('package', '-o', base / 'locked')
    equivalent = base / 'equivalent project'
    shutil.copytree(project, equivalent, ignore=shutil.ignore_patterns('dist'))
    equivalent_output = base / 'equivalent package'
    run(compiler, 'package', '--project', equivalent, '-o', equivalent_output)
    reference = snapshot(base / 'locked')
    reproduced = snapshot(equivalent_output)
    assert reference.keys() == reproduced.keys()
    assert {name: digest for name, digest in reference.items() if name not in ('app', 'void-package.txt')} == {
        name: digest for name, digest in reproduced.items() if name not in ('app', 'void-package.txt')}
    assert (base / 'locked/void-package.txt').read_text().splitlines()[4] == \
           (equivalent_output / 'void-package.txt').read_text().splitlines()[4]
    lock = (project / 'void.lock').read_bytes()
    (helper / 'api.void').write_text('public func value() -> i32:\n    return 2\n')
    cli('package', '-o', base / 'locked', '--replace', status=1, message='Stale or tampered void.lock')
    assert (project / 'void.lock').read_bytes() == lock
    project.rename(base / 'parked project')
    run(*isolated, '/usr/bin/test', '!', '-e', project / 'void.project.toml')
    run(*isolated, '/usr/bin/test', '!', '-e', project / 'helper/api.void')
    run(*isolated, base / 'locked/app', message='D5 packaged application')
    assert not list(base.glob('acquire-*'))
    print('D5 resources, metadata, locked sources, repeat inventory, replacement and isolated relocation passed')
