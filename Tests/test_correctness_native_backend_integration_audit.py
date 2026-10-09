#!/usr/bin/env python3
"""#360: validation scheduling, dead-code isolation and native composition."""
from __future__ import annotations

import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
VOIDC = ROOT / 'bin' / ('voidc.exe' if os.name == 'nt' else 'voidc')
NAME = 'CorrectnessNativeBackendIntegrationAudit'
FIXTURE = ROOT / 'Tests' / NAME
GENERATED = FIXTURE / '.void' / (NAME + '.c')
EXPECTED = 'True\n' * 32
COUNT = 0


def expect(ok, detail):
    global COUNT
    if not ok:
        raise AssertionError(detail)
    COUNT += 1
    print('True', flush=True)


def invoke(*args, **kwargs):
    return subprocess.run(list(map(str, args)), cwd=ROOT, capture_output=True,
                          timeout=240, **kwargs)


def negative(path, source, command='check', project=False, local='value'):
    path.write_text(source, encoding='utf-8')
    result = invoke(VOIDC, command, path.parent if project else path,
                    '--diagnostics=json', text=True)
    records = [json.loads(line) for line in result.stderr.splitlines() if line.startswith('{')]
    ok = result.returncode != 0 and len(records) == 1
    if ok:
        record = records[0]
        span = record.get('span') or {}
        start = (span.get('start') or {}).get('offset', -1)
        end = (span.get('end') or {}).get('offset', -1)
        ok = (record.get('code') == 'VOID3000' and
              'definitely assigned' in record.get('message', '') and
              source[start:end] == local)
    expect(ok, f'{command} {path}: {result.stdout}\n{result.stderr}')


def lsp(path, source, methods):
    messages = [dict(jsonrpc='2.0', id=1, method='initialize', params={}),
                dict(jsonrpc='2.0', method='textDocument/didOpen', params={
                    'textDocument': dict(uri=path.as_uri(), languageId='void', version=1, text=source)})]
    messages += methods
    messages += [dict(jsonrpc='2.0', id=90, method='shutdown', params=None),
                 dict(jsonrpc='2.0', method='exit', params=None)]
    wire = b''
    for message in messages:
        body = json.dumps(message).encode()
        wire += f'Content-Length: {len(body)}\r\n\r\n'.encode() + body
    result = invoke(VOIDC, 'lsp', input=wire)
    assert result.returncode == 0 and not result.stderr, result.stderr
    remaining = result.stdout
    replies = []
    while remaining:
        header, _, content = remaining.partition(b'\r\n\r\n')
        size = int(header.split(b':')[1])
        replies.append(json.loads(content[:size]))
        remaining = content[size:]
    return replies


def main():
    result = invoke(VOIDC, 'version', text=True)
    expect(result.returncode == 0 and result.stdout.strip() == 'voidc 0.0.381', result.stdout)
    built = invoke(VOIDC, 'build', FIXTURE, text=True)
    expect(built.returncode == 0 and not built.stderr, built.stdout + built.stderr)
    exe = FIXTURE / 'bin' / (NAME + ('.exe' if os.name == 'nt' else ''))
    result = invoke(exe, text=True)
    expect(result.returncode == 0 and result.stdout == EXPECTED and not result.stderr,
           result.stdout + result.stderr)
    first = GENERATED.read_bytes()
    built = invoke(VOIDC, 'build', FIXTURE, text=True)
    expect(built.returncode == 0 and not built.stderr and GENERATED.read_bytes() == first,
           'nondeterministic generated C: ' + built.stderr)
    text = first.decode()
    expect('"Dead360.Unused()"' not in text and '"Dead360.Initialize()"' not in text,
           'validation activated dead method/static-initializer emission')
    expect(text.count('{"Box__g1_Audit360__dReader",') == 1,
           'aliases duplicated a generic type specialization')

    with tempfile.TemporaryDirectory(prefix='void360-audit-') as temp:
        base = Path(temp)
        compilers = [shlex.split(os.environ.get('CC', 'cc'))]
        if shutil.which('clang') and os.name != 'nt':
            compilers.append(['clang'])
        for compiler in compilers:
            for optimization in ('-O0', '-O2'):
                native = base / ('native' + ('.exe' if os.name == 'nt' else ''))
                flags = ['-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', optimization,
                         '-I' + str(ROOT / 'Runtime/include')]
                if os.name != 'nt':
                    flags.append('-pthread')
                result = invoke(*compiler, *flags, GENERATED,
                                *sorted((ROOT / 'Runtime/src').glob('*.c')), '-lm', '-o', native, text=True)
                assert result.returncode == 0 and not result.stderr, result.stderr
                result = invoke(native, text=True)
                assert result.returncode == 0 and result.stdout == EXPECTED and not result.stderr, result
            print(f'{compiler[0]} strict C -O0/-O2: passed')
        expect(True, 'strict C and optimization-independent results')
        if os.name != 'nt':
            native = base / 'ubsan'
            result = invoke(*compilers[0], '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                            '-O1', '-fsanitize=undefined', '-fno-sanitize-recover=undefined', '-pthread',
                            '-I' + str(ROOT / 'Runtime/include'), GENERATED,
                            *sorted((ROOT / 'Runtime/src').glob('*.c')), '-lm', '-o', native, text=True)
            assert result.returncode == 0 and not result.stderr, result.stderr
            result = invoke(native, text=True)
            assert result.returncode == 0 and result.stdout == EXPECTED and not result.stderr, result
            print('UBSan: passed')
        expect(True, 'native sanitizer checks')

        folder = base / 'negative'
        folder.mkdir()
        path = folder / 'Program.void'
        broken = 'static int Broken(){int value; return value;}'
        main = 'public static void Main(){Console.WriteLine(1);}'
        source = 'public static class Program {' + broken + main + '}'
        # All CLI entry paths share validation; no executable is allowed to run.
        for command in ('check', 'build', 'run'):
            negative(path, source, command)
        project = folder / 'Audit.voidproj'
        project.write_text('{"format":1,"name":"Audit","output":"exe","version":"0.0.360"}\n')
        for command in ('check', 'build', 'run'):
            negative(path, source, command, project=True)
        project.unlink()
        negative(path, 'public static class Program {' + main + broken + '}')
        # Existing root diagnostics take priority over unrelated unused errors.
        negative(path, 'public static class Program {' + broken +
                 'public static void Main(){int other;Console.WriteLine(other);}}', local='other')
        negative(path, 'public class Unused {public int Broken(){int value;return value;}}'
                 'public static class Program {' + main + '}')
        negative(path, 'public class Unused {public Unused(){int value;Console.WriteLine(value);}}'
                 'public static class Program {' + main + '}')
        negative(path, 'public class Unused {public int Broken {get {int value;return value;}}}'
                 'public static class Program {' + main + '}')
        negative(path, 'public class Unused {public int Broken {get {return 0;}set {int other;Console.WriteLine(other);}}}'
                 'public static class Program {' + main + '}', local='other')
        negative(path, 'using Void;public class Unused {public Func<int> Broken(){int value;return ()=>value;}}'
                 'public static class Program {' + main + '}')
        # Separate source files and reverse declaration discovery order.
        for other_name in ('A.void', 'Z.void'):
            path.write_text('public static class Program {' + main + '}')
            other = folder / other_name
            other_source = 'public static class Other {' + broken + '}'
            project.write_text('{"format":1,"name":"Audit","output":"exe","version":"0.0.360"}\n')
            negative(other, other_source, project=True)
            other.unlink()
            project.unlink()
        # Library export roots must not determine validity of private bodies.
        project.write_text('{"format":1,"name":"Audit","output":"library","version":"0.0.360"}\n')
        negative(path, 'public static class Library {' + broken + '}', project=True)
        project.unlink()
        # Open templates remain specialization-time native bodies. These cases
        # explicitly record the boundary rather than inventing symbolic T values.
        generic = 'static int Broken<T>(){int value;return value;}'
        deferred = 'public static class Program {' + generic + main + '}'
        path.write_text(deferred)
        result = invoke(VOIDC, 'check', path, text=True)
        expect(result.returncode == 0 and not result.stderr, result.stderr)
        negative(path, 'public static class Program {' + generic +
                 'public static void Main(){Console.WriteLine(Broken<int>());}}')
        dependent = ('public static class Program {static T Add<T>(T a){return a-1;}' + main + '}')
        path.write_text(dependent)
        result = invoke(VOIDC, 'check', path, text=True)
        expect(result.returncode == 0 and not result.stderr, result.stderr)
        path.write_text(dependent.replace('Console.WriteLine(1)', 'Console.WriteLine(Add<int>(7))'))
        result = invoke(VOIDC, 'check', path, text=True)
        expect(result.returncode == 0 and not result.stderr, result.stderr)
        path.write_text(dependent.replace('Console.WriteLine(1)', 'Console.WriteLine(Add<string>("x"))'))
        result = invoke(VOIDC, 'check', path, text=True)
        expect(result.returncode != 0, 'type-dependent generic error must fail at specialization')
        # Force semantic binding storage to grow while resolving a ref-field RHS.
        # Source order previously changed whether its borrowed LHS binding survived.
        padding = 'value += 0;' * 1200
        index_method = 'public static int Index(){int value=0;' + padding + 'return value;}'
        main_method = ('public static void Main(){int[] items=new int[1];'
                       'Cell cell=new Cell(items);Console.WriteLine(cell.Read());}')
        cell = ('public ref struct Cell {private ref int _value;'
                'public Cell(int[] items){_value=ref items[Program.Index()];}'
                'public int Read(){return _value;}}')
        for methods in (main_method + index_method, index_method + main_method):
            path.write_text(cell + 'public static class Program {' + methods + '}')
            result = invoke(VOIDC, 'check', path, text=True)
            expect(result.returncode == 0 and not result.stderr,
                   (result.returncode, result.stdout, result.stderr))
        # The same ref-rebinding rule must survive growth of the local table:
        # many inline out declarations are introduced while binding the RHS.
        parameters = ','.join(f'out int a{i}' for i in range(40))
        assignments = ''.join(f'a{i}=0;' for i in range(40))
        arguments = ','.join(f'out var a{i}' for i in range(40))
        path.write_text('public static class Program {public static void Main(){'
                        'int scalar=7;ref int alias=ref scalar;int[] items=new int[1];'
                        'alias=ref items[Offset(' + arguments + ')];alias=9;'
                        'Console.WriteLine(items[0]==9 && scalar==7);}'
                        'public static int Offset(' + parameters + '){' + assignments + 'return 0;}}')
        result = invoke(VOIDC, 'run', path, text=True)
        expect(result.returncode == 0 and not result.stderr and result.stdout.endswith('True\n'),
               (result.returncode, result.stdout, result.stderr))
        # The LSP in-memory source override must report the same unused-body error.
        path.write_text('public static class Program {' + main + '}')
        replies = lsp(path, source, [])
        published = [r for r in replies if r.get('method') == 'textDocument/publishDiagnostics']
        diagnostics = [d for r in published for d in r['params']['diagnostics']]
        expect(len(diagnostics) == 1 and 'definitely assigned' in diagnostics[0]['message'], replies)
    print(f'# {COUNT} checks')


if __name__ == '__main__':
    main()
