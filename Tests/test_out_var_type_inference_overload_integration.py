#!/usr/bin/env python3
"""#342 out var inference, overload/generic integration, tooling and strict C."""
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / 'bin/voidc'
FIXTURE = ROOT / 'Tests/OutVarTypeInferenceOverloadIntegration'
count = 0


def expect(condition, detail=''):
    global count
    assert condition, detail
    count += 1
    print('True')


def invoke(*args):
    env = dict(os.environ)
    env.setdefault('TERM', 'xterm')
    return subprocess.run([str(COMPILER), *map(str, args)], cwd=ROOT,
                          capture_output=True, text=True, timeout=180, env=env)


# Positive/runtime coverage: concrete inference across primitive, user, generic, array,
# parsing, overload, generic-specialization, scope and locked legacy forms.
result = invoke('run', FIXTURE)
expect(result.returncode == 0, result.stderr)
expect(not result.stderr, result.stderr)
runtime = [line for line in result.stdout.splitlines() if line in ('True', 'False')]
expect(len(runtime) == 42, result.stdout)
for index, line in enumerate(runtime):
    expect(line == 'True', (index, result.stdout))

# The parser extends #341's single declaration-bearing unary out node.
result = invoke('parse', FIXTURE)
expect(result.returncode == 0, result.stderr)
expect(result.stdout.count('Unary out declare var') >= 30, result.stdout)
expect('Unary out declare int' in result.stdout, result.stdout)
expect('Identifier ordinaryVar' in result.stdout, result.stdout)

# Generated C is deterministic and independently accepted by the strict C11 gate.
generated = FIXTURE / '.void/OutVarTypeInferenceOverloadIntegration.c'
first = generated.read_bytes()
result = invoke('build', FIXTURE)
expect(result.returncode == 0, result.stderr)
expect(not result.stderr, result.stderr)
expect(generated.read_bytes() == first, 'generated C changed across identical builds')
with tempfile.TemporaryDirectory(prefix='void342-strict-') as temporary:
    strict = subprocess.run(shlex.split(os.environ.get('CC', 'cc')) + [
        '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2',
        '-IRuntime/include', '-pthread', '-c', str(generated), '-o',
        str(Path(temporary) / 'out-var.o')], cwd=ROOT,
        capture_output=True, text=True, timeout=180)
    expect(strict.returncode == 0, strict.stderr)
    expect(not strict.stderr, strict.stderr)


def diagnostic(source_text, json_mode=True):
    with tempfile.TemporaryDirectory(prefix='void342-diag-') as temporary:
        source = Path(temporary) / 'Program.void'
        source.write_text(source_text)
        args = ['check', source]
        if json_mode:
            args.append('--diagnostics=json')
        return invoke(*args)


def one_record(result):
    records = [json.loads(line) for line in result.stderr.splitlines()
               if line.startswith('{')]
    expect(result.returncode != 0, result.stderr)
    expect(len(records) == 1, result.stderr)
    return records[0]


# out var alone never selects between otherwise equal out-only overloads.
record = one_record(diagnostic(
    'public static class Program { '
    'static void Get(out int value) { value = 1; } '
    'static void Get(out string value) { value = "x"; } '
    'public static void Main() { Get(out var value); } }'))
expect(record['code'].startswith('VOID3003'), record)
expect('ambiguous' in record['message'] and 'var' not in record['message'].lower(), record)
expect(record.get('span') is not None, record)

# A generic parameter appearing only in out T cannot be inferred circularly.
record = one_record(diagnostic(
    'public static class Program { '
    'static void Create<T>(out T value) { value = default(T); } '
    'public static void Main() { Create(out var value); } }'))
expect(record['code'].startswith('VOID3003'), record)
expect('could not be inferred from the call arguments' in record['message'], record)
expect('var' not in record['message'].lower(), record)

# out var is not legal against an ordinary by-value parameter.
record = one_record(diagnostic(
    'public static class Program { static void Use(int value) { } '
    'public static void Main() { Use(out var value); } }'))
expect(record['code'].startswith('VOID3003'), record)
expect('no matching' in record['message'], record)

# Redeclaration uses the ordinary lexical-local rule after the winning call is known.
record = one_record(diagnostic(
    'public static class Program { static void Need(out int value) { value = 1; } '
    'public static void Main() { int value = 0; Need(out var value); } }'))
expect(record['code'].startswith('VOID3008'), record)
expect('already declared in this scope' in record['message'], record)

# Failed call binding remains the real diagnostic; no fake/untyped local leaks out.
record = one_record(diagnostic(
    'public static class Program { public static void Main() { '
    'Missing(out var value); Console.WriteLine(value); } }'))
expect(record['code'].startswith('VOID3003'), record)
expect('Missing' in record['message'], record)
expect('<unknown>' not in record['message'] and '__g' not in record['message'], record)

# Malformed inferred declarations are parser diagnostics with truthful spans.
record = one_record(diagnostic(
    'public static class Program { static void Need(out int value) { value = 1; } '
    'public static void Main() { Need(out var); } }'))
expect(record['code'].startswith('VOID2'), record)
expect('out variable name' in record['message'], record)
expect(record.get('span') is not None, record)

record = one_record(diagnostic(
    'public static class Program { static void Need(out int value) { value = 1; } '
    'public static void Main() { Need(out var value extra); } }'))
expect(record['code'].startswith('VOID2'), record)
expect("expected ',' or ')'" in record['message'], record)

# Speculative candidates may disagree on result type; the winning candidate alone
# determines the local, proved by using it in int arithmetic afterward.
rollback = diagnostic(
    'public static class Program { '
    'static void Pick(int key, out int value) { value = key; } '
    'static void Pick(long key, out string value) { value = "bad"; } '
    'public static void Main() { Pick(1, out var value); int next = value + 1; '
    'Console.WriteLine(next); } }')
expect(rollback.returncode == 0, rollback.stderr)
expect(not rollback.stderr, rollback.stderr)

# #341 typed inline out, predeclared out, ordinary var, ref and in remain unchanged.
legacy = diagnostic(
    'public static class Program { '
    'static void Set(out int value) { value = 9; } '
    'static void Ref(ref int value) { value = value + 1; } '
    'static int Read(in int value) { return value; } '
    'public static void Main() { Set(out int typed); int predeclared; Set(out predeclared); '
    'var inferred = 0; Set(out inferred); Ref(ref inferred); int read = Read(in inferred); '
    'Console.WriteLine(typed + predeclared + read); } }')
expect(legacy.returncode == 0, legacy.stderr)
expect(not legacy.stderr, legacy.stderr)

# Human-readable failures retain the locked source-facing rich diagnostic shape.
result = diagnostic(
    'public static class Program { static void Need(out int value) { value = 1; } '
    'public static void Main() { int value = 0; Need(out var value); } }', False)
expect(result.returncode != 0, result.stderr)
expect('Program.void:' in result.stderr, result.stderr)
expect('code: VOID3008' in result.stderr, result.stderr)
expect('| ' in result.stderr and '^' in result.stderr, result.stderr)

# LSP/semantic queries consume the same inferred local and its finalized semantic type.
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


primitive_decl = position('out var intValue', len('out var '))
primitive_use = position('Check(intValue == 17)', len('Check('))
box_decl = position('out var inferredBox', len('out var '))
box_use = position('Check(inferredBox.Value', len('Check('))
signature_pos = position('out var overloadedInt', len('out var '))
messages = [
    {'jsonrpc': '2.0', 'id': 1, 'method': 'initialize', 'params': {}},
    {'jsonrpc': '2.0', 'method': 'textDocument/didOpen', 'params': {
        'textDocument': {'uri': uri, 'languageId': 'void', 'version': 1, 'text': text}}},
    request(2, 'hover', primitive_decl),
    request(3, 'hover', primitive_use),
    request(4, 'hover', box_decl),
    request(5, 'hover', box_use),
    request(6, 'definition', box_use),
    request(7, 'references', box_use, context={'includeDeclaration': True}),
    request(8, 'completion', box_use),
    request(9, 'signatureHelp', signature_pos),
    {'jsonrpc': '2.0', 'id': 99, 'method': 'shutdown', 'params': None},
    {'jsonrpc': '2.0', 'method': 'exit', 'params': None},
]
frames = []
for message in messages:
    body = json.dumps(message).encode()
    frames.append(b'Content-Length: ' + str(len(body)).encode() + b'\r\n\r\n' + body)
env = dict(os.environ)
env.setdefault('TERM', 'xterm')
process = subprocess.run([str(COMPILER), 'lsp'], cwd=ROOT, input=b''.join(frames),
                         capture_output=True, timeout=180, env=env)
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
for identifier in range(1, 10):
    expect(identifier in responses and 'error' not in responses[identifier], responses)
expect(responses[2]['result']['contents']['value'] == 'local intValue: int', responses[2])
expect(responses[3]['result']['contents']['value'] == 'local intValue: int', responses[3])
expect(responses[4]['result']['contents']['value'] == 'local inferredBox: Box<int>', responses[4])
expect(responses[5]['result']['contents']['value'] == 'local inferredBox: Box<int>', responses[5])
expect('__g' not in responses[4]['result']['contents']['value'], responses[4])
expect(responses[6]['result']['uri'] == uri, responses[6])
expect(responses[6]['result']['range']['start'] == box_decl, responses[6])
reference_ranges = {(entry['range']['start']['line'], entry['range']['start']['character'])
                    for entry in responses[7]['result']}
expect((box_decl['line'], box_decl['character']) in reference_ranges, responses[7])
expect((box_use['line'], box_use['character']) in reference_ranges, responses[7])
completion = {item['label']: item for item in responses[8]['result']['items']}
expect(completion['intValue'].get('detail') == 'int', completion.get('intValue'))
expect(completion['inferredBox'].get('detail') == 'Box<int>', completion.get('inferredBox'))
expect('__g' not in completion['inferredBox'].get('detail', ''), completion['inferredBox'])
signatures = responses[9]['result']['signatures']
expect(any('Get(int kind, out int value): void' == item['label'] for item in signatures), signatures)
expect(responses[9]['result']['activeParameter'] == 1, responses[9])
expect(all(not note['params']['diagnostics'] for note in notifications
           if note.get('method') == 'textDocument/publishDiagnostics'), notifications)

print(f'#342 focused assertions: {count}')
