#!/usr/bin/env python3
"""#340 cross-milestone audit: realistic projects, strict C, diagnostics and LSP."""
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / 'bin/voidc'
FIXTURES = {
    'TypeQualifiedStaticMembersBuiltinsIntegrationAudit': 39,
    'StaticGenericDeclarationScopeAudit': 11,
    'StaticPrimitiveParsingIntegrationAudit': 34,
}
count = 0


def expect(condition, detail):
    global count
    assert condition, detail
    count += 1
    print('True')


def invoke(*args):
    return subprocess.run([str(COMPILER), *map(str, args)], cwd=ROOT,
                          capture_output=True, text=True, timeout=180)


with tempfile.TemporaryDirectory(prefix='void340-audit-') as temporary:
    temporary = Path(temporary)
    for name, runtime_count in FIXTURES.items():
        fixture = ROOT / 'Tests' / name
        result = invoke('run', fixture)
        expect(result.returncode == 0, result.stderr)
        expect(not result.stderr, result.stderr)
        lines = [line for line in result.stdout.splitlines() if line in ('True', 'False')]
        expect(len(lines) == runtime_count, (name, len(lines), result.stdout))
        for index, line in enumerate(lines):
            expect(line == 'True', (name, index, result.stdout))
        generated = fixture / '.void' / (name + '.c')
        first = generated.read_bytes()
        result = invoke('build', fixture)
        expect(result.returncode == 0, result.stderr)
        expect(generated.read_bytes() == first, name + ' nondeterministic C')
        # The ordinary build need not pass compiler warning flags through. Audit
        # representative generated output explicitly with the full strict gate.
        strict = subprocess.run(shlex.split(os.environ.get('CC', 'cc')) + [
            '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2',
            '-IRuntime/include', '-pthread', '-c', str(generated), '-o',
            str(temporary / (name + '.o'))], cwd=ROOT, capture_output=True, text=True, timeout=180)
        expect(strict.returncode == 0, strict.stderr)
        expect(not strict.stderr, strict.stderr)

    locale_binary = temporary / ('locale-audit.exe' if os.name == 'nt' else 'locale-audit')
    native = ROOT / 'Tests/TypeQualifiedStaticMembersBuiltinsIntegrationAudit/locale_main.c'
    built = subprocess.run(shlex.split(os.environ.get('CC', 'cc')) + [
        '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2',
        '-IRuntime/include', '-pthread', str(native),
        *map(str, sorted((ROOT / 'Runtime/src').glob('*.c'))), '-lm', '-o', str(locale_binary)],
        cwd=ROOT, capture_output=True, text=True, timeout=180)
    expect(built.returncode == 0, built.stderr)
    result = subprocess.run([str(locale_binary)], capture_output=True, text=True, timeout=180)
    expect(result.returncode in (0, 77), result.stderr)
    expect(result.returncode == 77 or result.stdout.splitlines() == ['True'] * 39, result.stdout)
    if result.returncode == 77:
        print('#340 ambient comma locale unavailable: execution explicitly skipped')
    else:
        print('#340 ambient comma locale execution and preservation: PASS')

    # Failures must be ordinary structured VOID diagnostics, never backend errors
    # or internal associated/specialization identities.
    source = temporary / 'Program.void'
    cases = [
        'int.Parse(1)', 'int.TryParse("1")', 'float.Parse()',
        'EqualityComparer<int>.Default = null', 'Range.All = ..',
        'Index.End = Index.Start', 'int.MaxValue = 1',
        'AuditBox<int>.Echo("wrong")', 'AuditBox<int>.Instance()',
        'AuditBox<AuditBox<int>>.Value = "wrong"',
        'AuditBox<AuditBox<int>[]> .Field = "wrong"',
        'AuditBox<int> local = "wrong"',
        'var value = BadInit<AuditBox<int>>.Field',
        "char value = 'ab'", "char value = '\\uD800'",
    ]
    for statement in cases:
        source.write_text('using Void; using Void.Collections; '
                          'public class AuditBox<T> { public static T Value { get; set; } public static T Field; '
                          'public static T Echo(T x) { return x; } '
                          'public int Instance() { return 1; } } '
                          'public class BadInit<T> { public static T Field = "wrong"; } '
                          'public static class Program { public static void Main() { ' + statement + '; } }')
        result = invoke('check', source, '--diagnostics=json')
        expect(result.returncode != 0, statement)
        records = [json.loads(line) for line in result.stderr.splitlines() if line.startswith('{')]
        expect(len(records) == 1, (statement, result.stderr))
        record = records[0]
        expect(record['code'].startswith('VOID3'), record)
        expect(all(token not in record['message'] for token in (
            '__g', 'AssociatedMembers', 'PrimitiveParsing', 'invariant_numeric')), record)
        expect(record.get('span') is not None, record)

# Historical generic contract/storage diagnostics exercise the same display path.
for path in (
        'Tests/StaticInterfaceOperatorContractDiagnostics/Missing',
        'Tests/SpanFoundationDiagnostics/GenericArgument',
        'Tests/UnmanagedSpanConstructionDiagnostics/SafeConstructor'):
    result = invoke('check', ROOT / path, '--diagnostics=json')
    expect(result.returncode != 0, path)
    records = [json.loads(line) for line in result.stderr.splitlines() if line.startswith('{')]
    expect(len(records) == 1, (path, result.stderr))
    record = records[0]
    expect(record['code'].startswith('VOID3'), record)
    expect('__g' not in record['message'], record)
    expect(record.get('span') is not None, record)

# Query the very same program that compiled and executed above.
fixture = ROOT / 'Tests/TypeQualifiedStaticMembersBuiltinsIntegrationAudit'
source = fixture / 'Program.void'
text = source.read_text()
uri = source.resolve().as_uri()


def request(identifier, method, needle, within, **extra):
    offset = text.index(needle) + within
    params = dict(textDocument=dict(uri=uri), position=dict(
        line=text[:offset].count('\n'), character=offset - text.rfind('\n', 0, offset) - 1), **extra)
    return dict(jsonrpc='2.0', id=identifier, method='textDocument/' + method, params=params)


nested = 'Audit340.Cache<Audit340.Cache<int>>.Echo(closed)'
array = 'Audit340.Cache<Audit340.Cache<int>[]>.Echo(closedArray)'
queries = [
    (2, 'completion', 'int.TryParse(join', len('int.')),
    (3, 'signatureHelp', 'int.TryParse(join', len('int.TryParse(join("4", "2"), ')),
    (4, 'definition', 'int.TryParse(join', len('int.')),
    (5, 'hover', nested, nested.index('Echo')),
    (6, 'signatureHelp', nested, nested.index('Echo(') + 5),
    (7, 'signatureHelp', array, array.index('Echo(') + 5),
    (8, 'completion', 'EqualityComparer<Audit340.Item>.Default', len('EqualityComparer<Audit340.Item>.')),
    (9, 'definition', 'EqualityComparer<Audit340.Item>.Default', len('EqualityComparer<Audit340.Item>.')),
    (10, 'completion', 'Range.All.Equals', len('Range.')),
    (11, 'definition', nested, nested.index('Echo')),
    (14, 'signatureHelp', 'new Audit340.Cache<int>()', len('new Audit340.Cache<int>(')),
    (15, 'hover', 'new Audit340.Cache<int>()', 0),
    (12, 'hover', 'Audit340.Cache<Audit340_Item>.Initialized = 9', len('Audit340.Cache<Audit340_Item>.')),
]
messages = [dict(jsonrpc='2.0', id=1, method='initialize', params={}),
            dict(jsonrpc='2.0', method='textDocument/didOpen', params=dict(
                textDocument=dict(uri=uri, languageId='void', version=1, text=text)))]
messages += [request(*query) for query in queries]
messages += [request(13, 'references', nested, nested.index('Echo'), context={'includeDeclaration': True}),
             dict(jsonrpc='2.0', id=99, method='shutdown', params=None),
             dict(jsonrpc='2.0', method='exit', params=None)]
frames = []
for message in messages:
    body = json.dumps(message).encode()
    frames.append(b'Content-Length: ' + str(len(body)).encode() + b'\r\n\r\n' + body)
result = subprocess.run([str(COMPILER), 'lsp'], input=b''.join(frames), cwd=ROOT,
                        capture_output=True, timeout=180)
expect(result.returncode == 0 and not result.stderr, result.stderr)
remaining = result.stdout
responses = {}
notifications = []
while remaining:
    header, separator, content = remaining.partition(b'\r\n\r\n')
    assert separator, remaining
    length = int(header.split(b': ')[1])
    message = json.loads(content[:length])
    if 'id' not in message:
        notifications.append(message)
    if 'id' in message:
        responses[message['id']] = message
    remaining = content[length:]
for identifier in range(1, 16):
    expect(identifier in responses and 'error' not in responses[identifier], responses)
    if identifier != 1:
        expect(responses[identifier].get('result') is not None, (responses[identifier], notifications))
    expect('__g' not in json.dumps(responses[identifier]), responses[identifier])
expect(all(not n['params']['diagnostics'] for n in notifications if n.get('method') == 'textDocument/publishDiagnostics'), notifications)
labels = {item['label'] for item in responses[2]['result']['items']}
expect({'MinValue', 'MaxValue', 'Parse', 'TryParse'} <= labels, labels)
expect('out int result' in json.dumps(responses[3]), responses[3])
expect(responses[4]['result']['uri'].endswith('/BuiltInAssociatedMembers.void'), responses[4])
expect('Cache<int>' in json.dumps(responses[5]), responses[5])
expect('Cache<int> value' in json.dumps(responses[6]), responses[6])
expect('Cache<int>[] value' in json.dumps(responses[7]), responses[7])
expect(any(item['label'] == 'Default' for item in responses[8]['result']['items']), responses[8])
expect(responses[9]['result']['uri'].endswith('/EqualityComparer.void'), responses[9])
expect(any(item['label'] == 'All' for item in responses[10]['result']['items']), responses[10])
expect(responses[11]['result']['uri'] == uri, responses[11])
expect(len(responses[13]['result']) >= 3, responses[13])

# Architecture audit assertions retain the intended shared authorities.
compiler = ''.join((ROOT / ('Compiler/src/' + name)).read_text()
                   for name in ('compiler.c', 'semantic.c', 'monomorph.c'))
for api in ('"Parse"', '"TryParse"', 'EqualityComparer', 'Range.All', 'Index.Start', 'Index.End'):
    expect(api not in compiler, 'API-specific compiler path: ' + api)
parsing = (ROOT / 'StandardLibrary/Void/Internal/PrimitiveParsing.void').read_text()
expect('catch' not in parsing, 'TryParse uses exceptions')
expect('overflow ? Overflow : Success' in parsing, 'integer full-consumption range policy')
expect('setlocale(' not in compiler, 'process-global locale mutation')
for native in ('_strtof_l', '_strtod_l', '_strtold_l', 'strtof_l', 'strtod_l', 'strtold_l',
               '_create_locale', '_free_locale', 'newlocale', 'freelocale'):
    expect(native in compiler, native)
expect('vc_ast_character_scalar' in compiler, 'shared scalar literal primitive')
expect('clone_canonical_arguments' in compiler, 'canonical argument transfer')
print(f'#340 integrated audit: {count} checks')
