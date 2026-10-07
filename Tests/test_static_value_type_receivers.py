"""Permanent #332 static value receiver execution, diagnostics and tooling checks."""
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / 'bin/voidc'
FIXTURE = ROOT / 'Tests/StaticValueTypeReceivers'
count = 0


def expect(condition, detail):
    global count
    assert condition, detail
    count += 1
    print('True')


def run(*args):
    return subprocess.run([str(COMPILER), *map(str, args)], cwd=ROOT,
                          capture_output=True, text=True, timeout=60)


result = run('run', FIXTURE)
expect(result.returncode == 0, result.stderr)
expect([line for line in result.stdout.splitlines() if line in ('True', 'False')] == ['True'] * 43, result.stdout)
# Inspect real emitted storage: constants remain initializer expressions; static
# property access calls the normal generated accessor; managed fields use roots.
emitted = (FIXTURE / '.void/StaticValueTypeReceivers.c').read_text()
expect('vc_type_init_' in emitted, 'normal type initialization')
expect('vc_sr_' in emitted, 'normal managed static roots')

with tempfile.TemporaryDirectory(prefix='void332-') as temporary:
    source = Path(temporary) / 'Program.void'
    declaration = '''public class Config {
        public static int Field;
        public static readonly int Ready = 1;
        public const int Constant = 2;
        public static int Property { get; set; }
        public static int Restricted { private get; set; }
        public static int PrivateSetter { get; private set; }
        public static int ReadOnly => 3;
        private static int Secret;
        private const int Hidden = 4;
        private static int HiddenProperty => 5;
        public int Instance;
        public int InstanceProperty { get; set; }
    }'''

    def diagnostic(expression, definitions=declaration, message=None, code=None, token=None):
        text = definitions + '\npublic static class Program { public static void Main() { ' + expression + '; } }'
        source.write_text(text)
        result = run('check', source, '--diagnostics=json')
        expect(result.returncode != 0, expression)
        records = [json.loads(line) for line in result.stderr.splitlines()]
        expect(len(records) == 1, records)
        record = records[0]
        expect(record['code'].startswith('VOID3'), record)
        if message:
            expect(message in record['message'], record)
        if code:
            expect(record['code'] == code, record)
        start = record['span']['start']['offset']
        end = record['span']['end']['offset']
        expect(0 <= start < end <= len(text), record)
        if token:
            expect(text[start:end] == token, (text[start:end], record))

    for member, message in [('Ready', 'static readonly'), ('Constant', 'const field'),
                            ('ReadOnly', 'read-only')]:
        diagnostic('Config.' + member + ' = 9', message=message, token=member)
    diagnostic('Config.PrivateSetter = 9', message='setter', token='PrivateSetter')
    diagnostic('Config.Restricted', message='getter', code='VOID3005', token='Restricted')
    diagnostic('Config.Restricted += 9', message='getter', code='VOID3005', token='Restricted')
    for member in ['Secret', 'Hidden', 'HiddenProperty']:
        diagnostic('Config.' + member, message='inaccessible', token=member)
    for member in ['Field', 'Property']:
        diagnostic('Config.' + member + ' = "wrong"', message='cannot assign',
                   code='VOID3002', token='"wrong"')
    for member in ['Instance', 'InstanceProperty']:
        diagnostic('Config.' + member, message='instance member', code='VOID3012', token=member)
    diagnostic('Config.Missing', message='static member', code='VOID3012', token='Missing')
    diagnostic('Config value = new Config(); value.Field', message='has no field', token='Field')
    for builtin in ['bool', 'byte', 'sbyte', 'short', 'ushort', 'int', 'uint', 'long',
                    'ulong', 'float', 'double', 'decimal', 'char', 'string', 'object']:
        diagnostic(builtin + '.Missing', message='static member', token='Missing')
    diagnostic('Config.Field', 'namespace A { public class Config { public static int Field; } } '
               'namespace B { public class Config { public static int Field; } }',
               message='ambiguous', token='Config')
    diagnostic('Box<int, string>.Field', 'public class Box<T> { public static int Field; }',
               message='2 argument(s)', code='VOID3013', token='Box<int, string>')
    diagnostic('Box Box = new Box(); Box<int>.Field',
               'public class Box<T> { public static int Field; } public class Box {}',
               message="value 'Box'", code='VOID3013', token='Box')
    diagnostic('IContract.Value', 'public interface IContract { static int Value { get; set; } }',
               message='implementing type')
    diagnostic('Limits.Value', 'public readonly struct Limits { public static int Value { readonly get { return 1; } } }',
               message='instance value struct')
    diagnostic('Limits.Value', 'public readonly struct Limits { public int Value { get; set; } }',
               message='cannot declare a setter')
    diagnostic('Config.Value', 'public class Config { public static int Value { set {} } }',
               message='must declare a getter')
    diagnostic('int x = B.Config.Value', 'namespace A { public class Config { public static readonly int Value; } } '
               'namespace B { public class Config { public static int Value; static Config() { A.Config.Value = 9; } } }',
               message='declaring static constructor', token='Value')
    # Constant-only compile-time validation works through qualified receivers.
    source.write_text('namespace E { public class Config { public const int Value = 2; } } '
                      'public class Other { public const int Value = E.Config.Value + 1; } '
                      'public static class Program { public static void Main() { int[,] a = new int[Other.Value, 1] { {1}, {2}, {3} }; } }')
    expect(run('check', source).returncode == 0, 'constant array-size evaluation')
    source.write_text(source.read_text().replace('{ {1}, {2}, {3} }', '{ {1}, {2} }'))
    result = run('check', source, '--diagnostics=json')
    expect(result.returncode != 0 and 'is 3 but initializer requires 2' in result.stderr and 'VOID3000' in result.stderr, result.stderr)

# All navigation and completion requests consume the compiler's member bindings.
text = (FIXTURE / 'Program.void').read_text()
uri = (FIXTURE / 'Program.void').as_uri()


def position(needle, within=0):
    offset = text.index(needle) + within
    return {'line': text[:offset].count('\n'), 'character': offset - text.rfind('\n', 0, offset) - 1}


messages = [dict(jsonrpc='2.0', id=1, method='initialize', params={}),
            dict(jsonrpc='2.0', method='textDocument/didOpen', params=dict(
                textDocument=dict(uri=uri, languageId='void', version=1, text=text)))]
requests = []
for needle, within, member in [('Foo.Widget.Field == 2', len('Foo.Widget.'), 'Field'),
                              ('Foo.Widget.Folded == 8', len('Foo.Widget.'), 'Folded'),
                              ('Foo.Widget.Property = Next()', len('Foo.Widget.'), 'Property'),
                              ('Limits.Maximum == 100', len('Limits.'), 'Maximum'),
                              ('Bar.Widget.Field == 29', len('Bar.Widget.'), 'Field'),
                              ('Foo.Box<int>.Item = 109', len('Foo.Box<int>.'), 'Item')]:
    for method in ['hover', 'definition', 'references']:
        identifier = len(requests) + 2
        params = dict(textDocument=dict(uri=uri), position=position(needle, within))
        if method == 'references':
            params['context'] = dict(includeDeclaration=False)
        messages.append(dict(jsonrpc='2.0', id=identifier, method='textDocument/' + method, params=params))
        requests.append((identifier, method, member, needle))
for needle, within, expected, absent in [
    ('Foo.Widget.Field == 2', len('Foo.Widget.'), {'Field', 'Default', 'Folded', 'Ready', 'Property', 'ReadOnly'}, {'Instance', 'stored', 'HiddenProperty', 'Hidden'}),
    ('Limits.Maximum == 100', len('Limits.'), {'Maximum', 'Auto', 'Field', 'Constant', 'Ready'}, set()),
    ('Foo.Box<int>.Item = 109', len('Foo.Box<int>.'), {'Field', 'Constant', 'Item', 'Auto'}, set()),
    ('value.Instance == 23', len('value.'), {'Instance'}, {'Field', 'Property', 'Default'})]:
    identifier = len(requests) + 2
    messages.append(dict(jsonrpc='2.0', id=identifier, method='textDocument/completion',
                         params=dict(textDocument=dict(uri=uri), position=position(needle, within))))
    requests.append((identifier, 'completion', expected, absent))
messages += [dict(jsonrpc='2.0', id=999, method='shutdown', params=None),
             dict(jsonrpc='2.0', method='exit', params=None)]
frames = []
for message in messages:
    body = json.dumps(message).encode()
    frames.append(b'Content-Length: ' + str(len(body)).encode() + b'\r\n\r\n' + body)
result = subprocess.run([str(COMPILER), 'lsp'], input=b''.join(frames), capture_output=True, cwd=ROOT, timeout=60)
expect(result.returncode == 0 and not result.stderr, result.stderr)
remaining = result.stdout
responses = {}
while remaining:
    header, _, content = remaining.partition(b'\r\n\r\n')
    length = int(header.split(b': ')[1])
    message = json.loads(content[:length])
    if 'id' in message:
        expect('error' not in message, message)
        responses[message['id']] = message.get('result')
    remaining = content[length:]
for identifier, method, member, extra in requests:
    response = responses[identifier]
    if method == 'hover':
        expect(member in json.dumps(response) and 'int' in json.dumps(response), response)
    elif method == 'definition':
        expect(response is not None and response['uri'] == uri, response)
        line = text.splitlines()[response['range']['start']['line']]
        expect(member in line, (member, line))
    elif method == 'references':
        expect(response and all(item['uri'] == uri for item in response), response)
    else:
        labels = {item['label'] for item in response['items']}
        expect(member <= labels, (member, labels))
        expect(not (extra & labels), (extra, labels))
# Same short name must navigate to distinct declarations.
expect(responses[3]['range'] != responses[15]['range'], 'Foo/Bar member declaration identity')
print(f'#332 Python checks: {count}')
