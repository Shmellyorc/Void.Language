"""Focused reproductions; Linux emulation is not native Windows/macOS acceptance.

Run from the repository: python3 <this file> <build-dir> [encoding-child ...]
"""
from pathlib import Path
from unittest.mock import patch
import json
import os
import runpy
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[4]
BASELINE = '5198c7ac9c256fd05f53ff5ba594c38dc0093b7f'
BUILD = Path(sys.argv[1]).resolve()
sys.path.insert(0, str(ROOT / 'tools'))
from test_platform import native_text, small_stack


def encoding_child(script_name, baseline=False):
    script = ROOT / 'tests' / script_name
    sys.path.insert(0, str(script.parent))
    accesses = []
    original_open = Path.open

    def locale_open(path, mode='r', buffering=-1, encoding=None, errors=None, newline=None):
        if 'b' not in mode and (path.suffix == '.void' or path.name == 'void-native.txt'):
            accesses.append({'file': str(path), 'mode': mode, 'encoding': encoding})
            if encoding in (None, 'locale'):
                encoding = 'cp1252'
        return original_open(path, mode, buffering, encoding, errors, newline)

    target = BUILD / ('voidls' if '/lsp.py' in script_name else 'voidc')
    with patch.object(sys, 'argv', [str(script), str(target)]), patch.object(Path, 'open', locale_open):
        try:
            if baseline:
                source = subprocess.check_output(['git', 'show', f'{BASELINE}:tests/{script_name}'], cwd=ROOT)
                exec(compile(source, str(script), 'exec'), {'__name__': '__main__', '__file__': str(script)})
            else:
                runpy.run_path(str(script), run_name='__main__')
        except BaseException as error:
            if not baseline:
                raise
            current = error
            while current and not isinstance(current, (UnicodeEncodeError, UnicodeDecodeError)):
                current = current.__context__
            assert current is not None, repr(error)
            print(json.dumps({'baseline': script_name, 'reproduced': str(current)}))
            return
    assert not baseline, 'baseline unexpectedly passed the cp1252 reproduction'
    assert accesses and all(item['encoding'] == 'utf-8' for item in accesses), accesses
    print(json.dumps({'fixed': script_name, 'explicit_utf8_accesses': len(accesses)}))


def manifests(work):
    clang = ROOT / '.toolchains/llvm-23.1.4/bin/clang'
    linker = clang.with_name('ld.lld')
    for platform in ('Windows', 'Darwin', 'Linux'):
        project = work / platform
        project.mkdir()
        (project / 'main.c').write_text('int main(void) { return 0; }\n', encoding='utf-8')
        cmake = f'''cmake_minimum_required(VERSION 3.25)
project(native_flags C)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${{CMAKE_BINARY_DIR}}/bin")
add_executable(voidc main.c)
add_library(void_runtime STATIC main.c)
add_library(void_native_fixture STATIC main.c)
set(VOID_CLANG "{clang.as_posix()}")
set(VOID_LINKER "{linker.as_posix()}")
set(WIN32 {'TRUE' if platform == 'Windows' else 'FALSE'})
set(APPLE {'TRUE' if platform == 'Darwin' else 'FALSE'})
set(UNIX {'FALSE' if platform == 'Windows' else 'TRUE'})
set(VOID_APPLE_LIBCXX_INCLUDE_DIR "/fixture/Apple SDK/c++/v1")
set(VOID_APPLE_SYSROOT "/fixture/Apple SDK")
include("{ROOT.as_posix()}/cmake/NativeBuild.cmake")
'''
        (project / 'CMakeLists.txt').write_text(cmake, encoding='utf-8')
        result = subprocess.run(['cmake', '-S', str(project), '-B', str(project / 'build')],
                                capture_output=True, timeout=30)
        assert result.returncode == 0, result.stderr
        flags = (project / 'build/bin/void-native.txt').read_text(encoding='utf-8').splitlines()[5:]
        assert ('-pthread' in flags) == (platform != 'Windows'), flags
        assert ('-fms-runtime-lib=static' in flags) == (platform == 'Windows'), flags
        if platform == 'Windows':
            assert {'-llegacy_stdio_definitions', '-lbcrypt', '-lshell32', '-lole32', '-luuid'} <= set(flags)
        if platform == 'Darwin':
            assert {'-stdlib=libc++', '-nostdinc++', '-isystem', '/fixture/Apple SDK/c++/v1',
                    '-isysroot', '/fixture/Apple SDK', 'Foundation', '-lobjc'} <= set(flags)
        print(json.dumps({'generated_flags_only': platform, 'flags': flags}))


def aliases(work):
    sys.path.insert(0, str(ROOT / 'tests/text_encoding'))
    import crash
    project = work / 'project'
    project.mkdir()
    alias = work / 'ancestor-alias'
    alias.symlink_to(project, target_is_directory=True)
    source = project / 'same.void'
    other = project / 'nested/same.void'
    other.parent.mkdir()
    raw = b'# exact launch bytes\r\n\0\xff'
    source.write_bytes(raw)
    other.write_bytes(b'# companion\r\n')
    evidence = work / 'incidents'
    command = [str(Path(sys.executable).resolve()), '-c',
               'import pathlib,sys; pathlib.Path(sys.argv[1]).write_bytes(b"edited"); sys.exit(87)',
               str(alias / source.name)]
    with patch.dict(os.environ, {'VOID_CRASH_EVIDENCE': str(evidence)}):
        result = crash.run(command, cwd=alias, capture_output=True, source_inputs=(source, alias / source.name))
    assert result.returncode == 87
    receipt, = evidence.glob('*/receipt.json')
    record = json.loads(receipt.read_text(encoding='utf-8'))
    assert record['command'] == command and record['cwd'] == str(project.resolve())
    originals = [item['original'] for item in record['sources']]
    assert len(originals) == len(set(originals)), 'alias duplicated a source snapshot'
    entries = [item for item in record['sources'] if Path(item['original']).is_relative_to(project)]
    assert len(entries) == 2
    selected = next(item for item in entries if item['original'] == str(source.resolve()))
    assert Path(selected['copy']).read_bytes() == raw
    assert selected['captured_at'] == 'subprocess_launch'
    assert selected['current_sha256'] != selected['original_sha256']
    assert len({item['copy'] for item in entries}) == 2
    print('Canonical ancestor alias: exact argv, launch bytes, edited hash, unique source identities and colliding basenames passed')


def stdlib_alias(work):
    alias = work / 'stdlib-alias'
    alias.symlink_to(ROOT / 'stdlib', target_is_directory=True)
    environment = dict(os.environ, VOID_TEST_STDLIB=str(alias), VOID_STDLIB_ROOT=str(alias))
    script = ROOT / 'tests/collections_stdlib/lsp.py'
    for baseline in (True, False):
        mode = 'stdlib-alias-baseline' if baseline else 'stdlib-alias-fixed'
        result = subprocess.run([sys.executable, str(Path(__file__).resolve()), str(BUILD), mode],
                                capture_output=True, env=environment, timeout=60)
        if baseline:
            assert result.returncode == 1 and b'Import escapes the project root' in result.stderr, result
        else:
            assert result.returncode == 0, result.stderr
        print(json.dumps({'stdlib_ancestor_alias': mode, 'returncode': result.returncode,
                          'expected_definition_result_verified': True}))


if len(sys.argv) > 2 and sys.argv[2] == 'encoding-child':
    encoding_child(sys.argv[3], '--baseline' in sys.argv)
elif len(sys.argv) > 2 and sys.argv[2].startswith('stdlib-alias-'):
    script = ROOT / 'tests/collections_stdlib/lsp.py'
    sys.path.insert(0, str(script.parent))
    sys.argv = [str(script), str(BUILD / 'voidls'), *sys.argv[2:]]
    if sys.argv[2] == 'stdlib-alias-baseline':
        source = subprocess.check_output(['git', 'show', f'{BASELINE}:tests/collections_stdlib/lsp.py'], cwd=ROOT)
        exec(compile(source, str(script), 'exec'), {'__name__': '__main__', '__file__': str(script)})
    else:
        runpy.run_path(str(script), run_name='__main__')
else:
    assert os.name == 'posix', 'these local emulations require POSIX; use native CTest on Windows'
    with tempfile.TemporaryDirectory(prefix='void-platform-contracts-') as directory:
        work = Path(directory)
        manifests(work)
        aliases(work)
        stdlib_alias(work)
    import resource
    before = resource.getrlimit(resource.RLIMIT_STACK)
    child = subprocess.run([sys.executable, '-c', 'import resource; print(resource.getrlimit(resource.RLIMIT_STACK))'],
                           preexec_fn=small_stack, capture_output=True, timeout=10)
    assert child.returncode == 0 and child.stdout == b'(1048576, 1048576)\n', child
    assert resource.getrlimit(resource.RLIMIT_STACK) == before
    print(child.stderr.decode('ascii').strip())
    with patch.object(os, 'linesep', '\r\n'):
        assert 'INFO "" "雪"\n'.encode('utf-8') + native_text('7\n') == 'INFO "" "雪"\n7\r\n'.encode('utf-8')
    cases = ['os/errors.py', 'os/lsp.py', 'text_encoding/lsp.py', 'log/lsp.py',
             'collections_stdlib/lsp.py', 'channel/execution.py']
    for script in cases:
        for suffix in (['--baseline'], []):
            subprocess.run([sys.executable, str(Path(__file__).resolve()), str(BUILD), 'encoding-child', script, *suffix],
                           check=True, timeout=240, env=dict(os.environ, PYTHONUTF8='0'))
    print('All platform contract checks passed; native Windows/macOS execution remains pending')
