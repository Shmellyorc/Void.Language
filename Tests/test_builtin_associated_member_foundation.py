#!/usr/bin/env python3
"""Permanent #335 built-in associated-member ownership/binding/tooling regressions."""
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / 'bin/voidc'
FIXTURE = ROOT / 'Tests/BuiltInAssociatedMemberFoundation'
GENERATED_C = FIXTURE / '.void/BuiltInAssociatedMemberFoundation.c'
count = 0


def expect(condition, detail):
    global count
    assert condition, detail
    count += 1
    print('True')


def run(*args):
    return subprocess.run([str(COMPILER), *map(str, args)], cwd=ROOT,
                          capture_output=True, text=True, timeout=120)


# Existing string surfaces, method groups, ordinary statics, and #334 closed generics.
result = run('run', FIXTURE)
expect(result.returncode == 0, result.stderr)
observed = [line for line in result.stdout.splitlines() if line in ('True', 'False')]
expect(observed == ['True'] * 18, result.stdout)

# Every authoritative built-in keyword remains a first-class semantic type receiver.
builtins = ['bool', 'byte', 'sbyte', 'short', 'ushort', 'int', 'uint', 'long', 'ulong',
            'float', 'double', 'decimal', 'char', 'string', 'object']
with tempfile.TemporaryDirectory(prefix='void335-parse-') as temporary:
    source = Path(temporary) / 'Program.void'
    calls = ' '.join(f'{name}.Missing335();' for name in builtins)
    source.write_text('public static class Program { public static void Main() { ' + calls + ' } }')
    parsed = run('parse', source)
    expect(parsed.returncode == 0, parsed.stderr)
    for name in builtins:
        expect(f'TypeReceiver {name}' in parsed.stdout, (name, parsed.stdout))

# Missing built-in members are semantic member/call diagnostics, never parser/identifier failures.
with tempfile.TemporaryDirectory(prefix='void335-diag-') as temporary:
    source = Path(temporary) / 'Program.void'

    def diagnostic(statement):
        text = 'using Void; public static class Program { public static void Main() { ' + statement + '; } }'
        source.write_text(text)
        result = run('check', source, '--diagnostics=json')
        expect(result.returncode != 0, (statement, result.stdout, result.stderr))
        records = [json.loads(line) for line in result.stderr.splitlines() if line.startswith('{')]
        expect(len(records) == 1, records)
        record = records[0]
        expect(record['code'].startswith('VOID3'), record)
        expect('AssociatedMembers' not in record['message'] and 'Void.Internal' not in record['message'], record)
        return record

    for name in ('int', 'bool', 'double'):
        record = diagnostic(f'{name}.DoesNotExist335()')
        expect(f"type '{name}'" in record['message'], record)
        expect('static method' in record['message'], record)

    # The same int semantic type remains an ordinary runtime value when used as a value receiver.
    value_record = diagnostic('int value335 = 1; value335.DoesNotExist335()')
    expect('instance' in value_record['message'], value_record)
    expect('static method' not in value_record['message'], value_record)

    # Parsing remains a later API. #336 now legitimately populates int.MaxValue
    # through the same #335 associated-member owner rather than changing the binder.
    parse_record = diagnostic('int.DoesNotExist("1")')
    expect("type 'int'" in parse_record['message'] and 'DoesNotExist' in parse_record['message'], parse_record)
    source.write_text('using Void; public static class Program { public static void Main() { Console.WriteLine(int.MaxValue == 2147483647); } }')
    max_check = run('check', source)
    expect(max_check.returncode == 0, (max_check.stdout, max_check.stderr))
    expect('checked: Program' in max_check.stdout, max_check.stdout)
    max_run = run('run', source.parent)
    expect(max_run.returncode == 0, (max_run.stdout, max_run.stderr))
    expect(max_run.stdout.splitlines()[-1:] == ['True'], max_run.stdout)
    expect('AssociatedMembers' not in max_run.stdout + max_run.stderr, max_run.stdout + max_run.stderr)

# Generated C keeps existing primitive/string representations and #334 closed identities.
built = run('build', FIXTURE)
expect(built.returncode == 0, built.stderr)
generated = GENERATED_C.read_bytes()
for spelling in [b'Box335__g1_int', b'Box335__g1_string', b'Box335__g1_bool', b'Box335__g1_float']:
    expect(spelling in generated, spelling)
expect(b'"BuiltInAssociated"' not in generated,
       'compiler-only built-in association metadata must not become runtime attribute metadata')

# LSP must consume the same string associated declaration/member bindings as compilation.
text = (FIXTURE / 'Program.void').read_text()
uri = (FIXTURE / 'Program.void').resolve().as_uri()


def position(needle, within=0):
    offset = text.index(needle) + within
    return {'line': text[:offset].count('\n'),
            'character': offset - text.rfind('\n', 0, offset) - 1}


def request(identifier, method, needle, within=0, **extra):
    params = dict(textDocument=dict(uri=uri), position=position(needle, within), **extra)
    return dict(jsonrpc='2.0', id=identifier, method='textDocument/' + method, params=params)


messages = [
    dict(jsonrpc='2.0', id=1, method='initialize', params={}),
    dict(jsonrpc='2.0', method='textDocument/didOpen', params=dict(
        textDocument=dict(uri=uri, languageId='void', version=1, text=text))),
    request(2, 'completion', 'string.Empty ==', len('string.')),
    request(3, 'hover', 'string.Concat("VO"', len('string.')),
    request(4, 'signatureHelp', 'string.Concat("VO"', len('string.Concat(')),
    request(5, 'definition', 'string.Concat("VO"', len('string.')),
    request(6, 'definition', 'string.Empty ==', len('string.')),
    request(7, 'completion', 'int value335 = 7', 0),
    dict(jsonrpc='2.0', id=99, method='shutdown', params=None),
    dict(jsonrpc='2.0', method='exit', params=None),
]
frames = []
for message in messages:
    body = json.dumps(message).encode()
    frames.append(b'Content-Length: ' + str(len(body)).encode() + b'\r\n\r\n' + body)
lsp = subprocess.run([str(COMPILER), 'lsp'], input=b''.join(frames), capture_output=True,
                     cwd=ROOT, timeout=120)
expect(lsp.returncode == 0 and not lsp.stderr, lsp.stderr)
remaining = lsp.stdout
responses = {}
while remaining:
    header, separator, content = remaining.partition(b'\r\n\r\n')
    expect(bool(separator), remaining[:200])
    length = int(header.split(b': ')[1])
    message = json.loads(content[:length])
    if 'id' in message:
        expect('error' not in message, message)
        responses[message['id']] = message.get('result')
    remaining = content[length:]

completion_items = responses[2]['items']
labels = {item['label'] for item in completion_items}
expect({'Empty', 'Concat', 'Equals', 'CompareOrdinal'} <= labels, completion_items)
expect(not any('AssociatedMembers' in item['label'] for item in completion_items), completion_items)
expect('Concat(string left, string right): string' in json.dumps(responses[3]), responses[3])
expect('Concat(string left, string right): string' in json.dumps(responses[4]), responses[4])
expect(responses[5] is not None and 'BuiltInAssociatedMembers.void' in responses[5]['uri'], responses[5])
expect(responses[6] is not None and 'BuiltInAssociatedMembers.void' in responses[6]['uri'], responses[6])
# Global completion must hide compiler-associated owner class symbols.
global_labels = {item['label'] for item in responses[7]['items']}
expect(not any(name.endswith('AssociatedMembers') for name in global_labels), sorted(global_labels))

print(f'#335 Python checks: {count}')
