#!/usr/bin/env python3
"""Permanent #337 integral, bool, and Unicode-scalar parsing regressions."""
import json
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / 'bin/voidc'
FIXTURE = ROOT / 'Tests/BuiltInIntegralBoolCharParsing'
GENERATED_C = FIXTURE / '.void/BuiltInIntegralBoolCharParsing.c'
ASSOCIATED = ROOT / 'StandardLibrary/Void/Internal/BuiltInAssociatedMembers.void'
PARSING = ROOT / 'StandardLibrary/Void/Internal/PrimitiveParsing.void'
count = 0


def expect(condition, detail):
    global count
    assert condition, detail
    count += 1
    print('True')


def run(*args):
    return subprocess.run([str(COMPILER), *map(str, args)], cwd=ROOT,
                          capture_output=True, text=True, timeout=120)


# End-to-end semantics: every runtime assertion is counted separately.
result = run('run', FIXTURE)
expect(result.returncode == 0, result.stderr)
lines = [line for line in result.stdout.splitlines() if line in ('True', 'False')]
expect(len(lines) >= 70, (len(lines), result.stdout))
for index, line in enumerate(lines):
    expect(line == 'True', f'runtime assertion {index + 1}: {result.stdout}')

# Public APIs are genuine declarations on the #335 associated scopes.
associated = ASSOCIATED.read_text()
for type_name in ['sbyte', 'byte', 'short', 'ushort', 'int', 'uint', 'long', 'ulong']:
    expect(f'public static {type_name} Parse(string value)' in associated, type_name)
    expect(f'public static bool TryParse(string value, out {type_name} result)' in associated, type_name)
for type_name in ['bool', 'char']:
    expect(f'public static {type_name} Parse(string value)' in associated, type_name)
    expect(f'public static bool TryParse(string value, out {type_name} result)' in associated, type_name)
expect('public static float Parse(string value)' in associated and 'out float result' in associated, 'float parsing is attached by #338')
expect('public static double Parse(string value)' in associated and 'out double result' in associated, 'double parsing is attached by #338')
expect('public static decimal Parse(string value)' in associated and 'out decimal result' in associated, 'decimal parsing is attached by #338')

# One shared editable StandardLibrary integer parser, with overflow-safe unsigned accumulation.
parsing = PARSING.read_text()
expect('ParseIntegerMagnitude' in parsing, parsing)
expect('(limit - digit) / 10ul' in parsing, parsing)
expect('magnitude = magnitude * 10ul + digit;' in parsing, parsing)
expect('strtol' not in parsing and 'strtoul' not in parsing and 'atoi' not in parsing and 'sscanf' not in parsing, parsing)
expect("value == ' ' || value == '\\t' || value == '\\n'" in parsing, parsing)
expect("value == '\\r' || value == '\\f' || value == '\\v'" in parsing, parsing)
expect('value.Length != 1' in parsing and 'result = value[0];' in parsing, parsing)
expect('EqualsAsciiIgnoreCase' in parsing, parsing)
expect('Runtime.' not in parsing.split('internal static bool EqualsAsciiIgnoreCase', 1)[0], 'the locked #337 integer parser should not need a runtime primitive')

# Signed minimum conversion never casts the out-of-range unsigned magnitude directly to signed.
expect('-(long)(magnitude - 1ul) - 1l' in associated, associated)
expect('(long)magnitude' in associated, associated)
expect('9223372036854775808ul' in associated, associated)
expect('18446744073709551615ul' in associated, associated)

# The compiler has no Parse/TryParse API-name branch.
compiler_sources = ''.join((ROOT / 'Compiler/src' / name).read_text()
                           for name in ['semantic.c', 'compiler.c', 'monomorph.c'])
expect('"TryParse"' not in compiler_sources, 'compiler must not special-case TryParse')
# Existing parser/compiler internals may contain the English word parse, but not the public API string.
expect('"Parse"' not in compiler_sources, 'compiler must not special-case Parse')

# Generated C must build strictly and remain deterministic.
built = run('build', FIXTURE)
expect(built.returncode == 0, built.stderr)
expect(GENERATED_C.exists(), GENERATED_C)
generated_first = GENERATED_C.read_bytes()
expect(b'18446744073709551615' in generated_first, 'ulong limit missing from generated C')
expect(b'9223372036854775808' in generated_first, 'signed long minimum magnitude missing')
rebuilt = run('build', FIXTURE)
expect(rebuilt.returncode == 0, rebuilt.stderr)
expect(GENERATED_C.read_bytes() == generated_first, 'generated C changed across identical builds')

# Static call diagnostics remain the ordinary call/member families; #338 remains absent.
with tempfile.TemporaryDirectory(prefix='void337-diag-') as temporary:
    source = Path(temporary) / 'Program.void'

    def diagnostic(statement):
        source.write_text('using Void; public static class Program { public static void Main() { ' +
                          statement + '; } }')
        result = run('check', source, '--diagnostics=json')
        expect(result.returncode != 0, (statement, result.stdout, result.stderr))
        records = [json.loads(line) for line in result.stderr.splitlines() if line.startswith('{')]
        expect(len(records) == 1, records)
        record = records[0]
        expect('AssociatedMembers' not in record['message'] and 'PrimitiveParsing' not in record['message'], record)
        return record

    no_args = diagnostic('int.Parse()')
    expect(no_args['code'] == 'VOID3003', no_args)
    too_many = diagnostic('int.Parse("1", "2")')
    expect(too_many['code'] == 'VOID3003', too_many)
    try_missing_out = diagnostic('int.TryParse("1")')
    expect(try_missing_out['code'] == 'VOID3003', try_missing_out)
    later = diagnostic('float.ParseExact("1")')
    expect(later['code'] == 'VOID3012' and 'ParseExact' in later['message'], later)

# LSP consumes the associated declarations: completion, hover, signature, definition, references.
text = (FIXTURE / 'Program.void').read_text()
uri = (FIXTURE / 'Program.void').resolve().as_uri()


def position_at(needle, within=0, occurrence=0):
    offset = -1
    start = 0
    for _ in range(occurrence + 1):
        offset = text.index(needle, start)
        start = offset + 1
    offset += within
    return {'line': text[:offset].count('\n'),
            'character': offset - text.rfind('\n', 0, offset) - 1}


def request(identifier, method, needle, within=0, occurrence=0, **extra):
    params = dict(textDocument=dict(uri=uri), position=position_at(needle, within, occurrence), **extra)
    return dict(jsonrpc='2.0', id=identifier, method='textDocument/' + method, params=params)


messages = [
    dict(jsonrpc='2.0', id=1, method='initialize', params={}),
    dict(jsonrpc='2.0', method='textDocument/didOpen', params=dict(
        textDocument=dict(uri=uri, languageId='void', version=1, text=text))),
    request(2, 'completion', 'int.Parse("0")', len('int.')),
    request(3, 'completion', 'bool.Parse("True")', len('bool.')),
    request(4, 'completion', 'char.TryParse("A"', len('char.')),
    request(5, 'hover', 'int.Parse("2147483647")', len('int.')),
    request(6, 'hover', 'int.TryParse("-2147483649"', len('int.')),
    request(7, 'signatureHelp', 'int.Parse("2147483647")', len('int.Parse(')),
    request(8, 'signatureHelp', 'int.TryParse("-2147483649"', len('int.TryParse(')),
    request(9, 'definition', 'int.Parse("2147483647")', len('int.')),
    request(10, 'definition', 'int.TryParse("-2147483649"', len('int.')),
    request(11, 'references', 'int.Parse("2147483647")', len('int.'), context={'includeDeclaration': True}),
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

for identifier in [2, 3, 4]:
    labels = {item['label'] for item in responses[identifier]['items']}
    expect({'Parse', 'TryParse'} <= labels, (identifier, labels))
    expect(not any('AssociatedMembers' in label or 'PrimitiveParsing' in label for label in labels), labels)
hover_parse = json.dumps(responses[5])
hover_try = json.dumps(responses[6])
expect('Parse' in hover_parse and 'string' in hover_parse and 'int' in hover_parse, responses[5])
expect('TryParse' in hover_try and 'out' in hover_try and 'int' in hover_try, responses[6])
sig_parse = json.dumps(responses[7])
sig_try = json.dumps(responses[8])
expect('Parse' in sig_parse and 'string' in sig_parse, responses[7])
expect('TryParse' in sig_try and 'out' in sig_try, responses[8])
expect(responses[9] is not None and 'BuiltInAssociatedMembers.void' in responses[9]['uri'], responses[9])
expect(responses[10] is not None and 'BuiltInAssociatedMembers.void' in responses[10]['uri'], responses[10])
expect(isinstance(responses[11], list) and len(responses[11]) >= 2, responses[11])
expect(all('AssociatedMembers' not in json.dumps(responses[i]) for i in [5, 6, 7, 8]), responses)

print(f'#337 Python checks: {count}')
