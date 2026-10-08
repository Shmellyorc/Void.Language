#!/usr/bin/env python3
"""#341 inline typed out-variable declarations: syntax, binding, flow, tooling and C."""
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / 'bin/voidc'
FIXTURE = ROOT / 'Tests/InlineTypedOutVariableDeclarations'
count = 0


def expect(condition, detail=''):
    global count
    assert condition, detail
    count += 1
    print('True')


def invoke(*args):
    return subprocess.run([str(COMPILER), *map(str, args)], cwd=ROOT,
                          capture_output=True, text=True, timeout=180)


# Positive/runtime coverage: primitives, reference/value/qualified/generic/array types,
# overloads, explicit+inferred generic methods, TryParse, scopes and legacy out forms.
result = invoke('run', FIXTURE)
expect(result.returncode == 0, result.stderr)
expect(not result.stderr, result.stderr)
runtime = [line for line in result.stdout.splitlines() if line in ('True', 'False')]
expect(len(runtime) == 30, result.stdout)
for index, line in enumerate(runtime):
    expect(line == 'True', (index, result.stdout))

# Parser keeps the declaration explicit in the out AST rather than source rewriting.
result = invoke('parse', FIXTURE)
expect(result.returncode == 0, result.stderr)
expect('Unary out declare int' in result.stdout, result.stdout)
expect('Unary out declare string' in result.stdout, result.stdout)
expect('Unary out declare Out341.Item' in result.stdout, result.stdout)
expect('Unary out declare Out341.Box<int>' in result.stdout, result.stdout)
expect('Unary out declare int[]' in result.stdout, result.stdout)

# Generated C is deterministic and independently accepted by the strict C11 gate.
generated = FIXTURE / '.void/InlineTypedOutVariableDeclarations.c'
first = generated.read_bytes()
result = invoke('build', FIXTURE)
expect(result.returncode == 0, result.stderr)
expect(not result.stderr, result.stderr)
expect(generated.read_bytes() == first, 'generated C changed across identical builds')
with tempfile.TemporaryDirectory(prefix='void341-strict-') as temporary:
    strict = subprocess.run(shlex.split(os.environ.get('CC', 'cc')) + [
        '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2',
        '-IRuntime/include', '-pthread', '-c', str(generated), '-o',
        str(Path(temporary) / 'inline-out.o')], cwd=ROOT,
        capture_output=True, text=True, timeout=180)
    expect(strict.returncode == 0, strict.stderr)
    expect(not strict.stderr, strict.stderr)


def diagnostic(source_text, json_mode=True):
    with tempfile.TemporaryDirectory(prefix='void341-diag-') as temporary:
        source = Path(temporary) / 'Program.void'
        source.write_text(source_text)
        args = ['check', source]
        if json_mode:
            args.append('--diagnostics=json')
        return invoke(*args)


cases = [
    # Inline storage must use exact ordinary out compatibility.
    ('public static class Program { static void Need(out long value) { value = 1l; } '
     'public static void Main() { Need(out int value); } }', 'VOID3003'),
    # Existing predeclared out storage follows the same corrected exact rule.
    ('public static class Program { static void Need(out long value) { value = 1l; } '
     'public static void Main() { int value; Need(out value); } }', 'VOID3003'),
    # Duplicate locals use the ordinary local declaration rule.
    ('public static class Program { static void Need(out int value) { value = 1; } '
     'public static void Main() { int value = 0; Need(out int value); } }', 'VOID3008'),
    # Missing variable name is a parser error with the declaration token span.
    ('public static class Program { static void Need(out int value) { value = 1; } '
     'public static void Main() { Need(out int); } }', 'VOID2001'),
    # Unknown explicit type is a semantic local-type failure.
    ('public static class Program { static void Main() { Missing(out MissingType value); } }', 'VOID3000'),
    # An inline out declaration cannot bind to an ordinary value parameter.
    ('public static class Program { static void Need(int value) { } '
     'public static void Main() { Need(out int value); } }', 'VOID3003'),
]
for source_text, code_prefix in cases:
    result = diagnostic(source_text)
    expect(result.returncode != 0, source_text)
    records = [json.loads(line) for line in result.stderr.splitlines() if line.startswith('{')]
    expect(len(records) == 1, result.stderr)
    expect(records[0]['code'].startswith(code_prefix), records[0])
    expect(records[0].get('span') is not None, records[0])
    expect('__g' not in records[0]['message'], records[0])

# #342 now accepts out var; keep the inherited #341 suite compatible while preserving
# its assertion count. Typed inline declarations remain the primary #341 coverage above.
result = diagnostic('public static class Program { static void Need(out int value) { value = 1; } '
                    'public static void Main() { Need(out var value); } }')
expect(result.returncode == 0, result.stderr)
records = [json.loads(line) for line in result.stderr.splitlines() if line.startswith('{')]
expect(len(records) == 0, result.stderr)
expect('VOID project resolved' in result.stdout, result.stdout)

# Invalid call binding must remain a truthful call diagnostic rather than committing a fake local.
result = diagnostic('public static class Program { public static void Main() { Missing(out int value); } }')
expect(result.returncode != 0, result.stderr)
records = [json.loads(line) for line in result.stderr.splitlines() if line.startswith('{')]
expect(len(records) == 1, result.stderr)
expect(records[0]['code'].startswith('VOID3'), records[0])
expect('Missing' in records[0]['message'], records[0])

# Human-readable diagnostics retain the locked rich source-facing shape.
result = diagnostic('public static class Program { static void Need(out int value) { value = 1; } '
                    'public static void Main() { int value = 0; Need(out int value); } }', False)
expect(result.returncode != 0, result.stderr)
expect('Program.void:' in result.stderr, result.stderr)
expect('code: VOID3008' in result.stderr, result.stderr)
expect('| ' in result.stderr and '^' in result.stderr, result.stderr)
expect('already declared in this scope' in result.stderr, result.stderr)

# ref/in parsing and binding remain untouched while ordinary valid out continues to work.
legacy = diagnostic('public static class Program { '
    'static void Set(out int value) { value = 9; } '
    'static void Ref(ref int value) { value = value + 1; } '
    'static int Read(in int value) { return value; } '
    'public static void Main() { int value; Set(out value); Ref(ref value); '
    'int read = Read(in value); Console.WriteLine(read == 10); } }')
expect(legacy.returncode == 0, legacy.stderr)

# LSP/semantic queries consume the same real local symbol created by compilation.
source = FIXTURE / 'Program.void'
text = source.read_text()
uri = source.resolve().as_uri()


def position(needle, within):
    offset = text.index(needle) + within
    line_start = text.rfind('\n', 0, offset) + 1
    return {'line': text[:offset].count('\n'), 'character': offset - line_start}


def request(identifier, method, pos, **extra):
    return {'jsonrpc': '2.0', 'id': identifier, 'method': 'textDocument/' + method,
            'params': {'textDocument': {'uri': uri}, 'position': pos, **extra}}


declaration = position('out int integer', len('out int '))
use = position('Check(integer == 17)', len('Check('))
messages = [
    {'jsonrpc': '2.0', 'id': 1, 'method': 'initialize', 'params': {}},
    {'jsonrpc': '2.0', 'method': 'textDocument/didOpen', 'params': {
        'textDocument': {'uri': uri, 'languageId': 'void', 'version': 1, 'text': text}}},
    request(2, 'hover', declaration),
    request(3, 'hover', use),
    request(4, 'definition', use),
    request(5, 'references', use, context={'includeDeclaration': True}),
    request(6, 'completion', use),
    {'jsonrpc': '2.0', 'id': 99, 'method': 'shutdown', 'params': None},
    {'jsonrpc': '2.0', 'method': 'exit', 'params': None},
]
frames = []
for message in messages:
    body = json.dumps(message).encode()
    frames.append(b'Content-Length: ' + str(len(body)).encode() + b'\r\n\r\n' + body)
process = subprocess.run([str(COMPILER), 'lsp'], cwd=ROOT, input=b''.join(frames),
                         capture_output=True, timeout=180)
expect(process.returncode == 0, process.stderr)
expect(not process.stderr, process.stderr)
remaining = process.stdout
responses = {}
notifications = []
while remaining:
    header, separator, content = remaining.partition(b'\r\n\r\n')
    expect(bool(separator), remaining)
    length = int(header.split(b': ')[1])
    message = json.loads(content[:length])
    remaining = content[length:]
    if 'id' in message:
        responses[message['id']] = message
    else:
        notifications.append(message)
for identifier in range(1, 7):
    expect(identifier in responses and 'error' not in responses[identifier], responses)
expect(responses[2]['result']['contents']['value'] == 'local integer: int', responses[2])
expect(responses[3]['result']['contents']['value'] == 'local integer: int', responses[3])
expect(responses[4]['result']['uri'] == uri, responses[4])
expect(responses[4]['result']['range']['start'] == declaration, responses[4])
reference_ranges = {(entry['range']['start']['line'], entry['range']['start']['character'])
                    for entry in responses[5]['result']}
expect((declaration['line'], declaration['character']) in reference_ranges, responses[5])
expect((use['line'], use['character']) in reference_ranges, responses[5])
completion = {item['label']: item for item in responses[6]['result']['items']}
expect('integer' in completion, completion)
expect(completion['integer'].get('detail') == 'int', completion['integer'])
expect(all(not note['params']['diagnostics'] for note in notifications
           if note.get('method') == 'textDocument/publishDiagnostics'), notifications)

print(f'#341 focused assertions: {count}')
