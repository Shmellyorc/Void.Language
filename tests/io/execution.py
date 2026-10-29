"""Actual VOID public APIs with independent wire/file oracles in JIT/O2/native modes."""
from pathlib import Path
import os
import struct
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'text_encoding'))
from crash import run as run_preserving

compiler = None
small = '--small-stack' in sys.argv
checks = 0


def stack():
    import resource
    resource.setrlimit(resource.RLIMIT_STACK, (1024 * 1024, 1024 * 1024))


def run(arguments, cwd, expected=b'', success=True, source_inputs=(), provenance_inputs=()):
    global checks
    result = run_preserving(
        list(map(str, arguments)),
        cwd=cwd,
        capture_output=True,
        timeout=180,
        preexec_fn=stack if small else None,
        source_inputs=source_inputs,
        provenance_inputs=(Path(__file__).resolve(), *provenance_inputs),
    )
    expected_status = 0 if success else 1
    assert result.returncode == expected_status, (arguments, result.returncode, result.stdout, result.stderr)
    assert b'warning' not in result.stderr.lower(), result.stderr
    if expected is not None:
        assert result.stdout == expected, (arguments, result.stdout, expected, result.stderr)
    checks += 1
    return result


def vector_literal(value):
    return '[' + ', '.join(map(str, value)) + ']'


vectors = []
for signed in (True, False):
    for width in (8, 16, 32, 64):
        target = ('i' if signed else 'u') + str(width)
        low = -(1 << (width - 1)) if signed else 0
        high = (1 << (width - int(signed))) - 1
        for value in sorted({low, high, 0, 1}):
            vectors.append((target, f'{value} as {target}', value.to_bytes(width // 8, 'little', signed=signed)))
for target, width, patterns in (
    ('f32', 4, (0, 0x80000000, 1, 0x7f800000, 0xff800000, 0x7fc12345, 0x7f812345)),
    ('f64', 8, (0, 0x8000000000000000, 1, 0x7ff0000000000000, 0xfff0000000000000,
                0x7ff8123456789abc, 0x7ff0123456789abc)),
):
    for pattern in patterns:
        vectors.append((target, None, pattern.to_bytes(width, 'little')))

source = ['use io', 'use path', 'use encoding/binary', 'use io/memory', '',
          'func same(const left: [u8], const right: [u8]) -> bool:',
          '    if left.count != right.count:', '        return false',
          '    var index: i64 = 0', '    for byte in left:',
          '        if byte != right[index]:', '            return false',
          '        index += 1', '    return true', '',
          'func wire(order: binary.byte_order, name: string) -> result<i32, io.error>:',
          '    with io.open(name, io.open_mode.create, order)? as handle:']
for index, (target, expression, wire) in enumerate(vectors):
    if expression is None:
        source += [f'        var bits{index} = binary.decode<{target}>({vector_literal(wire)})',
                   f'        if bits{index} is Ok(var number{index}):',
                   f'            var written{index} = handle.write(number{index})?']
    else:
        source += [f'        var written{index} = handle.write({expression})?']
source += ['        var framed = handle.write_pascal_string("雪\\0x")?',
           '        var raw = handle.write_string("raw")?',
           '        var beginning = handle.seek(0)?']
for index, (target, expression, wire) in enumerate(vectors):
    source += [f'        var restored{index} = handle.read<{target}>()?',
               f'        var oracle{index} = binary.encode(restored{index})',
               f'        if not same(oracle{index}, {vector_literal(wire)}):',
               f'            return Ok({index + 1})']
source += ['        var text = handle.read_pascal_string()?', '        var tail = handle.read_string(3)?',
           '        var empty = handle.read_bytes(0)?',
           '        if empty.count != 0:', '            return Ok(73)',
           '        if text != "雪\\0x" or tail != "raw":', '            return Ok(70)',
           '        var exhausted = handle.read_exact(1)',
           '        if exhausted is Err(var reason):',
           '            if not (reason.kind is io.error_kind.unexpected_eof):', '                return Ok(71)',
           '        else:', '            return Ok(72)',
           '    return Ok(7)', '',
           'func exercise() -> result<i32, io.error>:',
           '    var little = wire(binary.byte_order.little, "little.bin")?',
           '    var big = wire(binary.byte_order.big, "big.bin")?',
           '    if little != 7 or big != 7:', '        return Ok(80)',
           '    var made = io.create_directories("tree/nested")?',
           '    var wrote = io.write_text("tree/a.txt", "first\\0雪")?',
           '    var appended = io.append_text("tree/a.txt", " tail")?',
           '    var text = io.read_text("tree/a.txt")?',
           '    if text != "first\\0雪 tail":', '        return Ok(81)',
           '    var nested = io.write_bytes("tree/nested/b.bin", [1, 2, 3])?',
           '    var added = io.append_bytes("tree/nested/b.bin", [4, 5])?',
           '    var appended_bytes = io.read_bytes("tree/nested/b.bin")?',
           '    if appended_bytes.count != 5 or appended_bytes[4] != 5:', '        return Ok(89)',
           '    var bad = io.write_bytes("bad.txt", [192, 175])?',
           '    var invalid = io.read_text("bad.txt")',
           '    if invalid is Err(var malformed):',
           '        if not (malformed.kind is io.error_kind.invalid_utf8) or malformed.partial.count != 2:',
           '            return Ok(82)', '    else:', '        return Ok(83)',
           '    var files = io.get_files("tree", "*.bin", true)?',
           '    var directories = io.get_directories("tree", "nest*", true)?',
           '    var entries = io.entries("tree", "*", true)?',
           '    if files.count != 1 or directories.count != 1 or entries.count != 3:', '        return Ok(84)',
           '    var copied = io.copy_directory("tree", "copy")?',
           '    var moved = io.move_directory("copy", "moved")?',
           '    var copied_file = io.copy_file("tree/a.txt", "one.txt")?',
           '    var moved_file = io.move_file("one.txt", "two.txt")?',
           '    var details = io.metadata("two.txt")?',
           '    if details.size != 15:', '        return Ok(85)',
           '    var deleted = io.remove_file("two.txt")?',
           '    var link_target = path.absolute("tree/a.txt")?',
           '    var link_created = io.create_symlink(link_target, "link.txt")',
           '    if link_created is Ok(var link_count):',
           '        var link_value = io.read_link("link.txt")?',
           '        var link_metadata = io.metadata("link.txt")?',
           '        if link_value != link_target or not (link_metadata.kind is io.entry_kind.symlink):',
           '            return Ok(90)',
           '        var link_removed = io.remove_file("link.txt")?',
           '    else:',
           '        return Ok(91)',
           '    var empty = io.create_directory("empty")?',
           '    var removed_empty = io.remove_directory("empty")?',
           '    var removed = io.remove_tree("moved")?',
           '    var present = io.exists("moved")?', '    if present:', '        return Ok(86)',
           '    var normalized = path.normalize("a/./b/../c")?',
           '    var joined = path.join(["a", "b.txt"])?',
           '    var filename = path.filename(joined)?',
           '    var extension = path.extension(joined)?',
           '    var stem = path.stem(joined)?',
           '    var changed = path.change_extension(joined, "bin")?',
           '    var parent = path.parent(joined)?',
           '    var absolute = path.absolute(joined)?',
           '    var rooted = path.is_rooted(absolute)?',
           '    var is_absolute = path.is_absolute(absolute)?',
           '    var is_relative = path.is_relative(joined)?',
           '    var root = path.root(absolute)?',
           '    var relative = path.relative(absolute)?',
           '    var separator = path.separator()?',
           '    if normalized != "a/c" or filename != "b.txt" or extension != ".txt" or stem != "b":',
           '        return Ok(87)',
           '    if parent != "a" or not rooted or not is_absolute or not is_relative or relative != joined:',
           '        return Ok(88)',
           '    return Ok(7)', '',
           'func main() -> i32:', '    if exercise() is Ok(var answer):',
           '        if answer != 7:', '            print("check {answer} failed")', '            return answer',
           '        return 0', '    return 99', '']
source.insert(source.index('    return Ok(7)'), '        var closed = handle.close()?')

# The expected UTF-8 byte count is independent of the source string's scalar count.
text_size = len('first\0雪 tail'.encode())
source = [line.replace('details.size != 15', f'details.size != {text_size}') for line in source]

# VOID forbids local shadowing of imported declaration names, even qualified ones.
import re
reserved = ('entries', 'filename', 'extension', 'stem', 'parent', 'absolute',
            'is_absolute', 'is_relative', 'root', 'relative', 'separator')
source = [re.sub(r'(?<!\.)\b(' + '|'.join(reserved) + r')\b',
                 lambda match: 'observed_' + match[0], line) for line in source]

def async_fixture(selected=None):
    lines = []
    chosen = source[:source.index('func main() -> i32:')] if selected is None else selected
    for line in chosen:
        if line.startswith('func wire(') or line.startswith('func exercise('):
            line = 'async ' + line
        if line.startswith(' '):
            matches = list(re.finditer(r'(?P<target>io\.[a-z_]+|handle\.[a-z_]+|wire)(?P<generic><[^>]+>)?\(', line))
            for match in reversed(matches):
                start = match.start()
                opening = match.end() - 1
                depth = 1
                closing = opening + 1
                while depth:
                    if line[closing] == '(':
                        depth += 1
                    elif line[closing] == ')':
                        depth -= 1
                    closing += 1
                target = match['target']
                selected = target if target == 'wire' else target + '_async'
                call = selected + (match['generic'] or '') + line[opening:closing]
                line = line[:start] + '(await ' + call + ')' + line[closing:]
        lines.append(line)
    lines += ['async func entry(input: i32) -> i32:', '    var outcome = await exercise()',
              '    if outcome is Ok(var answer):', '        return answer', '    return 99', '']
    return '\n'.join(lines)


def main():
    global compiler, checks
    if sys.argv[1] == '--generate-async':
        Path(sys.argv[2]).write_text(async_fixture(), encoding='utf-8')
        return
    if sys.argv[1] == '--generate-errors-async':
        selected = (Path(__file__).parent / 'errors.void').read_text(encoding='utf-8').splitlines()
        selected = selected[:selected.index('func entry(input: i32) -> i32:')]
        Path(sys.argv[2]).write_text(async_fixture(selected), encoding='utf-8')
        return
    compiler = Path(sys.argv[1]).resolve()
    if '--async' in sys.argv:
        runner = Path(sys.argv[2]).resolve()
        with tempfile.TemporaryDirectory(prefix='VOID io async wire ') as directory:
            root = Path(directory)
            fixture = root / 'main.void'
            fixture.write_text(async_fixture(), encoding='utf-8')
            run([compiler, 'check', fixture], root, expected=b'Source checked successfully\n',
                source_inputs=(fixture,))
            run([runner, '--async', fixture], root, expected=None, source_inputs=(fixture,))
            for endian in ('little', 'big'):
                expected = b''.join(wire if endian == 'little' else wire[::-1] for _, _, wire in vectors)
                expected += (5).to_bytes(4, endian) + '雪\0x'.encode() + b'raw'
                assert (root / (endian + '.bin')).read_bytes() == expected
        print(f'{len(vectors)} numeric vectors, both byte orders and filesystem APIs passed async JIT/O2')
        return
    with tempfile.TemporaryDirectory(prefix='VOID io source ') as directory:
        root = Path(directory)
        fixture = root / 'main.void'
        fixture.write_text('\n'.join(source), encoding='utf-8')
        run([compiler, 'check', fixture], root, expected=b'Source checked successfully\n',
            source_inputs=(fixture,))
        for mode in ('jit', 'optimized', 'native', 'native-optimized'):
            cwd = root / mode
            cwd.mkdir()
            if mode == 'jit':
                command = [compiler, 'run', fixture, '--no-opt']
            elif mode == 'optimized':
                command = [compiler, 'run', fixture]
            else:
                executable = cwd / ('io.exe' if os.name == 'nt' else 'io')
                arguments = [compiler, 'build', fixture, '-o', executable]
                if mode == 'native':
                    arguments += ['--no-opt']
                run(arguments, cwd, expected=None, source_inputs=(fixture,))
                command = [executable]
            run(command, cwd, source_inputs=(fixture,))
            for endian in ('little', 'big'):
                payload = b''.join(wire if endian == 'little' else wire[::-1] for _, _, wire in vectors)
                payload += (5).to_bytes(4, endian) + '雪\0x'.encode() + b'raw'
                actual = (cwd / (endian + '.bin')).read_bytes()
                assert actual == payload, (mode, endian, actual, payload)
                checks += 1
        # Invalid readonly authority, unsupported numerics, and worker effects stay rejected.
        for body in (
            'func rejected(const handle: io.file):\n    var result = handle.seek(0)\n',
            'func rejected(handle: io.file):\n    var result = handle.read<bool>()\n',
            'func worker() -> i32:\n    var result = io.exists("x")\n    return 1\n'
            'async func entry(value: i32) -> i32:\n    return await spawn worker()\n',
        ):
            fixture.write_text('use io\n' + body, encoding='utf-8')
            run([compiler, 'check', fixture], root, expected=None, success=False,
                source_inputs=(fixture,))
    print(f'{checks} public source/file checks passed; {len(vectors)} numeric vectors, both byte orders, '
          f'JIT/O2/linked AOT; 1 MiB stack={small}')

if __name__ == '__main__':
    main()
