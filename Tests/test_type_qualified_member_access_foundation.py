"""Permanent #331 syntax, static-member, diagnostics and LSP regressions."""
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / 'bin/voidc'
FIXTURE = ROOT / 'Tests/TypeQualifiedMemberAccessFoundation'
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
expect(result.stdout.splitlines().count('True') == 31, result.stdout)
parsed = run('parse', FIXTURE / 'Program.void')
for representation in ['TypeReceiver Box<int>', 'TypeReceiver Box<Box<int>>',
                       'TypeReceiver Example.Container<int>', 'TypeReceiver string']:
    expect(representation in parsed.stdout, parsed.stdout)

with tempfile.TemporaryDirectory(prefix='void331-') as temporary:
    directory = Path(temporary)
    source = directory / 'Program.void'
    prefix = 'public static class Program { public static void Main() { '
    suffix = '; } }'

    def diagnostic(expression, declaration='', message=None, code=None, token=None):
        text = declaration + '\n' + prefix + expression + suffix
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
        return record

    for name in ['bool', 'byte', 'sbyte', 'short', 'ushort', 'int', 'uint', 'long',
                 'ulong', 'float', 'double', 'decimal', 'char', 'string', 'object']:
        diagnostic(name + '.Missing', message='static member', token='Missing')
    diagnostic('int.DoesNotExist("42")', message="no static method 'DoesNotExist'", code='VOID3012', token='DoesNotExist')
    diagnostic('Missing.Member', message='unknown', code='VOID3001', token='Missing')
    diagnostic('Missing<int>.Member', message='generic type', code='VOID3013', token='Missing<int>')
    diagnostic('Box<int, int>.Number', 'public class Box<T> { public static int Number; }',
               message='2 argument(s)', code='VOID3013', token='Box<int, int>')
    diagnostic('Widget.Number', 'namespace A { public class Widget { public static int Number; } } '
               'namespace B { public class Widget { public static int Number; } }', message='ambiguous', token='Widget')
    diagnostic('Widget.Instance', 'public class Widget { public int Instance; }',
               message='instance member', code='VOID3012', token='Instance')
    diagnostic('C.Widget.Number', 'namespace C { public class Marker {} } '
               'namespace A { public class Widget { public static int Number; } } '
               'namespace B { public class Widget { public static int Number; } }',
               message='unknown type receiver', code='VOID3001', token='Widget')
    diagnostic('E.Missing.Number', 'namespace E { public class Widget {} }',
               message='unknown type receiver', code='VOID3001', token='Missing')
    diagnostic('E.Missing.Call()', 'namespace E { public class Widget {} }',
               message='unknown type receiver', code='VOID3001', token='Missing')
    diagnostic('Widget.Secret', 'public class Widget { private static int Secret; }',
               message='inaccessible', token='Secret')
    diagnostic('Widget?.Number', 'public class Widget { public static int Number; }',
               message='instance receiver', token='Number')
    diagnostic('IFactory.Create(1)', 'public interface IFactory { static int Create(int x); }',
               message='implementing type')
    # Primitive type names are not value expressions, and malformed types fail structurally.
    source.write_text(prefix + 'int x = int' + suffix)
    result = run('check', source, '--diagnostics=json')
    expect(result.returncode != 0, result.stdout)
    expect('type receiver' in result.stderr, result.stderr)
    for expression in ['Box<int', 'Box<int>.', 'int.', 'Box<,>.Number']:
        source.write_text(prefix + expression + suffix)
        result = run('check', source, '--diagnostics=json')
        expect(result.returncode != 0, expression)
        expect(all(json.loads(line)['code'].startswith('VOID') for line in result.stderr.splitlines()), result.stderr)
    diagnostic('Box Box = new Box(); Box<int>.Number',
               'public class Box<T> { public static int Number; } public class Box {}',
               message="value 'Box'", code='VOID3013', token='Box')
    source.write_text(prefix + 'int a = 1; int b = 2; int c = 3; bool x = a < b; bool y = b > a; int z = 8 >> 1; bool q = a < b >> c' + suffix)
    result = run('parse', source)
    expect(result.returncode == 0, result.stderr)
    expect('Binary <' in result.stdout and 'Binary >>' in result.stdout and 'TypeReceiver' not in result.stdout, result.stdout)
    source.write_text('public class Box<T> { public static int Number; }\n' + prefix +
                      'Box<int>.Number = 17; int x = Box<string>.Number' + suffix)
    expect(run('check', source).returncode == 0, 'distinct generic arguments')
    source.write_text('namespace E { public class Box<T> { public static int Number; } }\n' + prefix +
                      'E.Box<int>.Number = 17; int x = Box<int>.Number' + suffix)
    expect(run('check', source).returncode == 0, 'qualified and unqualified generic identity')

# LSP consumes the same bindings for receiver symbols and static members.
text = (FIXTURE / 'Program.void').read_text()
uri = (FIXTURE / 'Program.void').as_uri()

def position(needle, within=0):
    offset = text.index(needle) + within
    return {'line': text[:offset].count('\n'), 'character': offset - text.rfind('\n', 0, offset) - 1}


def request(identifier, method, needle, within=0, **extra):
    return dict(jsonrpc='2.0', id=identifier, method='textDocument/' + method,
                params=dict(textDocument=dict(uri=uri), position=position(needle, within), **extra))


messages = [dict(jsonrpc='2.0', id=1, method='initialize', params={}),
    dict(jsonrpc='2.0', method='textDocument/didOpen', params=dict(textDocument=dict(uri=uri, languageId='void', version=1, text=text))),
    request(2, 'hover', 'Example.Widget.Method(9)', len('Example.Widget.')),
    request(3, 'definition', 'Example.Widget.Method(9)', len('Example.Widget.')),
    request(4, 'signatureHelp', 'Example.Widget.Method(9)', len('Example.Widget.Method(')),
    request(5, 'hover', 'Example.Container<int>.Echo', len('Example.')),
    request(6, 'definition', 'Example.Container<int>.Echo', len('Example.')),
    request(7, 'hover', 'string.Empty'),
    request(8, 'references', 'Example.Widget.Method(9)', len('Example.Widget.'), context=dict(includeDeclaration=False)),
    request(9, 'completion', 'Example.Widget.Method(9)', len('Example.Widget.')),
    request(11, 'completion', 'Example.Container<int>.Echo', len('Example.Container<int>.')),
    dict(jsonrpc='2.0', id=10, method='shutdown', params=None),
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
        responses[message['id']] = message.get('result')
    remaining = content[length:]
expect('Method' in json.dumps(responses[2]), responses)
expect(responses[3] is not None and responses[3]['uri'] == uri, responses)
expect('Method(int value)' in json.dumps(responses[4]), responses)
expect('Container' in json.dumps(responses[5]), responses)
expect(responses[6] is not None and responses[6]['uri'] == uri, responses)
expect('string' in json.dumps(responses[7]), responses)
expect(len(responses[8]) >= 2, responses)
expect('Method' in json.dumps(responses[9]), responses)
expect('Instance' not in json.dumps(responses[9]), responses)
expect('Echo' in json.dumps(responses[11]), responses)
print(f'#331 Python checks: {count}')
