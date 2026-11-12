"""D5 application directories through the existing CLI, AOT and publication path."""

from hashlib import sha256
import os
from pathlib import Path
import platform
import shutil
import struct
import subprocess
import sys
import tempfile
import tomllib

compiler = Path(sys.argv[1]).resolve()
root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / 'tools'))
from test_platform import relocate, windows_isolated_application, windows_package_imports

windows = sys.platform == 'win32'
executable = 'app.exe' if windows else 'app'
replacement_error = 'Windows application packaging cannot replace an existing destination'
readobj = Path(sys.argv[2]).resolve().with_name('llvm-readobj.exe')
assert not windows or platform.machine().lower() in ('x86_64', 'amd64'), platform.machine()
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


def contents_only(files):
    return {name: digest for name, digest in files.items() if name not in (executable, 'void-package.txt')}


def selected_inputs(package):
    return (package / 'void-package.txt').read_text().splitlines()[4]


def exact_inventory(package):
    lines = (package / 'void-package.txt').read_text().splitlines()
    assert lines[0] == 'VOID application package 1'
    assert lines[1] == 'target ' + ('x86_64-pc-windows-msvc' if windows else 'x86_64-unknown-linux-gnu')
    assert lines[2].startswith('profile project-profile-v2:')
    files = snapshot(package)
    del files['void-package.txt']
    assert lines[5:] == [f'{digest}\t{(package / name).stat().st_size}\t{name}'
                         for name, digest in sorted(files.items())]


with tempfile.TemporaryDirectory(prefix='VOID D5 packages 雪 % ') as temporary:
    base = Path(temporary).resolve()
    cwd = base / 'independent cwd'
    cwd.mkdir()
    run(compiler, 'package', '--help', message='voidc package --verify DIR')
    project = base / 'project'
    run(compiler, 'new', project, '--source', 'src/launcher.void')
    manifest = project / 'void.project.toml'
    initial = manifest.read_text()
    if not (platform.machine().lower() in ('x86_64', 'amd64') and
            (windows or (sys.platform.startswith('linux') and platform.libc_ver()[0] == 'glibc'))):
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
    notice_rows = (compiler.parent / 'void-package-notices.txt').read_text(encoding='utf-8').splitlines()[1:]
    required = {f'licenses/{name}': digest for digest, name, _ in (row.split('\t') for row in notice_rows)}
    expected = {executable, 'application.meta', 'void-package.txt', 'assets/message.txt',
                'licenses/APPLICATION-LICENSE.txt', *required}
    good = snapshot(default)
    assert expected == good.keys()
    assert all(good[name] == digest for name, digest in required.items())
    assert ('licenses/AWS-LC-NOTICE.txt' in required) != windows
    exact_inventory(default)
    receipt = default / 'void-package.txt'
    current = receipt.read_bytes()
    profile_tag = b'\nprofile project-profile-v2:'
    assert current.count(profile_tag) == 1
    try:
        # Inventory revision 1 also exists with a profile key from before
        # native source debugging changed the effective-profile key version.
        receipt.write_bytes(current.replace(profile_tag, b'\nprofile project-profile-v1:'))
        run(compiler, 'package', '--verify', default)
        receipt.write_bytes(current.replace(profile_tag, b'\nprofile project-profile-v3:'))
        run(compiler, 'package', '--verify', default, status=1, message='Invalid application package inventory header.')
    finally:
        receipt.write_bytes(current)
    manifest_data = tomllib.loads(manifest.read_text(encoding='utf-8'))
    project_info, application = manifest_data['project'], manifest_data['application']
    fields = (project_info['id'], project_info['name'], project_info['version'],
              application['id'], project_info['name'], project_info['version'])
    expected_metadata = struct.pack('<I', 1)
    for field in fields:
        data = field.encode()
        expected_metadata += struct.pack('<I', len(data)) + data
    assert (default / 'application.meta').read_bytes() == expected_metadata
    assert (default / 'licenses/VOID-MIT-LICENSE.txt').read_bytes() == (root / 'LICENSE').read_bytes()
    assert (default / 'licenses/APPLICATION-LICENSE.txt').read_bytes() == (project / 'LICENSE').read_bytes()
    assert not any(p.suffix in ('.void', '.o', '.a') for p in default.rglob('*'))
    cli('package', status=1, message='already exists')
    resource.write_text('replacement resource')
    if windows:
        cli('package', '--replace', status=1, message=replacement_error)
        assert snapshot(default) == good
        fresh_replace = base / 'fresh with replace'
        cli('package', '--replace', '-o', fresh_replace)
        run(compiler, 'package', '--verify', fresh_replace)
        assert (fresh_replace / 'assets/message.txt').read_text() == 'replacement resource'
        windows_package_imports(readobj, default / executable)
    else:
        cli('package', '--replace')
        assert (default / 'assets/message.txt').read_text() == 'replacement resource'
        resource.write_text('D5 resource')
        cli('package', '--replace')
        assert snapshot(default) == good
    resource.unlink()
    if windows:
        other_file = base / 'external resource'
        shutil.copyfile(root / 'LICENSE', other_file)
        os.link(other_file, resource)
    else:
        resource.symlink_to(root / 'LICENSE')
    bad_output = base / 'invalid resource'
    cli('package', '-o', bad_output, status=1, message='aliases')
    resource.unlink()
    resource.write_bytes(b'x' * (16 * 1024 * 1024 + 1))
    cli('package', '-o', bad_output, status=1, message='byte limit')
    assert not bad_output.exists()
    resource.write_text('D5 resource')
    assert snapshot(default) == good

    copied_compiler = base / 'compiler copy'
    copied_compiler.mkdir()
    for name in (compiler.name, 'void-native.txt', 'void-native-resources.txt', 'void-package-notices.txt'):
        shutil.copyfile(compiler.parent / name, copied_compiler / name)
    copied = copied_compiler / compiler.name
    if not windows:
        copied.chmod(0o755)
    notices = copied_compiler / 'void-package-notices.txt'
    approved_notices = notices.read_text(encoding='utf-8')
    first = approved_notices.splitlines()[1]
    fields = first.split('\t')
    notices.write_text(approved_notices.replace(first, '\t'.join([fields[0], fields[1], str(base / 'absent notice')])),
                       encoding='utf-8')
    run(copied, 'package', '--project', project, '-o', base / 'missing notice',
        status=1, message='project input')
    notices.write_text(approved_notices.replace(fields[0], '0' * 64, 1), encoding='utf-8')
    run(copied, 'package', '--project', project, '-o', base / 'changed notice',
        status=1, message='notice checksum mismatch')
    if windows:
        notices.write_text(''.join(approved_notices.splitlines(keepends=True)[:-1]), encoding='utf-8')
        run(copied, 'package', '--project', project, '-o', base / 'omitted required notice',
            status=1, message='Missing required Windows application package notices')
    notices.write_text(approved_notices, encoding='utf-8')
    native_config = copied_compiler / 'void-native.txt'
    native_config.write_text(native_config.read_text().replace('VOID native 23.1.4 v2', 'VOID native 23.1.4 v1', 1))
    run(copied, 'package', '--project', project, '-o', base / 'unverified link inputs',
        status=1, message='verified v2 native resource inventory')

    missing = base / 'missing package'
    resource.unlink()
    cli('package', '-o', missing, status=1, message='project input')
    assert not missing.exists()
    cli('package', '--replace', status=1, message=replacement_error if windows else 'project input')
    assert snapshot(default) == good
    resource.write_text('D5 resource')

    output_parent = base / 'output parent 雪'
    if windows:
        for index in range(6):
            output_parent /= f'level {index} ' + 'x' * 31
    destination = output_parent / 'distribution 雪 with spaces'
    cli('package', '-o', destination)
    if windows:
        assert len(str(destination / executable)) > 260
        run(compiler, 'package', '--verify', destination)
    assert contents_only(snapshot(destination)) == contents_only(good)
    assert selected_inputs(destination) == selected_inputs(default)
    if not windows:
        assert snapshot(destination) == good
    moved = base / 'moved package 雪'
    relocate(destination, moved)
    moved_snapshot = snapshot(moved)
    parked = base / 'parked project'
    relocate(project, parked)
    if not windows:
        bwrap = shutil.which('bwrap')
        assert bwrap, 'bwrap is required for independent execution qualification'
        isolated = [bwrap, '--ro-bind', '/', '/', '--dev-bind', '/dev', '/dev', '--proc', '/proc',
                    '--tmpfs', root, '--tmpfs', compiler.parent, '--tmpfs', parked,
                    '--bind', moved, moved, '--chdir', cwd, '--']
        run(*isolated, '/usr/bin/test', '!', '-e', root / 'CMakeLists.txt')
        run(*isolated, '/usr/bin/test', '!', '-e', compiler)

    def execute(image, status=0):
        if windows:
            result = windows_isolated_application(image, [], blocked=[(parked, 'src/launcher.void')],
                                                  cwd=cwd, env=environment, timeout=90)
            assert result.returncode == status, (result.stdout, result.stderr)
            if status == 0:
                assert result.stdout.splitlines() == [b'D5 packaged application'], result.stdout
        else:
            run(*isolated, image, status=status, message='D5 packaged application' if status == 0 else None)

    execute(moved / executable)
    packaged_resource = moved / 'assets/message.txt'
    packaged_resource.write_text('changed')
    run(compiler, 'package', '--verify', moved, status=1, message='inventory mismatch')
    execute(moved / executable, status=23)
    packaged_resource.unlink()
    run(compiler, 'package', '--verify', moved, status=1, message='Unexpected empty application package directory')
    packaged_resource.write_text('D5 resource')
    if not windows:
        packaged_resource.chmod(0o644)
    run(compiler, 'package', '--verify', moved)
    relocate(parked, project)

    for name in ('application.meta', 'licenses/VOID-MIT-LICENSE.txt', executable):
        path = moved / name
        backup = base / 'original file'
        relocate(path, backup)
        run(compiler, 'package', '--verify', moved, status=1)
        relocate(backup, path)
        if name != executable:
            original_bytes = path.read_bytes()
            path.write_bytes(original_bytes + b'changed')
            run(compiler, 'package', '--verify', moved, status=1, message='inventory mismatch')
            path.write_bytes(original_bytes)
    extra = moved / 'assets/extra'
    extra.write_bytes(b'extra')
    if not windows:
        extra.chmod(0o644)
    run(compiler, 'package', '--verify', moved, status=1, message='inventory mismatch')
    extra.unlink()
    if windows:
        receipt = moved / 'void-package.txt'
        original = receipt.read_bytes()
        image = moved / executable
        image_bytes = image.read_bytes()
        pe_offset = struct.unpack_from('<I', image_bytes, 0x3c)[0]
        wrong_machine = bytearray(image_bytes)
        struct.pack_into('<H', wrong_machine, pe_offset + 4, 0xAA64)
        masquerading_dll = bytearray(image_bytes)
        flags = struct.unpack_from('<H', image_bytes, pe_offset + 22)[0]
        struct.pack_into('<H', masquerading_dll, pe_offset + 22, flags | 0x2000)
        bad_entry = bytearray(image_bytes)
        struct.pack_into('<I', bad_entry, pe_offset + 24 + 16, 0)
        for changed in (wrong_machine, masquerading_dll, bad_entry, image_bytes[:pe_offset + 8]):
            image.write_bytes(changed)
            rows = original.splitlines(keepends=True)
            for index, row in enumerate(rows):
                if row.endswith(b'\tapp.exe\n'):
                    rows[index] = f'{sha256(changed).hexdigest()}\t{len(changed)}\tapp.exe\n'.encode()
                    break
            receipt.write_bytes(b''.join(rows))
            run(compiler, 'package', '--verify', moved, status=1, message='executable')
        image.write_bytes(image_bytes)
        receipt.write_bytes(original)
        renamed_receipt = moved / 'renamed inventory'
        relocate(receipt, renamed_receipt)
        wrong_case = moved / 'VOID-PACKAGE.TXT'
        relocate(renamed_receipt, wrong_case)
        run(compiler, 'package', '--verify', moved, status=1)
        relocate(wrong_case, renamed_receipt)
        relocate(renamed_receipt, receipt)
        hard_link = base / 'image link'
        os.link(image, hard_link)
        run(compiler, 'package', '--verify', moved, status=1, message='without aliases')
        hard_link.unlink()
        run(compiler, 'package', '--verify', moved)
        for leaf in ('NUL.txt', 'COM¹', 'stream:name', 'trailing.'):
            cli('package', '-o', base / leaf, status=1, message='portable directory')

    unowned = base / 'unowned'
    unowned.mkdir()
    (unowned / 'keep.txt').write_text('preserve')
    cli('package', '-o', unowned, '--replace', status=1, message=replacement_error if windows else 'project input')
    assert (unowned / 'keep.txt').read_text() == 'preserve'
    alias = base / 'alias'
    if windows:
        subprocess.run(['cmd.exe', '/d', '/c', 'mklink', '/J', alias.name, moved.name],
                       cwd=base, check=True, capture_output=True)
        run(compiler, 'package', '--verify', alias, status=1, message='reparse point')
        physical = base / 'Éxisting output'
        physical.mkdir()
        (physical / 'preserve').write_bytes(b'original')
        cli('package', '-o', base / 'éxisting output', status=1, message='already exists')
        assert (physical / 'preserve').read_bytes() == b'original'
    else:
        alias.symlink_to(moved, target_is_directory=True)
    cli('package', '-o', alias, '--replace', status=1)
    assert alias.exists() and snapshot(moved) == moved_snapshot
    other = base / 'different app'
    run(compiler, 'new', other)
    run(compiler, 'package', '--project', other)
    other_package = other / 'dist'
    other_snapshot = snapshot(other_package)
    cli('package', '-o', other_package, '--replace', status=1,
        message=replacement_error if windows else 'different application')
    assert snapshot(other_package) == other_snapshot

    shared = base / 'competing package'
    command = [str(compiler), 'package', '--project', str(project), '-o', str(shared)]
    processes = [subprocess.Popen(command, cwd=cwd, env=environment, stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE) for _ in range(2)]
    results = [process.communicate(timeout=120) for process in processes]
    assert sorted(process.returncode for process in processes) == [0, 1], results
    run(compiler, 'package', '--verify', shared)
    assert contents_only(snapshot(shared)) == contents_only(good)
    assert selected_inputs(shared) == selected_inputs(default)

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
    assert contents_only(reference) == contents_only(reproduced)
    assert selected_inputs(base / 'locked') == selected_inputs(equivalent_output)
    lock = (project / 'void.lock').read_bytes()
    (helper / 'api.void').write_text('public func value() -> i32:\n    return 2\n')
    cli('package', '-o', base / 'locked', '--replace', status=1, message='Stale or tampered void.lock')
    assert (project / 'void.lock').read_bytes() == lock
    relocate(project, parked)
    if not windows:
        run(*isolated, '/usr/bin/test', '!', '-e', project / 'void.project.toml')
        run(*isolated, '/usr/bin/test', '!', '-e', project / 'helper/api.void')
    execute(base / 'locked' / executable)
    assert not list(base.rglob('acquire-*'))
    print('D5 resources, metadata, locked sources, repeat inventory, replacement and isolated relocation passed')
