"""CI #88: physical output ancestors, exact descendants and artifact safety."""
import os
import pathlib
import subprocess
import sys
import tempfile

compiler = pathlib.Path(sys.argv[1]).resolve()
checks = 0


def run(args, status=0, diagnostic=None):
    global checks
    result = subprocess.run([str(a) for a in args], capture_output=True,
                            text=True, encoding='utf-8', timeout=90)
    checks += 1
    assert result.returncode == status, (args, result.returncode, result.stdout, result.stderr)
    if diagnostic:
        assert diagnostic in result.stderr, (args, result.stdout, result.stderr)
    return result


def short_path(path):
    import ctypes
    from ctypes import wintypes
    get = ctypes.WinDLL('kernel32', use_last_error=True).GetShortPathNameW
    get.argtypes = [wintypes.LPCWSTR, wintypes.LPWSTR, wintypes.DWORD]
    get.restype = wintypes.DWORD
    size = get(str(path), None, 0)
    assert size, (path, ctypes.get_last_error())
    buffer = ctypes.create_unicode_buffer(size)
    length = get(str(path), buffer, size)
    assert 0 < length < size, (path, ctypes.get_last_error())
    return pathlib.Path(buffer.value)


with tempfile.TemporaryDirectory(prefix='VOID CI88 é 🎮 # % ') as temporary:
    # Source selection uses its established canonical identity. Output spelling
    # is varied independently; an output alias never becomes a project VFS root.
    root = pathlib.Path(temporary).resolve()
    directory = root / 'Exact descendants é 🎮 # %'
    directory.mkdir()
    source = root / 'main.void'
    library = root / 'library.void'
    library.write_text('func value() -> i32:\n    return 7\n', encoding='utf-8')
    source.write_text('use "library.void" as library\nfunc main() -> i32:\n'
                      '    var xs: [string] = ["A🎮", "yes"]\n'
                      '    if xs[-1] == "yes":\n'
                      '        return library.value()\n    return 3\n', encoding='utf-8')
    roots = [root, pathlib.Path(temporary)]
    if os.name == 'nt':
        # Prefer an existing profile ancestor (RUNNER~1 on hosted Windows).
        # Never invent a short name or assume 8.3 generation is enabled for new files.
        alias = None
        for prefix in [*reversed(root.parents), root]:
            shortened = short_path(prefix)
            if '~' in str(shortened) and shortened != prefix:
                assert os.path.samefile(prefix, shortened), (prefix, shortened)
                alias = shortened / root.relative_to(prefix)
                break
        assert alias is not None, 'Native CI #88 gate requires a real OS 8.3 directory ancestor; none available'
        assert os.path.samefile(root, alias)
        print('OS 8.3 ancestor: ' + str(alias).encode('ascii', 'backslashreplace').decode())
        roots.extend([alias, pathlib.Path(str(alias)[0].swapcase() + str(alias)[1:])])
    else:
        print('Windows 8.3/reparse checks require native Windows; portable checks execute here.')
    roots = list(dict.fromkeys(roots))
    hardlink = directory / 'source-hard-link'
    os.link(library, hardlink)
    sentinel = directory / 'sentinel'
    sentinel.write_bytes(b'previous artifact')
    link = directory / 'output-symlink'
    try:
        link.symlink_to(sentinel)
    except OSError as error:
        if os.name != 'nt' or getattr(error, 'winerror', None) != 1314:
            raise
        print('Windows file symlink privilege 1314 unavailable; mandatory junction checks still execute.')
    else:
        for command in ['emit', 'build']:
            run([compiler, command, source, '-o', link], 1, 'ordinary file')
        assert sentinel.read_bytes() == b'previous artifact'

    for spelling in roots:
        for command, suffix in [('emit', '.o'), ('build', '.exe' if os.name == 'nt' else '.bin')]:
            output = spelling / directory.name / ('Exact Output é # %' + suffix)
            real = directory / output.name
            run([compiler, command, source, '-o', output])
            assert real.is_file() and real.stat().st_size > 0
            if command == 'build':
                result = run([real], 7)
                assert result.stdout == result.stderr == ''
            before = real.read_bytes()
            # Existing case-insensitive aliases reject; on a sensitive filesystem
            # the alternate spelling names a distinct, valid physical destination.
            wrong = output.with_name(output.name.swapcase())
            if wrong.exists():
                run([compiler, command, source, '-o', wrong], 1, 'incorrect spelling')
            else:
                run([compiler, command, source, '-o', wrong])
                assert not os.path.samefile(wrong, real)
            assert real.read_bytes() == before
            parent_case = spelling / directory.name.swapcase() / output.name
            if parent_case.parent.exists():
                run([compiler, command, source, '-o', parent_case], 1, 'incorrect spelling')
                assert real.read_bytes() == before
            for collision in [spelling / 'main.void', spelling / 'library.void',
                              spelling / directory.name / hardlink.name]:
                run([compiler, command, source, '-o', collision], 1, 'collision')
            assert library.read_text(encoding='utf-8') == 'func value() -> i32:\n    return 7\n'
            if os.name == 'nt':
                leaf = short_path(real).name
                if leaf != real.name:
                    run([compiler, command, source, '-o', output.with_name(leaf)], 1, 'incorrect spelling')
                    assert real.read_bytes() == before

    if os.name == 'nt':
        # CPython's native junction API avoids cmd.exe interpolation of '%' paths.
        import _winapi
        junction = root / 'Redirect ancestor é # %'
        _winapi.CreateJunction(str(directory), str(junction))
        try:
            redirects = [junction]
            shortened = short_path(junction)
            if shortened.name != junction.name:
                redirects.append(root / shortened.name)
            for redirect in redirects:
                for command in ['emit', 'build']:
                    run([compiler, command, source, '-o', redirect / sentinel.name], 1, 'reparse point')
                    assert sentinel.read_bytes() == b'previous artifact'
        finally:
            junction.rmdir()
    else:
        # Preserve POSIX/macOS ancestor-symlink behavior, including /var aliases.
        redirect = root / 'directory-link'
        redirect.symlink_to(directory, target_is_directory=True)
        for command, suffix in [('emit', '.o'), ('build', '.bin')]:
            run([compiler, command, source, '-o', redirect / ('linked' + suffix)])
            if command == 'build':
                run([directory / ('linked' + suffix)], 7)
    assert not list(root.rglob('.void-build-*')), 'Failed temporary artifacts remain'
print(f'CI #88 physical output checks passed: {checks}')
