#!/usr/bin/env python3
"""Permanent host-process tests; the production Windows encoder runs on Linux.
Native Windows: use MinGW/w64devkit CC and Python, from the repository root.
"""
import json
import os
from pathlib import Path
import random
import re
import shutil
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]


def crt_decode(line):
    # Independent CRT parser, including its special executable-name rule.
    assert line[0] == '"'
    end = line.index('"', 1)
    result = [line[1:end]]
    i = end + 1
    while i < len(line):
        while i < len(line) and line[i] in ' \t':
            i += 1
        if i == len(line):
            break
        arg, quoted = '', False
        while i < len(line):
            if line[i] in ' \t' and not quoted:
                break
            slashes = 0
            while i < len(line) and line[i] == '\\':
                slashes += 1
                i += 1
            if i < len(line) and line[i] == '"':
                arg += '\\' * (slashes // 2)
                if slashes % 2:
                    arg += '"'
                else:
                    quoted = not quoted
                i += 1
            else:
                arg += '\\' * slashes
                if i < len(line) and (quoted or line[i] not in ' \t'):
                    arg += line[i]
                    i += 1
                else:
                    break
        assert not quoted
        result.append(arg)
    return result


def main():
    with tempfile.TemporaryDirectory(prefix='void host 雪 ', dir=ROOT) as temporary, \
            tempfile.TemporaryDirectory(prefix='void-compiler-host-', dir=ROOT) as compiler_temporary:
        directory = Path(temporary)
        compiler_directory = Path(compiler_temporary)
        binary = directory / ('process probe.exe' if os.name == 'nt' else 'process probe')
        compiled = compiler_directory / ('probe.exe' if os.name == 'nt' else 'probe')
        command = [os.environ.get('CC') or 'cc', '-std=c11', '-Wall', '-Wextra', '-Wpedantic',
                   '-Werror', '-O2', '-ICompiler/include', 'Compiler/src/host_process.c',
                   'Tests/HostProcessPortability/driver.c', '-o', str(compiled)]
        if os.name == 'nt':
            command.append('-municode')
        subprocess.run(command, cwd=ROOT, check=True)
        shutil.copy2(compiled, binary)

        def invoke(*args, env=None):
            result = subprocess.run([str(binary), *args], stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE, env=env)
            assert result.returncode == 0, (args, result.returncode, result.stdout, result.stderr)
            return result.stdout.replace(b'\r\n', b'\n')

        cases = ['', ' ', '\t', 'a b', '"', '\\', '\\\\', 'a"b', 'a\\"b', 'a\\\\"b',
                 'trailing space\\', 'C:\\path with spaces\\', '雪 😀 café', '$HOME; &|<>%']
        generator = random.Random(330)
        cases += [''.join(generator.choice('ab \\"\t雪😀') for _ in range(generator.randrange(40)))
                  for _ in range(512)]
        for start in range(0, len(cases), 32):
            arguments = ['C:\\tool dir\\tool.exe', *cases[start:start + 32]]
            line = invoke('encode', *arguments).decode('utf-8').rstrip('\n')
            assert crt_decode(line) == arguments
        assert invoke('encode', 'cc', '', 'a"b', 'x\\') == b'"cc" "" "a\\"b" "x\\\\"\n'
        bad = subprocess.run([str(binary), 'encode', 'bad"name'], capture_output=True)
        assert bad.returncode == 3
        for start in range(0, len(cases), 32):
            arguments = cases[start:start + 32]
            expected = b''.join(str(len(a.encode())).encode() + b':' + a.encode() + b'\n' for a in arguments)
            assert invoke('run', str(binary), 'child', *arguments) == expected + b'RESULT:1:0:\n'
        for code in [0, 7, 127, 255] + ([259, 4294967295] if os.name == 'nt' else []):
            assert invoke('run', str(binary), 'exit', str(code)) == f'RESULT:1:{code}:\n'.encode()
        missing = invoke('run', str(directory / 'missing executable'))
        assert b'RESULT:0:999:could not execute' in missing
        assert b'RESULT:0:999:invalid host process arguments' in invoke('invalid')
        assert b'RESULT:0:999:invalid host process arguments' in invoke('run', '')
        (compiler_directory / 'cwd-marker').write_text('marker')
        inherited = subprocess.run([str(binary), 'run', str(binary), 'io'],
                                   cwd=compiler_directory, input=b'inherited input\n',
                                   env=dict(os.environ, VOID_HOST_PROCESS_TEST='environment'),
                                   capture_output=True, check=True)
        assert inherited.stdout.replace(b'\r\n', b'\n') == b'environment:inherited input\nRESULT:1:0:\n'
        assert inherited.stderr.replace(b'\r\n', b'\n') == b'inherited stderr\n'
        started = time.monotonic()
        assert invoke('run', str(binary), 'delay') == b'waited\nRESULT:1:0:\n'
        assert time.monotonic() - started >= 1
        # Check repeated success/failure in one parent for descriptor/handle leaks.
        assert invoke('repeat-spawn', str(binary)) == b''
        failed_child = subprocess.run([str(binary), 'repeat-spawn',
                                       str(directory / 'missing child')], capture_output=True)
        assert failed_child.returncode == 2, (failed_child.returncode, failed_child.stderr)
        assert b'repeat-spawn: child iteration=0 exit=999: could not execute' in failed_child.stderr, failed_child.stderr
        if os.name == 'nt':
            traced = subprocess.run([str(binary), 'repeat-spawn', str(binary)],
                                    capture_output=True,
                                    env=dict(os.environ, VOID_HOST_PROCESS_TRACE='1'))
            assert traced.returncode == 0, (traced.returncode, traced.stderr)
            counts = dict((stage.decode(), int(count)) for stage, count in re.findall(
                rb'repeat-spawn: (initial|first child|first missing executable|warm-up|repeated loop) handles=(\d+)',
                traced.stderr))
            assert len(counts) == 5, traced.stderr
            assert counts['first missing executable'] == counts['warm-up'] == counts['repeated loop'], traced.stderr
            leaked = subprocess.run([str(binary), 'repeat-spawn-leak', str(binary)],
                                    capture_output=True)
            assert leaked.returncode == 2, (leaked.returncode, leaked.stderr)
            assert b'repeat-spawn: handle count mismatch' in leaked.stderr, leaked.stderr
            mismatch = re.search(rb'handle count mismatch before=(\d+) after=(\d+)', leaked.stderr)
            assert mismatch and int(mismatch[2]) - int(mismatch[1]) == 64, leaked.stderr
        bare = directory / ('path-probe.exe' if os.name == 'nt' else 'path-probe')
        shutil.copy2(binary, bare)
        env = dict(os.environ, PATH=str(directory) + os.pathsep + os.environ.get('PATH', ''))
        assert invoke('run', 'path-probe', 'exit', '7', env=env) == b'RESULT:1:7:\n'
        relative = os.path.relpath(binary, ROOT)
        assert invoke('run', relative, 'exit', '0') == b'RESULT:1:0:\n'
        # Verify compiler diagnostics consume the shared API's two outcomes.
        project = compiler_directory / 'Project'
        project.mkdir()
        (project / 'Project.voidproj').write_text(json.dumps({
            'format': 1, 'name': 'Project', 'output': 'exe', 'version': '0.1.0'}))
        (project / 'Program.void').write_text('public static class Program { public static void Main() {} }')
        failure = compiler_directory / ('failure compiler.exe' if os.name == 'nt' else 'failure compiler')
        shutil.copy2(binary, failure)
        for tool, expected in [(str(compiler_directory / 'missing compiler'), b'could not execute'),
                               (str(failure), b'C compiler exited with code 127'),
                               (str(failure) + ' --flags', b'could not execute')]:
            environment = dict(os.environ, CC=tool)
            result = subprocess.run([str(ROOT / 'bin' / 'voidc'), 'build', str(project)],
                                    capture_output=True, env=environment)
            assert result.returncode != 0
            assert expected in result.stdout + result.stderr, result.stderr
        if os.name != 'nt':
            assert b'RESULT:0:999:process terminated by signal 15' in invoke('run', str(binary), 'signal')
            denied = directory / 'not executable'
            denied.write_text('nothing')
            denied.chmod(0o600)
            assert b'Permission denied' in invoke('run', str(denied))
        else:
            # Windows limit includes the terminating UTF-16 NUL.
            assert b'oversized process command line' in invoke('oversized')
            assert b'invalid UTF-8' in invoke('invalid-utf8')
    # One guarded cumulative check representing this complete standalone suite.
    print('True')


if __name__ == '__main__':
    main()
