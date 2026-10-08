#!/usr/bin/env python3
"""#343 inline out scope/flow completion plus context-specific out discard support."""
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / 'bin/voidc'
FIXTURE = ROOT / 'Tests/OutVariableScopeFlowDiscardCompletion'
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


# Real application-shaped runtime coverage: explicit/inferred declarations, &&, !,
# branches, shadowing, multiple declarations, while/for/do-while, break/continue,
# discard, generic/overload discard, and locked predeclared/ordinary-var forms.
result = invoke('run', FIXTURE)
expect(result.returncode == 0, result.stderr)
expect(not result.stderr, result.stderr)
runtime = [line for line in result.stdout.splitlines() if line in ('True', 'False')]
expect(len(runtime) == 20, result.stdout)
for index, line in enumerate(runtime):
    expect(line == 'True', (index, result.stdout))

# #343 extends the existing declaration-bearing unary out architecture. Discards
# are explicit AST state, not fake identifiers/locals or textual rewriting.
result = invoke('parse', FIXTURE)
expect(result.returncode == 0, result.stderr)
expect(result.stdout.count('Unary out declare var') >= 8, result.stdout)
expect(result.stdout.count('Unary out discard') >= 6, result.stdout)
expect('Unary out declare int' in result.stdout, result.stdout)
expect('Unary out declare bool' in result.stdout, result.stdout)

# Generated C remains deterministic and passes the strict native C gate.
generated = FIXTURE / '.void/OutVariableScopeFlowDiscardCompletion.c'
first = generated.read_bytes()
result = invoke('build', FIXTURE)
expect(result.returncode == 0, result.stderr)
expect(not result.stderr, result.stderr)
expect(generated.read_bytes() == first, 'generated C changed across identical builds')
with tempfile.TemporaryDirectory(prefix='void343-strict-') as temporary:
    strict = subprocess.run(shlex.split(os.environ.get('CC', 'cc')) + [
        '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-O2',
        '-IRuntime/include', '-pthread', '-c', str(generated), '-o',
        str(Path(temporary) / 'scope-flow-discard.o')], cwd=ROOT,
        capture_output=True, text=True, timeout=180)
    expect(strict.returncode == 0, strict.stderr)
    expect(not strict.stderr, strict.stderr)


def diagnostic(source_text, json_mode=True):
    with tempfile.TemporaryDirectory(prefix='void343-diag-') as temporary:
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


# && propagates the LHS out assignment into the RHS and both assignments into
# the true branch. This is shared boolean-flow analysis, not Try-method logic.
success = diagnostic(
    'public static class Program { '
    'static bool A(out int a){a=1;return true;} '
    'static bool B(int a,out int b){b=a+1;return true;} '
    'static void Use(int x){} '
    'public static void Main(){ if(A(out var a)&&B(a,out var b)){Use(a);Use(b);} } }')
expect(success.returncode == 0, success.stderr)
expect(not success.stderr, success.stderr)

# || does not claim the RHS out variable is assigned when the LHS can skip it.
record = one_record(diagnostic(
    'public static class Program { '
    'static bool A(out int x){x=1;return true;} '
    'static bool B(out int x){x=2;return true;} static void Use(int x){} '
    'public static void Main(){ if(A(out var a)||B(out var b)){Use(b);} } }'))
expect(record['code'].startswith('VOID3000'), record)
expect("local 'b' cannot be read before it is definitely assigned" in record['message'], record)
expect(record.get('span') is not None, record)

# The same short-circuit fact remains truthful after a general expression.
record = one_record(diagnostic(
    'public static class Program { '
    'static bool A(out int x){x=1;return true;} '
    'static bool B(out int x){x=2;return true;} static void Use(int x){} '
    'public static void Main(){ bool ok=A(out var a)||B(out var b); Use(b); } }'))
expect(record['code'].startswith('VOID3000'), record)
expect('definitely assigned' in record['message'], record)

# Negated nested short-circuit flow plus a terminating branch proves both values
# assigned on the only path that continues.
success = diagnostic(
    'public static class Program { '
    'static bool A(out int a){a=1;return true;} '
    'static bool B(int a,out int b){b=a+1;return true;} static void Use(int x){} '
    'public static void Main(){ if(!(A(out var a)&&B(a,out var b))) return; Use(a);Use(b); } }')
expect(success.returncode == 0, success.stderr)
expect(not success.stderr, success.stderr)

# Loop-condition declarations follow loop lexical scope and do not leak after it.
for keyword, source in [
    ('while', 'while(A(out var x)){} Console.WriteLine(x);'),
    ('do-while', 'do{}while(A(out var x)); Console.WriteLine(x);'),
    ('for', 'for(;A(out var x);){} Console.WriteLine(x);'),
]:
    record = one_record(diagnostic(
        'public static class Program { static bool A(out int x){x=1;return false;} '
        f'public static void Main(){{ {source} }} }}'))
    expect(record['code'].startswith('VOID3001'), (keyword, record))
    expect("unknown identifier 'x'" in record['message'], (keyword, record))

# Multiple declaration and collision rules are exactly ordinary local rules.
record = one_record(diagnostic(
    'public static class Program { static void Pair(out int a,out int b){a=1;b=2;} '
    'public static void Main(){Pair(out var x,out var x);} }'))
expect(record['code'].startswith('VOID3008'), record)
expect('already declared in this scope' in record['message'], record)
expect(record.get('relatedLocations'), record)

record = one_record(diagnostic(
    'public static class Program { static void P(out int x){x=1;} '
    'public static void Main(){int x=0;P(out var x);} }'))
expect(record['code'].startswith('VOID3008'), record)
expect('already declared in this scope' in record['message'], record)

# VOID's ordinary method-body scope permits a local to shadow a parameter; inline out
# declarations follow that same established policy rather than inventing a stricter rule.
success = diagnostic(
    'public static class Program { static void P(out int x){x=1;} '
    'static void M(int x){P(out var x);Console.WriteLine(x);} public static void Main(){M(0);} }')
expect(success.returncode == 0, success.stderr)
expect(not success.stderr, success.stderr)

# Nested shadowing stays legal because ordinary VOID locals allow it at a deeper scope.
success = diagnostic(
    'public static class Program { static void P(out int x){x=2;} '
    'public static void Main(){int x=1;{P(out var x);Console.WriteLine(x);}Console.WriteLine(x);} }')
expect(success.returncode == 0, success.stderr)
expect(not success.stderr, success.stderr)

# Discard is context-specific out syntax: no user local is created and later '_'
# does not resolve unless an ordinary local named '_' is separately declared.
record = one_record(diagnostic(
    'public static class Program { static void P(out int x){x=1;} '
    'public static void Main(){P(out _);Console.WriteLine(_);} }'))
expect(record['code'].startswith('VOID3001'), record)
expect("unknown identifier '_'" in record['message'], record)

# Discard supplies no overload-selection type information.
record = one_record(diagnostic(
    'public static class Program { static void Get(out int x){x=1;} '
    'static void Get(out string x){x="x";} public static void Main(){Get(out _);} }'))
expect(record['code'].startswith('VOID3003'), record)
expect('ambiguous' in record['message'], record)
expect('_unknown' not in record['message'] and '__g' not in record['message'], record)

# Other arguments can select an overload; discard then consumes the winning out type.
success = diagnostic(
    'public static class Program { static void Get(int k,out int x){x=k;} '
    'static void Get(string k,out string x){x=k;} '
    'public static void Main(){Get(1,out _);Get("x",out _);} }')
expect(success.returncode == 0, success.stderr)
expect(not success.stderr, success.stderr)

# Generic type parameters cannot be inferred circularly from discard.
record = one_record(diagnostic(
    'public static class Program { static void Create<T>(out T x){x=default(T);} '
    'public static void Main(){Create(out _);} }'))
expect(record['code'].startswith('VOID3003'), record)
expect('could not be inferred from the call arguments' in record['message'], record)

# Explicit generic arguments or another ordinary argument may resolve the type first.
success = diagnostic(
    'public static class Program { static void Create<T>(out T x){x=default(T);} '
    'static void Transform<T>(T input,out T value){value=input;} '
    'public static void Main(){Create<int>(out _);Transform(5,out _);} }')
expect(success.returncode == 0, success.stderr)
expect(not success.stderr, success.stderr)

# Discard remains invalid against a non-out parameter and malformed syntax stays parser-facing.
record = one_record(diagnostic(
    'public static class Program { static void Use(int x){} public static void Main(){Use(out _);} }'))
expect(record['code'].startswith('VOID3003'), record)
expect('no matching' in record['message'], record)

record = one_record(diagnostic(
    'public static class Program { static void P(out int x){x=1;} '
    'public static void Main(){P(out _ extra);} }'))
expect(record['code'].startswith('VOID2'), record)
expect("expected ',' or ')'" in record['message'], record)
expect(record.get('span') is not None, record)

# Rejected overload candidates do not commit candidate-specific locals or flow facts.
success = diagnostic(
    'public static class Program { '
    'static void Pick(int key,out int a,out string b){a=key;b="ok";} '
    'static void Pick(long key,out string a,out int b){a="bad";b=0;} '
    'public static void Main(){Pick(1,out var a,out var b);int n=a+1;Console.WriteLine(b);Console.WriteLine(n);} }')
expect(success.returncode == 0, success.stderr)
expect(not success.stderr, success.stderr)

# #341, #342, ordinary predeclared out, ordinary var, ref and in remain unchanged.
success = diagnostic(
    'public static class Program { static void Set(out int value){value=9;} '
    'static void Ref(ref int value){value=value+1;} static int Read(in int value){return value;} '
    'public static void Main(){Set(out int typed);Set(out var inferred);int old;Set(out old);'
    'var ordinary=0;Set(out ordinary);Ref(ref ordinary);int read=Read(in ordinary);'
    'Console.WriteLine(typed+inferred+old+read);} }')
expect(success.returncode == 0, success.stderr)
expect(not success.stderr, success.stderr)

# Human diagnostics keep the locked source excerpt/code/span presentation.
result = diagnostic(
    'public static class Program { static bool A(out int x){x=1;return true;} '
    'static bool B(out int x){x=2;return true;} static void Use(int x){} '
    'public static void Main(){if(A(out var a)||B(out var b)){Use(b);}} }', False)
expect(result.returncode != 0, result.stderr)
expect('Program.void:' in result.stderr, result.stderr)
expect('code: VOID3000' in result.stderr, result.stderr)
expect('| ' in result.stderr and '^' in result.stderr, result.stderr)

# LSP/semantic queries consume the same compiler locals. Loop locals are visible
# inside their lexical scope, hidden afterward, and discard never becomes a symbol.
source = FIXTURE / 'Program.void'
text = source.read_text()
uri = source.resolve().as_uri()


def position(needle, within, occurrence=0):
    start = 0
    found = -1
    for _ in range(occurrence + 1):
        found = text.index(needle, start)
        start = found + 1
    offset = found + within
    line_start = text.rfind('\n', 0, offset) + 1
    return {'line': text[:offset].count('\n'), 'character': offset - line_start}


def request(identifier, method, pos, **extra):
    return {'jsonrpc': '2.0', 'id': identifier, 'method': 'textDocument/' + method,
            'params': {'textDocument': {'uri': uri}, 'position': pos, **extra}}


and_decl = position('out var andB', len('out var '))
and_use = position('Check(andB == "seven")', len('Check('))
while_decl = position('out var whileValue', len('out var '))
while_use = position('whileSum = whileSum + whileValue', len('whileSum = whileSum + '))
while_after = position('Check(whileSum == 5)', len('Check('))
for_use = position('forSum = forSum + forValue', len('forSum = forSum + '))
discard_pos = position('Produce(out _);', len('Produce(out '), 0)
ordinary_underscore = position('Check(_ == 9)', len('Check('))
signature_pos = position('Pick(1, out _);', len('Pick(1, out '))

messages = [
    {'jsonrpc': '2.0', 'id': 1, 'method': 'initialize', 'params': {}},
    {'jsonrpc': '2.0', 'method': 'textDocument/didOpen', 'params': {
        'textDocument': {'uri': uri, 'languageId': 'void', 'version': 1, 'text': text}}},
    request(2, 'hover', and_decl),
    request(3, 'hover', and_use),
    request(4, 'definition', and_use),
    request(5, 'references', and_use, context={'includeDeclaration': True}),
    request(6, 'hover', while_use),
    request(7, 'completion', while_use),
    request(8, 'completion', while_after),
    request(9, 'completion', for_use),
    request(10, 'hover', discard_pos),
    request(11, 'definition', discard_pos),
    request(12, 'references', discard_pos, context={'includeDeclaration': True}),
    request(13, 'completion', discard_pos),
    request(14, 'hover', ordinary_underscore),
    request(15, 'signatureHelp', signature_pos),
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
for identifier in range(1, 16):
    expect(identifier in responses and 'error' not in responses[identifier], responses)

expect(responses[2]['result']['contents']['value'] == 'local andB: string', responses[2])
expect(responses[3]['result']['contents']['value'] == 'local andB: string', responses[3])
expect(responses[4]['result']['uri'] == uri, responses[4])
expect(responses[4]['result']['range']['start'] == and_decl, responses[4])
reference_ranges = {(entry['range']['start']['line'], entry['range']['start']['character'])
                    for entry in responses[5]['result']}
expect((and_decl['line'], and_decl['character']) in reference_ranges, responses[5])
expect((and_use['line'], and_use['character']) in reference_ranges, responses[5])
expect(responses[6]['result']['contents']['value'] == 'local whileValue: int', responses[6])
inside_while = {item['label']: item for item in responses[7]['result']['items']}
expect(inside_while['whileValue'].get('detail') == 'int', inside_while.get('whileValue'))
after_while = {item['label']: item for item in responses[8]['result']['items']}
expect('whileValue' not in after_while, after_while.get('whileValue'))
inside_for = {item['label']: item for item in responses[9]['result']['items']}
expect(inside_for['forValue'].get('detail') == 'int', inside_for.get('forValue'))
expect(responses[10]['result'] is None, responses[10])
expect(responses[11]['result'] is None, responses[11])
expect(responses[12]['result'] == [], responses[12])
discard_completion = {item['label']: item for item in responses[13]['result']['items']}
expect('_' not in discard_completion, discard_completion.get('_'))
expect(responses[14]['result']['contents']['value'] == 'local _: int', responses[14])
signatures = responses[15]['result']['signatures']
expect(any('Pick(int kind, out int value): void' == item['label'] for item in signatures), signatures)
expect(responses[15]['result']['activeParameter'] == 1, responses[15])
expect(all(not note['params']['diagnostics'] for note in notifications
           if note.get('method') == 'textDocument/publishDiagnostics'), notifications)

print(f'#343 focused assertions: {count}')
