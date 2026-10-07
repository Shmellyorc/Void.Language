"""Permanent #333 static callable receiver, generic method-group, diagnostic and LSP regressions."""
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / 'bin/voidc'
FIXTURE = ROOT / 'Tests/StaticCallableTypeReceivers'
count = 0


def expect(condition, detail):
    global count
    assert condition, detail
    count += 1
    print('True')


def run(*args):
    return subprocess.run([str(COMPILER), *map(str, args)], cwd=ROOT,
                          capture_output=True, text=True, timeout=90)


result = run('run', FIXTURE)
expect(result.returncode == 0, result.stderr)
expect([line for line in result.stdout.splitlines() if line in ('True', 'False')] == ['True'] * 25,
       result.stdout)
parsed = run('parse', FIXTURE / 'Program.void')
expect(parsed.returncode == 0, parsed.stderr)
for representation in ['MemberAccess .Utility', 'MemberAccess .MathLike',
                       'TypeReceiver Container<int>', 'TypeReceiver string']:
    expect(representation in parsed.stdout, (representation, parsed.stdout))

with tempfile.TemporaryDirectory(prefix='void333-') as temporary:
    source = Path(temporary) / 'Program.void'
    definitions = '''
public interface IMarker {}
public class Marker : IMarker {}
public class Utility {
    public static int Double(int value) { return value * 2; }
    public static int Pick(int value) { return value; }
    public static int Pick(long value) { return (int)value; }
    public static int Ambiguous(long value) { return (int)value; }
    public static int Ambiguous(float value) { return (int)value; }
    public static T Identity<T>(T value) { return value; }
    public static T Keep<T>(T value) where T : IMarker { return value; }
    public static T Uninferable<T, U>(T value) { return value; }
    public static int GroupUninferable<T>(int value) { return value; }
    public static int OnlyString(string value) { return value.Length; }
    private static int Hidden(int value) { return value; }
    public int Instance(int value) { return value; }
}
'''

    def diagnostic(statement, extra=definitions, message=None, code=None, token=None):
        text = extra + '\npublic static class Program { public static void Main() { ' + statement + '; } }'
        source.write_text(text)
        result = run('check', source, '--diagnostics=json')
        expect(result.returncode != 0, (statement, result.stdout, result.stderr))
        records = [json.loads(line) for line in result.stderr.splitlines() if line.startswith('{')]
        expect(len(records) == 1, records)
        record = records[0]
        expect(record['code'].startswith('VOID'), record)
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

    diagnostic('Utility.Missing(1)', message='no matching static method', code='VOID3003')
    diagnostic('Utility.Instance(1)', message='requires an instance receiver', code='VOID3003', token='Instance')
    diagnostic('Utility.Hidden(1)', message='inaccessible', code='VOID3005', token='Hidden')
    diagnostic('Utility.OnlyString(1)', message='no matching static method', code='VOID3003')
    diagnostic('Utility.Ambiguous(1)', message="call to static method 'Utility.Ambiguous' is ambiguous",
               code='VOID3003', token='Ambiguous')
    diagnostic('Func<string, int> fn = Utility.Double', message='no method matches delegate signature')
    diagnostic('Func<int, int> fn = Utility.Ambiguous', message='delegate method group is ambiguous')
    diagnostic('Func<int, int> fn = Utility.Hidden', message='inaccessible')
    diagnostic('Utility.Uninferable(1)', message='could not be inferred', code='VOID3003', token='Uninferable')
    diagnostic('Func<int, int> fn = Utility.GroupUninferable', message='could not be inferred')
    diagnostic('Utility.Identity<int, string>(1)', message='with 2 type argument(s) was not found', code='VOID3013')
    diagnostic('Utility.Keep<int>(1)', message="does not satisfy constraint 'IMarker'", code='VOID3006')
    diagnostic('Func<int, int> fn = Utility.Keep', message="does not satisfy constraint 'IMarker'", code='VOID3006')
    diagnostic('int.Something()', extra='', message="type 'int' has no static method 'Something'",
               code='VOID3012', token='Something')
    diagnostic('Utility.Identity<int(1)')

    # Generic-call disambiguation must not consume ordinary comparison syntax.
    comparison_source = definitions + '''
public static class Program { public static void Main() {
    int a = 1; int b = 2; int c = 3;
    bool less = a < b; bool greater = b > a;
    int identity = Utility.Identity<int>(c);
} }'''
    source.write_text(comparison_source)
    result = run('check', source)
    expect(result.returncode == 0, result.stderr)
    source.write_text(comparison_source.replace('int identity = Utility.Identity<int>(c);',
                                                'int shifted = 8 >> 1; int identity = Utility.Identity<int>(c);'))
    result = run('parse', source)
    expect(result.returncode == 0 and 'Binary <' in result.stdout and 'Binary >>' in result.stdout,
           result.stderr + result.stdout)

    # The value-first rule remains authoritative even when the local shadows a type name.
    source.write_text('''public class Thing { public static int Run(int x) { return x; } public int Read(int x) { return x; } }
public static class Program { public static void Main() { Thing Thing = new Thing(); int x = Thing.Read(1); } }''')
    expect(run('check', source).returncode == 0, 'value shadowing must stay a value receiver')

# LSP must use the same resolved receiver/method identities as compilation.
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
    request(2, 'completion', 'Alpha.Utility.Add(1, 2)', len('Alpha.Utility.')),
    request(3, 'hover', 'Alpha.Utility.Identity<int>(22)', len('Alpha.Utility.')),
    request(4, 'definition', 'Alpha.Utility.Identity<int>(22)', len('Alpha.Utility.')),
    request(5, 'signatureHelp', 'Alpha.Utility.Identity<int>(22)', len('Alpha.Utility.Identity<int>(')),
    request(6, 'signatureHelp', 'Alpha.Utility.Pick(1)', len('Alpha.Utility.Pick(')),
    request(7, 'references', 'Alpha.Utility.Identity<int>(22)', len('Alpha.Utility.'),
            context=dict(includeDeclaration=False)),
    request(8, 'definition', 'Alpha.Same.Resolve() == 41', len('Alpha.Same.')),
    request(9, 'definition', 'Beta.Same.Resolve() == 42', len('Beta.Same.')),
    request(10, 'hover', 'Alpha.Utility.Double;\n        Console.WriteLine(direct', len('Alpha.Utility.')),
    dict(jsonrpc='2.0', id=99, method='shutdown', params=None),
    dict(jsonrpc='2.0', method='exit', params=None),
]
frames = []
for message in messages:
    body = json.dumps(message).encode()
    frames.append(b'Content-Length: ' + str(len(body)).encode() + b'\r\n\r\n' + body)
result = subprocess.run([str(COMPILER), 'lsp'], input=b''.join(frames), capture_output=True,
                        cwd=ROOT, timeout=90)
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

labels = {item['label'] for item in responses[2]['items']}
expect({'Add', 'Double', 'Pick', 'Identity', 'Keep'} <= labels, labels)
expect('Instance' not in labels and 'Hidden' not in labels, labels)
expect('Identity' in json.dumps(responses[3]) and '__gm' not in json.dumps(responses[3]), responses[3])
expect(responses[4] is not None and responses[4]['uri'] == uri, responses[4])
identity_line = text.splitlines()[responses[4]['range']['start']['line']]
expect('Identity<T>' in identity_line, identity_line)
expect('Identity<T>(' in json.dumps(responses[5]), responses[5])
expect(json.dumps(responses[6]).count('Pick(') >= 2, responses[6])
expect(responses[7] and all(item['uri'] == uri for item in responses[7]), responses[7])
expect(responses[8] is not None and responses[9] is not None and
       responses[8]['range'] != responses[9]['range'], (responses[8], responses[9]))
expect('Double' in json.dumps(responses[10]), responses[10])

print(f'#333 Python checks: {count}')
