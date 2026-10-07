#!/usr/bin/env python3
"""Permanent #338 floating-point and decimal parsing regressions."""
import json
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / 'bin/voidc'
FIXTURE = ROOT / 'Tests/BuiltInFloatingDecimalParsing'
GENERATED_C = FIXTURE / '.void/BuiltInFloatingDecimalParsing.c'
ASSOCIATED = ROOT / 'StandardLibrary/Void/Internal/BuiltInAssociatedMembers.void'
PARSING = ROOT / 'StandardLibrary/Void/Internal/PrimitiveParsing.void'
COMPILER_C = ROOT / 'Compiler/src/compiler.c'
SEMANTIC_C = ROOT / 'Compiler/src/semantic.c'
count = 0


def expect(condition, detail):
    global count
    assert condition, detail
    count += 1
    print('True')


def run(*args):
    return subprocess.run([str(COMPILER), *map(str, args)], cwd=ROOT,
                          capture_output=True, text=True, timeout=180)


# End-to-end numeric semantics. Every fixture assertion is counted separately.
result = run('run', FIXTURE)
expect(result.returncode == 0, result.stderr)
lines = [line for line in result.stdout.splitlines() if line in ('True', 'False')]
expect(len(lines) == 86, (len(lines), result.stdout))
for index, line in enumerate(lines):
    expect(line == 'True', f'runtime assertion {index + 1}: {result.stdout}')

# Public APIs are ordinary declarations on the #335 associated built-in scopes.
associated = ASSOCIATED.read_text()
for type_name in ['float', 'double', 'decimal']:
    expect(f'public static {type_name} Parse(string value)' in associated, type_name)
    expect(f'public static bool TryParse(string value, out {type_name} result)' in associated, type_name)
for old_type in ['int', 'bool', 'char']:
    expect(f'public static {old_type} Parse(string value)' in associated, old_type)
    expect(f'public static bool TryParse(string value, out {old_type} result)' in associated, old_type)
expect('NumberStyles' not in associated and 'IFormatProvider' not in associated and 'CultureInfo' not in associated, associated)
expect('MathF' not in associated and 'Math.' not in associated, associated)

# High-level grammar and policy stay in editable .void code and reuse #337 whitespace.
parsing = PARSING.read_text()
expect('ScanFloatingNumber' in parsing, parsing)
expect('TrimAsciiWhitespace(value, out start, out end)' in parsing, parsing)
expect('significandDigits' in parsing and 'exponentDigits' in parsing, parsing)
expect("value[index] == '.'" in parsing, parsing)
expect("value[index] == 'e' || value[index] == 'E'" in parsing, parsing)
expect('MatchAsciiNaN' in parsing and 'MatchAsciiInfinity' in parsing, parsing)
expect('FloatingSpecialNaN' in parsing and 'FloatingSpecialPositiveInfinity' in parsing and 'FloatingSpecialNegativeInfinity' in parsing, parsing)
expect('special != FloatingSpecialNone' in parsing, 'decimal must reject floating specials')
expect('Runtime.InvariantNumericConvertF32' in parsing, parsing)
expect('Runtime.InvariantNumericConvertF64' in parsing, parsing)
expect('Runtime.InvariantNumericConvertLongDouble' in parsing, parsing)
expect('setlocale' not in parsing, parsing)
expect('strtof' not in parsing and 'strtod' not in parsing and 'strtold' not in parsing, parsing)
expect('try Parse' not in parsing and 'catch' not in parsing, 'TryParse must not use exceptions as control flow')

# The compiler contains only a reusable low-level representation converter, never Parse/TryParse API logic.
compiler_text = COMPILER_C.read_text()
semantic_text = SEMANTIC_C.read_text()
compiler_sources = compiler_text + semantic_text + (ROOT / 'Compiler/src/monomorph.c').read_text()
expect('"Parse"' not in compiler_sources, 'compiler must not special-case public Parse')
expect('"TryParse"' not in compiler_sources, 'compiler must not special-case public TryParse')
for primitive in ['InvariantNumericConvertF32', 'InvariantNumericConvertF64', 'InvariantNumericConvertLongDouble']:
    expect(primitive in compiler_sources, primitive)
expect('Void.Internal.PrimitiveParsing' in semantic_text, semantic_text)
expect('source_is_standard_library(context->source)' in semantic_text, semantic_text)

# Locale isolation is per conversion: Windows and POSIX use explicit C-locale objects.
expect(r'_create_locale(LC_NUMERIC, \"C\")' in compiler_text, compiler_text)
expect(r'newlocale(LC_NUMERIC_MASK, \"C\"' in compiler_text, compiler_text)
expect('_strtof_l' in compiler_text and 'strtof_l' in compiler_text, compiler_text)
expect('_strtod_l' in compiler_text and 'strtod_l' in compiler_text, compiler_text)
expect('_strtold_l' in compiler_text and 'strtold_l' in compiler_text, compiler_text)
expect('setlocale(' not in compiler_text, 'must not mutate ambient process locale')
expect('errno = 0' in compiler_text and 'ERANGE' in compiler_text, compiler_text)
expect('fabsf(parsed) >= FLT_MAX' in compiler_text, compiler_text)
expect('fabs(parsed) >= DBL_MAX' in compiler_text, compiler_text)
expect('fabsl(parsed) >= LDBL_MAX' in compiler_text, compiler_text)
expect('isinf(parsed)' in compiler_text, compiler_text)
expect('vc_utf8_byte_offset' in compiler_text, 'runtime range conversion should respect scalar indices')

# Generated C must contain the invariant converter, strict locale isolation, and stay deterministic.
built = run('build', FIXTURE)
expect(built.returncode == 0, built.stderr)
expect(GENERATED_C.exists(), GENERATED_C)
generated_first = GENERATED_C.read_bytes()
for snippet in [b'vc_invariant_numeric_convert_f32', b'vc_invariant_numeric_convert_f64',
                b'vc_invariant_numeric_convert_long_double', b'newlocale', b'_create_locale',
                b'strtof_l', b'strtod_l', b'strtold_l']:
    expect(snippet in generated_first, snippet)
expect(b'setlocale(' not in generated_first, 'generated program must not mutate global locale')
expect(b'FLT_MAX' in generated_first and b'DBL_MAX' in generated_first and b'LDBL_MAX' in generated_first, generated_first[:1000])
rebuilt = run('build', FIXTURE)
expect(rebuilt.returncode == 0, rebuilt.stderr)
expect(GENERATED_C.read_bytes() == generated_first, 'generated C changed across identical builds')

# Static call errors remain the ordinary callable diagnostics, while runtime text errors stay library exceptions.
with tempfile.TemporaryDirectory(prefix='void338-diag-') as temporary:
    source = Path(temporary) / 'Program.void'

    def diagnostic(statement):
        source.write_text('using Void; public static class Program { public static void Main() { ' +
                          statement + '; } }')
        result = run('check', source, '--diagnostics=json')
        expect(result.returncode != 0, (statement, result.stdout, result.stderr))
        records = [json.loads(line) for line in result.stderr.splitlines() if line.startswith('{')]
        expect(len(records) == 1, records)
        record = records[0]
        expect('AssociatedMembers' not in record['message'] and
               'PrimitiveParsing' not in record['message'] and
               'InvariantNumeric' not in record['message'], record)
        return record

    no_args = diagnostic('float.Parse()')
    expect(no_args['code'] == 'VOID3003', no_args)
    missing_out = diagnostic('double.TryParse("1")')
    expect(missing_out['code'] == 'VOID3003', missing_out)
    too_many = diagnostic('decimal.Parse("1", "2")')
    expect(too_many['code'] == 'VOID3003', too_many)
    unknown = diagnostic('float.ParseExact("1")')
    expect(unknown['code'] == 'VOID3012', unknown)

# LSP must consume the associated StandardLibrary declarations, including out parameters.
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
    request(2, 'completion', 'float.Parse("0")', len('float.')),
    request(3, 'completion', 'double.Parse("0")', len('double.')),
    request(4, 'completion', 'decimal.Parse("0")', len('decimal.')),
    request(5, 'hover', 'float.Parse("1.5")', len('float.')),
    request(6, 'hover', 'double.Parse("0")', len('double.')),
    request(7, 'signatureHelp', 'float.Parse("1.5")', len('float.Parse(')),
    request(8, 'signatureHelp', 'double.TryParse("1e", out d)', len('double.TryParse(')),
    request(9, 'definition', 'decimal.Parse("0")', len('decimal.')),
    request(10, 'references', 'float.Parse("1.5")', len('float.'), context={'includeDeclaration': True}),
    dict(jsonrpc='2.0', id=99, method='shutdown', params=None),
    dict(jsonrpc='2.0', method='exit', params=None),
]
frames = []
for message in messages:
    body = json.dumps(message).encode()
    frames.append(b'Content-Length: ' + str(len(body)).encode() + b'\r\n\r\n' + body)
lsp = subprocess.run([str(COMPILER), 'lsp'], input=b''.join(frames), capture_output=True,
                     cwd=ROOT, timeout=180)
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
for identifier in [5, 6, 7, 8]:
    rendered = json.dumps(responses[identifier])
    expect('Parse' in rendered, responses[identifier])
    expect('AssociatedMembers' not in rendered and 'PrimitiveParsing' not in rendered and 'InvariantNumeric' not in rendered, rendered)
expect('out' in json.dumps(responses[8]), responses[8])
expect(responses[9] is not None and 'BuiltInAssociatedMembers.void' in responses[9]['uri'], responses[9])
expect(isinstance(responses[10], list) and len(responses[10]) >= 2, responses[10])

print(f'#338 Python checks: {count}')
