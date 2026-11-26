"""Ordinary project/file access, shared modes, cache hits and existing Ninja."""
import argparse
import errno
import hashlib
import json
import os
from pathlib import Path
import subprocess
import struct
import sys
import tempfile
import uuid

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('compiler', type=Path)
parser.add_argument('--ninja', type=Path)
args = parser.parse_args()
compiler = args.compiler.resolve()
rows = []


def run(*arguments, cwd, status=0):
    result = subprocess.run([str(compiler), *map(str, arguments)], cwd=cwd,
                            capture_output=True, text=True, timeout=180)
    assert result.returncode == status, (arguments, result.returncode, result.stdout, result.stderr)
    assert not any(marker in result.stderr for marker in
                   ('AddressSanitizer', 'UndefinedBehaviorSanitizer', 'runtime error:'))
    rows.append({'directory': str(cwd), 'arguments': list(map(str, arguments)),
                 'status': result.returncode})
    return result


SAVING = '''use io
use path

func exercise() -> result<i32, io.error>:
    var location_result = path.absolute(".void/saves/.")
    if location_result is Err(var reason):
        return Ok(1)
    if location_result is Ok(var location):
        with io.open_directory(location)? as directory:
            var saved = directory.save_text_atomic("existing.txt", "updated")
            if not saved.published or saved.code != 0:
                return Ok(2)
            with directory.open_file("existing.txt")? as input_file:
                if input_file.length()? != 7:
                    return Ok(3)
            with directory.create_file("created.txt")? as output_file:
                var count = output_file.write_bytes("created".to_utf8())?
            with directory.create_directory("child")? as child:
                pass
    print("ordinary access passed")
    return Ok(0)

func main() -> i32:
    var outcome = exercise()
    if outcome is Ok(var status):
        return status
    if outcome is Err(var failure):
        print(failure.message)
    return 8
'''

with tempfile.TemporaryDirectory(prefix='VOID ordinary filesystem ') as temporary:
    base = Path(temporary)
    modes = (0o755, 0o775, 0o2775) if os.name != 'nt' else (None,)
    for mode in modes:
        project = base / (oct(mode) if mode else 'inherited-acl')
        project.mkdir()
        if mode is not None:
            project.chmod(mode)
        initial_mode = project.stat().st_mode
        source = project / 'main.void'
        source.write_text('func main():\n    print("ordinary project")\n')
        run('run', source, cwd=project)
        run('init', cwd=project)
        # Existing administrative storage follows the same ordinary OS policy.
        cache = project / '.void/cache/v1'
        cache.mkdir(parents=True, exist_ok=True)
        if mode is not None:
            for directory in (cache.parent.parent, cache.parent, cache):
                directory.chmod(mode)
        administrative_modes = {p: p.stat().st_mode for p in (cache.parent.parent, cache.parent, cache)}
        run('check', '.', cwd=project)
        for expected in ('miss', 'hit'):
            timing = base / f'{project.name}-{expected}.json'
            run('run', '.', '--timings', '--timings-format', 'json', '--timings-output', timing, cwd=project)
            actual = json.loads(timing.read_text())['cache']['status']
            if os.name != 'nt':
                assert actual == expected, (mode, expected, actual)
        run('build', '.', cwd=project)
        library = base / ('library-' + project.name)
        run('new', 'lib', library, '--package-name', 'helper', '--package-id',
            'org.example.permissionhelper', cwd=base)
        run('add', 'helper', '--dir', library, '--copy', cwd=project)
        run('tidy', '--check', cwd=project)
        run('remove', 'helper', cwd=project)
        package = base / ('package-' + project.name)
        if sys.platform == 'darwin':
            diagnostic = 'Application packaging requires Linux x86-64 GNU or Windows x64 MSVC.'
            assert diagnostic in run('package', '.', '-o', package, cwd=project, status=1).stderr
            assert diagnostic in run('package', '--verify', package, cwd=project, status=1).stderr
            assert not package.exists()
        else:
            run('package', '.', '-o', package, cwd=project)
            if mode is not None:
                for member in [package, *package.rglob('*')]:
                    member.chmod(0o775 if member.is_dir() or member.stat().st_mode & 0o111 else 0o664)
            run('package', '--verify', package, cwd=project)
        source.write_text(SAVING)
        saves = project / '.void/saves'
        saves.mkdir()
        if mode is not None:saves.chmod(mode)
        existing = saves / 'existing.txt'
        existing.write_text('original')
        if mode is not None:
            existing.chmod(0o664 if mode & 0o20 else 0o644)
        saved_acl = None
        if sys.platform.startswith('linux') and mode == 0o2775:
            # A named group grant must survive atomic replacement, not just mode bits.
            acl = struct.pack('<I', 2) + b''.join(struct.pack('<HHI', *entry) for entry in
                [(1, 6, 0xffffffff), (4, 4, 0xffffffff), (8, 6, os.getgid()),
                 (16, 6, 0xffffffff), (32, 4, 0xffffffff)])
            try:
                os.setxattr(existing, 'system.posix_acl_access', acl)
                saved_acl = os.getxattr(existing, 'system.posix_acl_access')
            except OSError as error:
                if error.errno not in (95,):
                    raise
        file_mode = existing.stat().st_mode
        run('run', '.', '--no-cache', cwd=project)
        assert existing.read_text() == 'updated'
        assert os.name == 'nt' or existing.stat().st_mode == file_mode
        if saved_acl is not None:
            assert os.getxattr(existing, 'system.posix_acl_access') == saved_acl
        if sys.platform.startswith('linux') and mode == 0o2775 and saved_acl is not None:
            # No ACL on the original: a newly inherited stage ACL must be
            # removed, even when chmod would leave the same mode bits.
            os.removexattr(existing, 'system.posix_acl_access')
            existing.chmod(0o640)
            default_acl = struct.pack('<I', 2) + b''.join(struct.pack('<HHI', *entry) for entry in
                [(1, 7, 0xffffffff), (2, 4, 65534), (4, 0, 0xffffffff),
                 (16, 4, 0xffffffff), (32, 0, 0xffffffff)])
            os.setxattr(saves, 'system.posix_acl_default', default_acl)
            try:
                (saves / 'created.txt').unlink()
                (saves / 'child').rmdir()
                run('run', '.', '--no-cache', cwd=project)
                assert existing.stat().st_mode & 0o777 == 0o640
                try:
                    unexpected = os.getxattr(existing, 'system.posix_acl_access')
                except OSError as error:
                    assert error.errno == errno.ENODATA, error
                else:
                    raise AssertionError(('atomic save inherited an unrelated ACL', unexpected))
            finally:
                os.removexattr(saves, 'system.posix_acl_default')
        assert (saves / 'created.txt').read_text() == 'created'
        assert project.stat().st_mode == initial_mode
        assert all(p.stat().st_mode == old for p, old in administrative_modes.items())
        # Ordinary access does not turn a link into file-writing authority.
        if os.name != 'nt':
            (saves / 'child').rmdir()
            (saves / 'created.txt').unlink()
            existing.unlink()
            sentinel = base / ('sentinel-' + project.name)
            sentinel.write_text('KEEP')
            existing.symlink_to(sentinel)
            run('run', '.', '--no-cache', cwd=project, status=2)
            assert sentinel.read_text() == 'KEEP' and existing.is_symlink()

if args.ninja:
    ninja = args.ninja.resolve(strict=True)
    protected = [ninja, ninja / 'void.project.toml', ninja / 'app.void', ninja / 'void.lock']
    before = {p: (p.stat().st_mode, hashlib.sha256(p.read_bytes()).hexdigest() if p.is_file() else None)
              for p in protected if p.exists()}
    # An active editor may contain incomplete user code. Exercise the real
    # selected project through a disposable explicit entry, without editing it.
    entry = ninja / ('permission_dx_' + uuid.uuid4().hex + '.void')
    smoke = ('use pkg/raylib as rl\nfunc main() -> i32:\n'
             '    var image = rl.gen_image_color(2, 2, rl.orange()) as rl.image\n'
             '    defer rl.unload_image(image)\n'
             '    if rl.get_image_color(image, 0, 0).r != 255:\n'
             '        return 9\n    return 0\n')
    entry.write_text(smoke)
    try:
        with tempfile.TemporaryDirectory(prefix='permission-dx-', dir=ninja / '.void') as output:
            selected = ['--project', '.', '--entry', entry.name]
            run('check', *selected, cwd=ninja)
            run('resolve', '--project', '.', '--locked', cwd=ninja)
            run('run', *selected, '--allow-native', cwd=ninja)
            run('build', *selected, '--allow-native', '-o', Path(output) / 'app', cwd=ninja)
            run('add', 'raylib', cwd=ninja)
    finally:
        if entry.read_text() != smoke:
            raise RuntimeError('Temporary Ninja entry changed externally; retained it for inspection')
        entry.unlink()
    for path, state in before.items():
        assert (path.stat().st_mode, hashlib.sha256(path.read_bytes()).hexdigest()
                if path.is_file() else None) == state, path

print(json.dumps({'status': 'PASS', 'checks': len(rows), 'calls': rows}, indent=2))
