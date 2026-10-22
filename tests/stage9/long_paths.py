"""CI #89 acceptance: actual long physical paths and unchanged safety contracts."""
import os
import json
import pathlib
import subprocess
import sys
import tempfile
from urllib.parse import urlsplit
from urllib.request import url2pathname

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / 'tools'))
from lsp_client import Client
from windows_process import windows_application_path

compiler = pathlib.Path(sys.argv[1]).resolve()
server = pathlib.Path(sys.argv[2]).resolve()
checks = 0


def run(args, status=0, diagnostic=None, executable=None):
    global checks
    result = subprocess.run([str(a) for a in args], capture_output=True,
                            text=True, encoding='utf-8', timeout=90, executable=executable)
    checks += 1
    assert result.returncode == status, (args, result.returncode, result.stdout, result.stderr)
    if diagnostic:
        assert diagnostic in result.stderr, (args, result.stdout, result.stderr)
    return result


with tempfile.TemporaryDirectory(prefix='VOID CI89 é 🎮 # % ') as temporary:
    deep = pathlib.Path(temporary)
    # Retain exactly the original six components and their original lengths.
    for n in range(6):
        deep /= 'nested-' + str(n) + '-' + 'x' * 36
    deep.mkdir(parents=True)
    source = deep / 'main.void'
    assert len(str(source)) > 260, source
    library = deep / 'Library.void'
    library.write_text('func value() -> i32:\n    return 7\n', encoding='utf-8')
    valid = 'use "Library.void" as library\nreturn library.value()\n'
    source.write_text(valid, encoding='utf-8')
    discovery = run([server, '--discover', deep, '', ''])
    selected = json.loads(discovery.stdout)
    assert os.path.samefile(selected['entry'], source), selected
    assert os.path.samefile(selected['root'], deep), selected
    run([server, '--discover', deep, '', deep / 'MAIN.void'], 2)
    with Client(server, source.resolve()) as client:
        client.initialize()
        uri = source.resolve().as_uri()
        client.notify('textDocument/didOpen', {'textDocument': {
            'uri': uri, 'languageId': 'void', 'version': 1, 'text': valid}})
        diagnostics = client.sync()
        assert all(not item['diagnostics'] for item in diagnostics), diagnostics
        definition = client.request('textDocument/definition', {
            'textDocument': {'uri': uri}, 'position': {'line': 0, 'character': 7}})[0]
        assert os.path.samefile(url2pathname(urlsplit(definition['result']['uri']).path), library), definition
        client.shutdown()
    run([compiler, 'check', source])
    run([compiler, 'run', source], 7)
    run([compiler, 'check', deep])
    run([compiler, 'run', deep], 7)
    linked = deep / 'Linked.void'
    try:
        linked.symlink_to(library)
    except OSError as error:
        if os.name != 'nt' or getattr(error, 'winerror', None) != 1314:
            raise
        print('Source symlink privilege 1314 unavailable; hard-link and junction gates remain.')
    else:
        source.write_text(valid.replace('Library.void', 'Linked.void'), encoding='utf-8')
        for command in ['check', 'run', 'emit', 'build']:
            args = ['-o', deep / 'denied-output'] if command in ['emit', 'build'] else []
            run([compiler, command, source, *args], 1, 'Symbolic links')
        linked.unlink()
        source.write_text(valid, encoding='utf-8')
    for command in ['check', 'run', 'emit', 'build']:
        args = ['-o', deep / ('rejected-' + command)] if command in ['emit', 'build'] else []
        run([compiler, command, deep / 'MAIN.void', *args], 1)
    hardlink = deep / 'source-hardlink'
    os.link(library, hardlink)
    for command, suffix in [('emit', '.o'), ('build', '.exe' if os.name == 'nt' else '.bin')]:
        output = deep / ('Exact Artifact é 🎮 # %' + suffix)
        run([compiler, command, source, '-o', output])
        assert output.stat().st_size > 0
        if command == 'build':
            application = None
            if os.name == 'nt':
                assert len(str(output).encode('utf-16-le')) // 2 > 260, output
                application = windows_application_path(output)
                assert os.path.samefile(output, application), (output, application)
            run([output], 7, executable=application)
            if os.name == 'nt':
                print('Deep executable launched via explicit extended application:', application)
        before = output.read_bytes()
        wrong = output.with_name(output.name.swapcase())
        if wrong.exists():
            run([compiler, command, source, '-o', wrong], 1, 'incorrect spelling')
        else:
            run([compiler, command, source, '-o', wrong])
            assert not os.path.samefile(output, wrong)
        assert output.read_bytes() == before
        for collision in [source, library, hardlink]:
            saved = collision.read_bytes()
            run([compiler, command, source, '-o', collision], 1, 'collision')
            assert collision.read_bytes() == saved
        source.write_text(valid.replace('Library.void', 'library.void'), encoding='utf-8')
        run([compiler, command, source, '-o', output], 1, 'case mismatch')
        assert output.read_bytes() == before
        # Two VFS identities for one inode must reject before publication.
        alias = deep / 'Alias.void'
        os.link(library, alias)
        source.write_text('use "Library.void" as library\nuse "Alias.void" as alias\n'
                          'return library.value()\n', encoding='utf-8')
        run([compiler, command, source, '-o', output], 1, 'Hard-link aliases')
        assert output.read_bytes() == before
        alias.unlink()
        source.write_text(valid, encoding='utf-8')
        link = deep / ('output-symlink' + suffix)
        try:
            link.symlink_to(output)
        except OSError as error:
            if os.name != 'nt' or getattr(error, 'winerror', None) != 1314:
                raise
            print('File symlink privilege 1314 unavailable; junction checks remain mandatory.')
        else:
            run([compiler, command, source, '-o', link], 1, 'ordinary file')
            assert output.read_bytes() == before
            link.unlink()
        if os.name == 'nt':
            import _winapi
            junction = deep / 'Redirect é # %'
            _winapi.CreateJunction(str(deep), str(junction))
            try:
                run([compiler, command, source, '-o', junction / output.name], 1, 'reparse point')
                assert output.read_bytes() == before
            finally:
                junction.rmdir()
        # Failed analysis must preserve an existing artifact as well.
        source.write_text('this is invalid VOID\n', encoding='utf-8')
        run([compiler, command, source, '-o', output], 1)
        assert output.read_bytes() == before
        source.write_text(valid, encoding='utf-8')
    assert not list(deep.rglob('.void-build-*'))
print(f'CI #89 long physical path checks passed: {checks}')
