#!/usr/bin/env python3
"""Permanent #336 built-in static value/constant/property regressions."""
import json
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / 'bin/voidc'
FIXTURE = ROOT / 'Tests/BuiltInStaticValues'
GENERATED_C = FIXTURE / '.void/BuiltInStaticValues.c'
ASSOCIATED = ROOT / 'StandardLibrary/Void/Internal/BuiltInAssociatedMembers.void'
count = 0


def expect(condition, detail):
    global count
    assert condition, detail
    count += 1
    print('True')


def run(*args):
    return subprocess.run([str(COMPILER), *map(str, args)], cwd=ROOT,
                          capture_output=True, text=True, timeout=120)


# Runtime semantics across every public #336 value plus const/switch/generic/string regressions.
result = run('run', FIXTURE)
expect(result.returncode == 0, result.stderr)
observed = [line for line in result.stdout.splitlines() if line in ('True', 'False')]
expect(observed == ['True'] * 29, result.stdout)

# Public declarations belong to #335 associated StandardLibrary scopes and remain true consts.
associated_text = ASSOCIATED.read_text()
required_declarations = [
    'public const byte MinValue', 'public const byte MaxValue',
    'public const sbyte MinValue', 'public const sbyte MaxValue',
    'public const short MinValue', 'public const short MaxValue',
    'public const ushort MinValue', 'public const ushort MaxValue',
    'public const int MinValue', 'public const int MaxValue',
    'public const uint MinValue', 'public const uint MaxValue',
    'public const long MinValue', 'public const long MaxValue',
    'public const ulong MinValue', 'public const ulong MaxValue',
    'public const char MinValue', 'public const char MaxValue',
    'public const float Epsilon', 'public const float NaN',
    'public const float PositiveInfinity', 'public const float NegativeInfinity',
    'public const double Epsilon', 'public const double NaN',
    'public const double PositiveInfinity', 'public const double NegativeInfinity',
    'public const decimal Zero', 'public const decimal One', 'public const decimal MinusOne',
]
for declaration in required_declarations:
    expect(declaration in associated_text, declaration)
expect('[PrimitiveConstant("epsilon")] public const float Epsilon' in associated_text,
       'float.Epsilon must be semantic epsilon kind')
expect('[PrimitiveConstant("epsilon")] public const double Epsilon' in associated_text,
       'double.Epsilon must be semantic epsilon kind')

# Generated C proves exact backend representation and strict compilation.
built = run('build', FIXTURE)
expect(built.returncode == 0, built.stderr)
generated = GENERATED_C.read_text()
for spelling in [
    'INT8_MIN', 'INT8_MAX', 'UINT8_MAX', 'INT16_MIN', 'INT16_MAX', 'UINT16_MAX',
    'INT32_MIN', 'INT32_MAX', 'UINT32_MAX', 'INT64_MIN', 'INT64_MAX', 'UINT64_MAX',
    'INT32_C(0x10FFFF)', '(-FLT_MAX)', 'FLT_MAX', 'FLT_TRUE_MIN',
    '(-DBL_MAX)', 'DBL_MAX', 'DBL_TRUE_MIN', 'NAN', 'INFINITY',
    '(-LDBL_MAX)', 'LDBL_MAX', '0.0L', '1.0L', '(-1.0L)']:
    expect(spelling in generated, spelling)
# Critical C#-familiar semantic distinction: MinValue is -Max finite; Epsilon is true minimum.
for wrong in [r'\bFLT_MIN\b', r'\bFLT_EPSILON\b', r'\bDBL_MIN\b', r'\bDBL_EPSILON\b']:
    expect(re.search(wrong, generated) is None, wrong)
expect('/ 0' not in generated and '/0' not in generated,
       'NaN/infinity must not be manufactured with divide-by-zero arithmetic')
expect('PrimitiveConstant' not in generated,
       'compiler-only primitive-constant metadata must not leak into generated runtime metadata')

# Deterministic generated representation.
first = GENERATED_C.read_bytes()
rebuilt = run('build', FIXTURE)
expect(rebuilt.returncode == 0, rebuilt.stderr)
expect(GENERATED_C.read_bytes() == first, 'generated C changed across identical builds')

# Compiler code must describe primitive constant categories/types, not public API member names.
compiler_sources = (ROOT / 'Compiler/src/semantic.c').read_text() + (ROOT / 'Compiler/src/compiler.c').read_text()
for public_name in ['"MaxValue"', '"MinValue"', '"Epsilon"', '"PositiveInfinity"',
                    '"NegativeInfinity"', '"MinusOne"']:
    expect(public_name not in compiler_sources, public_name)

# Ordinary diagnostics: immutable constant, normal conversion rules, later APIs still absent.
with tempfile.TemporaryDirectory(prefix='void336-diag-') as temporary:
    source = Path(temporary) / 'Program.void'

    def diagnostic(statement):
        source.write_text('using Void; public static class Program { public static void Main() { ' +
                          statement + '; } }')
        result = run('check', source, '--diagnostics=json')
        expect(result.returncode != 0, (statement, result.stdout, result.stderr))
        records = [json.loads(line) for line in result.stderr.splitlines() if line.startswith('{')]
        expect(len(records) == 1, records)
        record = records[0]
        expect('AssociatedMembers' not in record['message'] and 'Void.Internal' not in record['message'], record)
        return record

    immutable = diagnostic('int.MaxValue = 10')
    expect('constant' in immutable['message'].lower() or 'assign' in immutable['message'].lower(), immutable)

    conversion = diagnostic('int value = long.MaxValue')
    expect("long" in conversion['message'] and "int" in conversion['message'], conversion)

    bool_value = diagnostic('Console.WriteLine(bool.MaxValue)')
    expect("type 'bool'" in bool_value['message'] and 'MaxValue' in bool_value['message'], bool_value)

    parsing = diagnostic('Console.WriteLine(float.ParseExact("1"))')
    expect("type 'float'" in parsing['message'] and 'ParseExact' in parsing['message'], parsing)

# Compiler-only primitive-constant association cannot be introduced by user code.
with tempfile.TemporaryDirectory(prefix='void336-attr-') as temporary:
    source = Path(temporary) / 'Program.void'
    source.write_text('public static class Holder336 { [PrimitiveConstant("max")] public const int X = default; } public static class Program { public static void Main() {} }')
    checked = run('check', source, '--diagnostics=json')
    expect(checked.returncode != 0, (checked.stdout, checked.stderr))

# LSP completion/hover/definition consume the same semantic associated declarations.
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
    request(2, 'completion', 'int.MinValue ==', len('int.')),
    request(3, 'completion', 'float.MinValue ==', len('float.')),
    request(4, 'completion', 'double.MinValue ==', len('double.')),
    request(5, 'completion', 'char.MinValue;', len('char.')),
    request(6, 'completion', 'decimal.MinValue ==', len('decimal.')),
    request(7, 'hover', 'int.MaxValue == 2147483647', len('int.')),
    request(8, 'definition', 'int.MaxValue == 2147483647', len('int.')),
    request(9, 'hover', 'float.Epsilon >', len('float.')),
    request(10, 'definition', 'float.Epsilon >', len('float.')),
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

expected_completion = {
    2: {'MinValue', 'MaxValue'},
    3: {'MinValue', 'MaxValue', 'Epsilon', 'NaN', 'PositiveInfinity', 'NegativeInfinity'},
    4: {'MinValue', 'MaxValue', 'Epsilon', 'NaN', 'PositiveInfinity', 'NegativeInfinity'},
    5: {'MinValue', 'MaxValue'},
    6: {'MinValue', 'MaxValue', 'Zero', 'One', 'MinusOne'},
}
for identifier, expected in expected_completion.items():
    labels = {item['label'] for item in responses[identifier]['items']}
    expect(expected <= labels, (identifier, expected, labels))
    expect(not any('AssociatedMembers' in item['label'] for item in responses[identifier]['items']), labels)

hover_int = json.dumps(responses[7])
hover_float = json.dumps(responses[9])
expect('MaxValue' in hover_int and 'int' in hover_int, responses[7])
expect('Epsilon' in hover_float and 'float' in hover_float, responses[9])
expect(responses[8] is not None and 'BuiltInAssociatedMembers.void' in responses[8]['uri'], responses[8])
expect(responses[10] is not None and 'BuiltInAssociatedMembers.void' in responses[10]['uri'], responses[10])

print(f'#336 Python checks: {count}')
