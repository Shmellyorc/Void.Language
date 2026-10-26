"""Actual source cross-device rename errors; a second native volume is required."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

compiler = Path(sys.argv[1]).resolve()
build = Path(sys.argv[2]).resolve()
secondary = os.environ.get('VOID_IO_SECOND_VOLUME', tempfile.gettempdir())

with tempfile.TemporaryDirectory(prefix='VOID io primary ', dir=build) as first:
    with tempfile.TemporaryDirectory(prefix='VOID io secondary ', dir=secondary) as second:
        root = Path(first)
        target = Path(second)
        if root.stat().st_dev == target.stat().st_dev:
            print('UNVERIFIED: no second filesystem; set VOID_IO_SECOND_VOLUME to a distinct volume')
            sys.exit(77)
        # Keep the path literal portable across VOID, POSIX and Windows.
        target_literal = str(target).replace('\\', '/').replace('"', '\\"')
        fixture = root / 'main.void'
        fixture.write_text('''use io
use path

func exercise() -> result<i32, io.error>:
    var made = io.create_directory("source directory")?
    var written = io.write_text("source file", "preserved")?
    var file_target = path.join(["TARGET", "file"])?
    var directory_target = path.join(["TARGET", "directory"])?
    var moved_file = io.move_file("source file", file_target)
    var moved_directory = io.move_directory("source directory", directory_target)

    if moved_file is Err(var reason):
        if not (reason.kind is io.error_kind.cross_device) or reason.os_code == 0:
            return Ok(1)
    else:
        return Ok(2)

    if moved_directory is Err(var reason):
        if not (reason.kind is io.error_kind.cross_device):
            return Ok(3)
    else:
        return Ok(4)

    var original = io.read_text("source file")?
    var directory_present = io.exists("source directory")?
    var file_destination = io.exists(file_target)?
    var directory_destination = io.exists(directory_target)?

    if original != "preserved" or not directory_present or file_destination or directory_destination:
        return Ok(5)

    var removed_file = io.remove_file("source file")?
    var removed_directory = io.remove_directory("source directory")?
    return Ok(0)

func main() -> i32:
    if exercise() is Ok(var answer):
        return answer

    return 99
'''.replace('TARGET', target_literal))
        for optimize in (False, True):
            arguments = [compiler, 'run', fixture]
            if not optimize:
                arguments += ['--no-opt']
            result = subprocess.run(list(map(str, arguments)), cwd=root, capture_output=True, timeout=180)
            assert result.returncode == 0, (result.stdout, result.stderr)
print('Cross-device file and directory renames preserved both source entries in actual JIT/O2')
