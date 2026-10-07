"""Permanent #334 constructed-generic static identity, substitution, diagnostics and LSP regressions."""
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / 'bin/voidc'
FIXTURE = ROOT / 'Tests/ConstructedGenericStaticMembers'
GENERATED_C = FIXTURE / '.void/ConstructedGenericStaticMembers.c'
count = 0


def expect(condition, detail):
    global count
    assert condition, detail
    count += 1
    print('True')


def run(*args):
    return subprocess.run([str(COMPILER), *map(str, args)], cwd=ROOT,
                          capture_output=True, text=True, timeout=120)


# End-to-end behavior must prove storage, initialization, rooting and substituted calls.
result = run('run', FIXTURE)
expect(result.returncode == 0, result.stderr)
observed = [line for line in result.stdout.splitlines() if line in ('True', 'False')]
expect(observed == ['True'] * 30, result.stdout)

# Constructed receiver syntax must survive parsing rather than being flattened to an open declaration.
parsed = run('parse', FIXTURE / 'Program.void')
expect(parsed.returncode == 0, parsed.stderr)
for representation in [
        'TypeReceiver Counter334<int>',
        'TypeReceiver Counter334<string>',
        'TypeReceiver Box334<int>',
        'TypeReceiver Example.Generic334<Example.Arg334>',
        'TypeReceiver Foo334.Same334<int>',
        'TypeReceiver Bar334.Same334<int>',
        'TypeReceiver Recursive334<Box334<int>>']:
    expect(representation in parsed.stdout, (representation, parsed.stdout))

# Generated C must use canonical, collision-resistant closed identities and be deterministic.
built = run('build', FIXTURE)
expect(built.returncode == 0, built.stderr)
first_c = GENERATED_C.read_bytes()
first_hash = hashlib.sha256(first_c).hexdigest()
for spelling in [
        'Generic334__g1_Example__dArg334',
        'Counter334__g1_int',
        'Counter334__g1_string',
        'Counter334__g1_uint',
        'Counter334__g1_bool',
        'Counter334__g1_Point334',
        'Counter334__g1_Node334',
        'Recursive334__g1_Box334__g1_int',
        'Recursive334__g1_Box334__g1_string',
        'Recursive334__g1_Box334__g1_Example__dArg334']:
    expect(spelling.encode() in first_c, spelling)
expect(b'Generic334__g1_Arg334' not in first_c,
       'unqualified source spelling must not create an alternate closed specialization')
expect(b'"Foo334.Same334__g1_int"' in first_c and b'"Bar334.Same334__g1_int"' in first_c,
       'same short generic name in distinct namespaces must retain declaration identity')
second = run('build', FIXTURE)
expect(second.returncode == 0, second.stderr)
second_c = GENERATED_C.read_bytes()
expect(hashlib.sha256(second_c).hexdigest() == first_hash and second_c == first_c,
       'closed-generic generated C must be deterministic')

# Structured diagnostics must keep source-level generic spelling and enforce substituted semantics.
with tempfile.TemporaryDirectory(prefix='void334-') as temporary:
    source = Path(temporary) / 'Program.void'
    definitions = '''
using Void;
public sealed class Node334 { }
public class Box334<T> {
    public static T Value { get; set; }
    public static T Echo(T value) { return value; }
}
public class Mapper334<T> { public static T Echo(T value) { return value; } }
public class Picker334<T> {
    public static int Pick(T value) { return 1; }
    public static int Pick(string value) { return 2; }
}
public class StructOnly334<T> where T : struct { public static int Read() { return 7; } }
public class Private334<T> { private static int Hidden() { return 1; } }
'''

    def diagnostic(statement, message=None, code=None, token=None):
        text = definitions + '\npublic static class Program { public static void Main() { ' + statement + '; } }'
        source.write_text(text)
        result = run('check', source, '--diagnostics=json')
        expect(result.returncode != 0, (statement, result.stdout, result.stderr))
        records = [json.loads(line) for line in result.stderr.splitlines() if line.startswith('{')]
        expect(len(records) == 1, records)
        record = records[0]
        expect(record['code'].startswith('VOID'), record)
        expect('__g' not in record['message'] and '__gm' not in record['message'], record)
        if message is not None:
            expect(message in record['message'], record)
        if code is not None:
            expect(record['code'] == code, record)
        start = record['span']['start']['offset']
        end = record['span']['end']['offset']
        expect(0 <= start < end <= len(text), record)
        if token is not None:
            expect(text[start:end] == token, (text[start:end], record))
        return record

    diagnostic('Box334<int>.Echo("bad")', message="Box334<int>")
    diagnostic('Box334<int>.Value = "bad"', message='cannot assign')
    diagnostic('Func<string, string> fn = Mapper334<int>.Echo',
               message="Func<string, string>")
    diagnostic('Picker334<string>.Pick("x")', message="Picker334<string>.Pick")
    diagnostic('Private334<int>.Hidden()', message='inaccessible', token='Hidden')
    diagnostic('Box334<int>.Missing()', message="Box334<int>", token='Missing')
    diagnostic('Box334<int, string>.Echo(1)', message='with 2 argument(s)')
    diagnostic('StructOnly334<Node334>.Read()', message='non-nullable value type')

# LSP must expose the closed substituted source surface, not specialization internals.
text = (FIXTURE / 'Program.void').read_text()
uri = (FIXTURE / 'Program.void').as_uri()


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
    request(2, 'completion', 'Box334<int>.Current = 30', len('Box334<int>.')),
    request(3, 'hover', 'Box334<int>.Echo(50)', len('Box334<int>.')),
    request(4, 'signatureHelp', 'Box334<int>.Echo(50)', len('Box334<int>.Echo(')),
    request(5, 'signatureHelp', 'Box334<int>.Pair<string>(1, "pair")',
            len('Box334<int>.Pair<string>(')),
    request(6, 'definition', 'Generic334<Arg334>.Value = 90', len('Generic334<Arg334>.')),
    request(7, 'definition', 'Example.Generic334<Example.Arg334>.Value == 90',
            len('Example.Generic334<Example.Arg334>.')),
    request(8, 'references', 'Generic334<Arg334>.Value = 90', len('Generic334<Arg334>.'),
            context=dict(includeDeclaration=False)),
    request(9, 'hover', 'Mapper334<int>.Echo;', len('Mapper334<int>.')),
    request(10, 'definition', 'Foo334.Same334<int>.Value = 1', len('Foo334.Same334<int>.')),
    request(11, 'definition', 'Bar334.Same334<int>.Value = 2', len('Bar334.Same334<int>.')),
    dict(jsonrpc='2.0', id=99, method='shutdown', params=None),
    dict(jsonrpc='2.0', method='exit', params=None),
]
frames = []
for message in messages:
    body = json.dumps(message).encode()
    frames.append(b'Content-Length: ' + str(len(body)).encode() + b'\r\n\r\n' + body)
result = subprocess.run([str(COMPILER), 'lsp'], input=b''.join(frames), capture_output=True,
                        cwd=ROOT, timeout=120)
expect(result.returncode == 0 and not result.stderr, result.stderr)
remaining = result.stdout
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

serialized = json.dumps(responses)
expect('__g' not in serialized and '__gm' not in serialized, serialized)
completion_items = responses[2]['items']
completion = {item['label']: item for item in completion_items}
expect({'Current', 'Value', 'Echo', 'Pair'} <= set(completion), completion)
expect('int' in json.dumps(completion['Current']), completion['Current'])
expect('int' in json.dumps(completion['Value']), completion['Value'])
expect('Echo' in json.dumps(responses[3]) and 'int' in json.dumps(responses[3]), responses[3])
expect('Echo(int value): int' in json.dumps(responses[4]), responses[4])
expect('Pair<string>' in json.dumps(responses[5]) or 'string' in json.dumps(responses[5]), responses[5])
expect(responses[6] is not None and responses[7] is not None and
       responses[6]['range'] == responses[7]['range'], (responses[6], responses[7]))
expect(responses[8] and len(responses[8]) >= 1, responses[8])
expect('Echo' in json.dumps(responses[9]) and 'int' in json.dumps(responses[9]), responses[9])
expect(responses[10] is not None and responses[11] is not None and
       responses[10]['range'] != responses[11]['range'], (responses[10], responses[11]))

print(f'#334 Python checks: {count}')
