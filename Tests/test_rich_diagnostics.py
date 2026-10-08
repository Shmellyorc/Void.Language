#!/usr/bin/env python3
"""Permanent structured diagnostics regressions, separated by milestone."""
import pathlib
import os
import subprocess
import sys
import tempfile
ROOT = pathlib.Path(__file__).resolve().parents[1]
STAGE = int(sys.argv[1])
def check(condition, detail):
    if not condition:
        raise AssertionError(detail)
    print('True')
def run(*args):
    # Diagnostic JSON and C fixtures emit UTF-8, independent of the host ACP.
    # Preserve bytes in any native-path text that is not valid UTF-8.
    return subprocess.run(args, cwd=ROOT, text=True, encoding='utf-8',
                          errors='surrogateescape', capture_output=True)
if STAGE == 321:
    with tempfile.TemporaryDirectory() as temp:
        exe = str(pathlib.Path(temp) / 'model')
        result = run('cc', '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2',
                     '-ICompiler/include', 'Tests/RichDiagnostics/model.c', 'Compiler/src/diagnostic.c', 'Compiler/src/lexer.c', '-o', exe)
        check(result.returncode == 0, result.stderr)
        result = run(exe)
        check(result.returncode == 0, result.stderr)
        check('error: primary' in result.stdout and 'note: context' in result.stdout and 'help: guidance' in result.stdout, result.stdout)
    result = run('./bin/voidc', 'lex', 'Tests/Diagnostics/Lexer/Program.void')
    check(result.returncode == 1 and 'error: unexpected character' in result.stderr, result.stderr)
    result = run('./bin/voidc', 'parse', 'Tests/Diagnostics/Parser/Program.void')
    check(result.returncode == 1 and 'error:' in result.stderr, result.stderr)
    result = run('./bin/voidc', 'check', 'Tests/Diagnostics/Semantic')
    check(result.returncode == 1 and 'error:' in result.stderr, result.stderr)
    result = run('./bin/voidc', 'check', 'Tests/Check/Valid')
    check(result.returncode == 0, result.stderr)
if STAGE == 322:
    with tempfile.TemporaryDirectory() as temp:
        exe = str(pathlib.Path(temp) / 'render')
        result = run('cc', '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2',
                     '-ICompiler/include', 'Tests/RichDiagnostics/render.c', 'Compiler/src/diagnostic.c', 'Compiler/src/lexer.c', '-o', exe)
        check(result.returncode == 0, result.stderr)
        path = pathlib.Path(temp) / 'source.void'
        for text, start, end, expected in [
            ('abc', 0, 3, '| ^~~'),
            ('abc', 3, 3, '|    ^'),
            ('\tfoo', 1, 4, '|     ^~~'),
            ('é x', 3, 4, '|   ^'),
            ('界 x', 4, 5, '|    ^'),
            ('e\u0301 x', 4, 5, '|   ^'),
            ('one\ntwo\nthree', 1, 9, '3 | three'),
            ('abc\n', 4, 4, '2 | '),
            ('abc\r\ndef', 5, 8, '2 | def'),
            ('\x1b x', 2, 3, '1 | ? x'),
        ]:
            path.write_bytes(text.encode('utf-8'))
            result = run(exe, str(path), str(start), str(end))
            check(result.returncode == 0 and expected in result.stdout and '\x1b' not in result.stdout, result.stdout)
        result = run(exe, str(path.parent / 'missing'), '0', '1')
        check(result.returncode == 0 and 'error: range' in result.stdout, result.stdout)
    result = run('./bin/voidc', 'parse', 'Tests/Diagnostics/Parser/Program.void')
    check(result.returncode == 1 and '| ' in result.stderr and '^' in result.stderr, result.stderr)
def source_check(source, command='check'):
    with tempfile.TemporaryDirectory() as temp:
        path = pathlib.Path(temp)
        (path / 'Program.void').write_text(source)
        return run('./bin/voidc', command, str(path))
if STAGE == 323:
    result = source_check('public static class Program { public static void Main() { int x = "text"; } }')
    check(result.returncode == 1 and 'cannot assign' in result.stderr, result.stderr)
    check('note: Local initialization requires an implicit conversion' in result.stderr, result.stderr)
    result = source_check('public interface I { void Work(); } public class C : I {} public static class Program { public static void Main() {} }')
    check(result.returncode == 1 and 'does not implement interface method' in result.stderr, result.stderr)
    check('note: Interface contract requires this member' in result.stderr and result.stderr.count('^') >= 2, result.stderr)
    result = source_check('public static class Program { public static void Main() { int x = 1; } }')
    check(result.returncode == 0 and not result.stderr, result.stderr)
if STAGE == 324:
    for source, command, expected in [
        ('public class C { void M() { int x = 1 } }', 'parse', "Insert ';'"),
        ('public class C { string x = "text; }', 'lex', 'Close the string'),
        ('public static class S {} public static class Program { public static void Main() { S x = new S(); } }', 'check', 'Access static members'),
        ('public static class Program { public static void M(in int x) { x = 2; } public static void Main() { int y = 1; M(in y); } }', 'check', 'Copy the value to a mutable local'),
    ]:
        result = source_check(source, command)
        check(result.returncode == 1 and 'help: ' + expected in result.stderr, result.stderr)
        check(result.stderr.count('error:') == 1, result.stderr)
    result = source_check('public static class Program { public static void Main() { Missing(); } }')
    check(result.returncode == 1 and 'help:' not in result.stderr, result.stderr)
    result = source_check('public static class Program { public static void Main() {} }')
    check(result.returncode == 0 and 'help:' not in result.stderr, result.stderr)
if STAGE == 325:
    with tempfile.TemporaryDirectory() as temp:
        exe = str(pathlib.Path(temp) / 'recovery')
        result = run('cc', '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2',
                     '-ICompiler/include', 'Tests/RichDiagnostics/recovery.c', 'Compiler/src/parser.c',
                     'Compiler/src/lexer.c', 'Compiler/src/ast.c', 'Compiler/src/diagnostic.c', '-o', exe)
        check(result.returncode == 0, result.stderr)
        check(run(exe).returncode == 0, 'declaration recovery lost valid following declaration')
    for source, expected in [
        ('class C { void M() { int x = 1 } } class Good {}', "expected ';'"),
        ('class C { void M() { F(1 2); } }', "expected ',' or ')'"),
        ('class C { void M() { int x = (1; } }', "expected ')'"),
        ('class C { void M() { int[] x = new int[2; } }', "expected ']'"),
        ('class C { void M() { }', "expected '}'"),
        ('class C { void M() { C x = new C { X = 1 Y = 2 }; } }', "expected ',' or '}'"),
        ('class C { int ; }', 'expected member name'),
        ('class C { void M() { F(,); } }', 'expected expression'),
        ('class C { string s = "unfinished; }', 'unterminated string literal'),
    ]:
        result = source_check(source, 'parse')
        check(result.returncode == 1 and expected in result.stderr and result.stderr.count('error:') == 1, result.stderr)
    result = source_check('public static class Program { public static void Main() { int x = 1 } }', 'build')
    check(result.returncode == 1 and 'built:' not in result.stdout, result.stdout)
    result = source_check('class C { void M(int x, int y) { F(x, y); } }', 'parse')
    check(result.returncode == 0, result.stderr)
if STAGE == 326:
    for source, error, note in [
        ('int x = 0; x = "long text";', 'cannot assign', 'Assignment requires'),
        ('int x = "long text";', 'cannot assign', 'Local initialization'),
        ('F("text");', 'no matching', 'Call supplies 1 argument(s)'),
        ('F();', 'no matching', 'Call supplies 0 argument(s)'),
        ('int x = R();', 'method returns', 'Return values must satisfy'),
        ('int x = 1; int y = x.Missing;', 'has no member', 'Member lookup uses the compile-time'),
        ('C.Secret();', 'is inaccessible', 'Member accessibility is checked'),
    ]:
        source = 'public class C { private static int Secret() { return 1; } } public static class Program { public static void F(int x) {} public static int R() { return "bad"; } public static void Main() { ' + source + ' } }'
        result = source_check(source)
        check(result.returncode == 1 and error in result.stderr, result.stderr)
        check('note: ' + note in result.stderr, result.stderr)
    result = source_check('public static class Program { public static void Main() { int x = "long text"; } }')
    check('^~~~~~~~~~~' in result.stderr, result.stderr)
    result = source_check('public static class Program { public static int F(int x) { return x; } public static void Main() { int x = F(1); x = 2; } }')
    check(result.returncode == 0, result.stderr)
if STAGE == 327:
    for source, expected in [
        ('class C {} class C {}', 'Previous type declaration'),
        ('class C { int X; int X; }', 'Previous field declaration'),
        ('public static class Program { public static void Main() { int x = 1; int x = 2; } }', 'Previous local or parameter'),
        ('public class B { public int M() { return 1; } } public class D : B { public override int M() { return 2; } }', 'Base member contract'),
        ('public interface I { int X { get; } } public class C : I {}', 'Required interface property'),
        ('public static class Program { public static int R() { return "bad"; } public static void Main() { int x = R(); } }', 'Declared return contract'),
        ('public static class Program { public static void F(int x) {} public static void F(string x, int y) {} public static void Main() { F(); } }', 'Overload candidate declares'),
    ]:
        result = source_check(source)
        check(result.returncode == 1 and 'note: ' + expected in result.stderr, result.stderr)
        check(result.stderr.count('^') >= 2, result.stderr)
    result = source_check('public static class Program { public static void F(int x) {} public static void F(string x, int y) {} public static void Main() { F(); } }')
    check(result.stderr.count('note: Overload candidate') == 2, result.stderr)
    result = source_check('public static class Program { public static void Main() { int x = "bad"; } }')
    check('Target local type is declared here' in result.stderr and 'Local initialization requires' in result.stderr, result.stderr)
    result = source_check('public static class Program { public static void F(int x) {} public static void Main() { F(1); } }')
    check(result.returncode == 0 and not result.stderr, result.stderr)
if STAGE == 328:
    cases = [
        ('`', 'lex', 'VOID1001'),
        ('class C { string x = "bad; }', 'lex', 'VOID1002'),
        ('class C { int x }', 'parse', 'VOID2001'),
        ('public static class Program { public static void Main() { int x = Missing; } }', 'check', 'VOID3001'),
        ('public static class Program { public static void Main() { int x = "bad"; } }', 'check', 'VOID3002'),
        ('public static class Program { public static void F(int x) {} public static void Main() { F(); } }', 'check', 'VOID3003'),
        ('class C {} class C {}', 'check', 'VOID3008'),
        ('public interface I { void M(); } public class C : I {}', 'check', 'VOID3007'),
        ('public static class S {} public static class Program { public static void Main() { S x = new S(); } }', 'check', 'VOID3010'),
    ]
    for source, command, code in cases:
        first = source_check(source, command)
        second = source_check(source, command)
        check(first.returncode == 1 and 'code: ' + code in first.stderr, first.stderr)
        check('code: ' + code in second.stderr, second.stderr)
    result = run('./bin/voidc', 'check', '/nonexistent/void/project')
    check(result.returncode == 1 and 'code: VOID4000' in result.stderr, result.stderr)
    import re
    catalog = (ROOT / 'Compiler/include/diagnostic_codes.def').read_text()
    codes = re.findall(r'"(VOID\d{4})"', catalog)
    check(len(codes) == len(set(codes)) == 20, codes)
if STAGE == 329:
    import json
    with tempfile.TemporaryDirectory() as temp:
        # Windows forbids quotes/control characters in filesystem names.
        # serialize.c below exercises them as diagnostic data on every host.
        name = 'é quote space' if os.name == 'nt' else 'é "quote" \\slash\nline'
        path = pathlib.Path(temp) / name
        path.mkdir()
        program = path / 'Program.void'
        program.write_text('public static class Program { public static void Main() { int x = "bad"; } }')
        result = run('./bin/voidc', 'check', str(path), '--diagnostics=json')
        data = json.loads(result.stderr)
        check(result.returncode == 1 and data['schemaVersion'] == 1, result.stderr)
        check(data['code'] == 'VOID3002' and data['severity'] == 'error', data)
        # The directory loader joins with '/', also accepted by Windows.
        check(pathlib.Path(data['file']) == program, data)
        check(data['span']['end']['offset'] > data['span']['start']['offset'], data)
        check(len(data['notes']) == 2 and len(data['relatedLocations']) == 1 and data['help'] == [], data)
        human = run('./bin/voidc', 'check', str(path), '--diagnostics=text')
        check(data['message'] in human.stderr and all(n['message'] in human.stderr for n in data['notes']), human.stderr)
        program.write_text('class C { void M() { int x = 1 } }')
        result = run('./bin/voidc', '--diagnostics=json', 'parse', str(program))
        data = json.loads(result.stderr)
        check(result.returncode == 1 and data['help'][0]['severity'] == 'help', data)
        check(data['span']['start'] == data['span']['end'], data)
        program.write_text('public static class Program { public static void Main() {} }')
        result = run('./bin/voidc', 'check', str(path), '--diagnostics=json')
        check(result.returncode == 0 and result.stderr == '', result.stderr)
    result = run('./bin/voidc', 'check', '/missing/é"\\\n', '--diagnostics=json')
    data = json.loads(result.stderr)
    check(result.returncode == 1 and data['code'] == 'VOID4000' and data['file'] is None, data)
    result = run('./bin/voidc', 'check', '--diagnostics=xml')
    check(result.returncode == 1 and "format must be 'text' or 'json'" in result.stderr, result.stderr)
    # Exercise every ASCII control byte and non-ASCII text through the serializer.
    with tempfile.TemporaryDirectory() as temp:
        exe = str(pathlib.Path(temp) / 'serialize')
        result = run('cc', '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2',
                     '-ICompiler/include', 'Tests/RichDiagnostics/serialize.c', 'Compiler/src/diagnostic.c', 'Compiler/src/lexer.c', '-o', exe)
        check(result.returncode == 0, result.stderr)
        result = run(exe)
        data = json.loads(result.stdout)
        check(data['message'] == ''.join(chr(i) for i in range(1, 32)) + '"\\é界\n', data)
        check(data['notes'][0]['message'] == 'multiline\ncontext\t"\\' and data['help'][0]['message'] == 'guidance', data)
        check(data['relatedLocations'][0]['file'] == 'path\n"\\é', data)
if STAGE == 330:
    import json
    import os
    # Cross-system coverage reuses authoritative, permanent negative projects.
    projects = [
        'AsyncAwaitSyntaxSemanticDiagnostics/AwaitOutsideAsync',
        'IteratorLocalDiagnostics/ReadBeforeAssignment',
        'ByReferenceValueRefLocalDiagnostics/TypeMismatch',
        'SpanFoundationDiagnostics/ReadOnlyWrite',
        'RefStructFoundationDiagnostics/AsyncLocal',
        'UnsafePointerPropertyDiagnostics/ManagedClassPointer',
        'NativeStringAbiContractMetadataFoundationDiagnostics/ByRefParameter',
        'DelegateCompletionDiagnostics/EqualityMismatch',
        'StaticEventDiagnostics/ExternalInvoke',
        'UnmanagedGenericConstraintDiagnostics/ManagedReference',
        'StaticInterfaceEventDiagnostics/Missing',
        'UnsafeDelegateDiagnostics/ManagedReturn',
        'Check/Parser',
        'Check/Semantic',
    ]
    for project in projects:
        target = 'Tests/' + project
        machine = run('./bin/voidc', 'check', target, '--diagnostics=json')
        check(machine.returncode == 1, machine.stdout + machine.stderr)
        data = json.loads(machine.stderr)
        check(data['code'].startswith('VOID') and data['file'] is not None and data['span']['start']['line'] > 0, data)
        human = run('./bin/voidc', 'check', target)
        check(human.returncode == 1 and data['message'] in human.stderr and 'code: ' + data['code'] in human.stderr and '^' in human.stderr, human.stderr)
    for source in [
        'public class Box<T> {} public static class Program { public static void Main() { Box<int, int> x; } }',
        'public static class Program { public static T F<T>(T x) { return x; } public static void Main() { F<int, int>(1); } }',
    ]:
        with tempfile.TemporaryDirectory() as temp:
            path = pathlib.Path(temp) / 'Program.void'
            path.write_text(source)
            result = run('./bin/voidc', 'check', str(path), '--diagnostics=json')
            data = json.loads(result.stderr)
            check(result.returncode == 1 and data['code'] == 'VOID3013' and pathlib.Path(data['file']) == path, data)
            check(data['notes'] and data['span']['end']['offset'] > data['span']['start']['offset'], data)
    with tempfile.TemporaryDirectory() as temp:
        path = pathlib.Path(temp)
        source = path / 'source.void'
        source.write_text('changed file contents\n')
        exe = str(path / 'snapshot')
        result = run('cc', '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2',
                     '-ICompiler/include', 'Tests/RichDiagnostics/snapshot.c', 'Compiler/src/diagnostic.c', 'Compiler/src/lexer.c', '-o', exe)
        check(result.returncode == 0, result.stderr)
        result = run(exe, str(source))
        text, machine = result.stdout.rsplit('\n', 2)[:2]
        check(result.returncode == 0 and 'original snapshot' in text and 'changed file' not in text and '\x1b' not in text and 'Additional diagnostic context was omitted' in text, text)
        data = json.loads(machine)
        check(data['contextTruncated'] is True and len(data['notes']) == 8 and data['message'] == 'ÿ', data)
        source.unlink()
        (path / 'Program.void').write_text('using Void; public static class Program { public static T Id<T>(T x) { return x; } public static void Main() { Console.WriteLine(Id<int>(42)); } }')
        for command in ['build', 'publish']:
            result = run('./bin/voidc', command, str(path), '--diagnostics=json')
            check(result.returncode == 0 and not result.stderr, result.stdout + result.stderr)
            output = result.stdout.strip().splitlines()[-1].split(': ', 1)[1]
            raw = subprocess.run([output], cwd=ROOT, capture_output=True)
            expected_bytes = b'42\r\n' if os.name == 'nt' else b'42\n'
            result = run(output)
            check(result.returncode == 0 and result.stdout == '42\n'
                  and raw.returncode == 0 and raw.stdout == expected_bytes,
                  (result.returncode, repr(result.stdout), result.stderr, raw.stdout, raw.stderr))
        # Exercise the shared runtime setup through normal Console, Environment,
        # and raw byte-array writes on both output streams. These internal
        # assertions retain the cumulative suite's existing guarded-check count.
        (path / 'Program.void').write_text('''using Void; using Void.IO;
public static class Program {
    public static void Main() {
        Console.WriteLine("Hello");
        Console.Write("Environment");
        Console.Write(Environment.NewLine);
        byte[] bytes = new byte[] { (byte)66, (byte)121, (byte)116, (byte)101, (byte)115, (byte)13, (byte)10 };
        Stream output = Console.OpenStandardOutput();
        output.Write(bytes, 0, bytes.Length);
        output.Flush();
        Console.Error.WriteLine("Error");
        Stream error = Console.OpenStandardError();
        error.Write(bytes, 0, bytes.Length);
        error.Flush();
    }
}''')
        newline = b'\r\n' if os.name == 'nt' else b'\n'
        for command in ['build', 'publish']:
            built = run('./bin/voidc', command, str(path), '--diagnostics=json')
            assert built.returncode == 0 and not built.stderr, (built.stdout, built.stderr)
            output = built.stdout.strip().splitlines()[-1].split(': ', 1)[1]
            raw = subprocess.run([output], cwd=ROOT, capture_output=True)
            assert raw.returncode == 0, (raw.returncode, raw.stdout, raw.stderr)
            assert raw.stdout == b'Hello' + newline + b'Environment' + newline + b'Bytes\r\n', raw.stdout
            assert raw.stderr == b'Error' + newline + b'Bytes\r\n', raw.stderr
        # The native helper also works without generated C and initializes once.
        exe = str(path / 'environment')
        built = run('cc', '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2',
                    '-IRuntime/include', 'Tests/RichDiagnostics/environment.c', '-o', exe)
        assert built.returncode == 0, built.stderr
        raw = subprocess.run([exe], cwd=ROOT, capture_output=True)
        assert raw.returncode == 0 and raw.stdout == raw.stderr == b'shared\r\n', (raw.returncode, raw.stdout, raw.stderr)
        env = dict(os.environ, CC='false')
        result = subprocess.run(['./bin/voidc', 'build', str(path), '--diagnostics=json'], cwd=ROOT, env=env, text=True, capture_output=True)
        check(result.returncode == 1 and json.loads(result.stderr)['code'] == 'VOID4000', result.stderr)
    result = run('./bin/voidc', 'version')
    check(result.returncode == 0 and result.stdout.strip() == 'voidc 0.0.360', result.stdout)
