#!/usr/bin/env python3
"""Permanent #339 static API integration and generic default surface regressions."""
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMPILER = ROOT / 'bin/voidc'
FIXTURE = ROOT / 'Tests/StaticApiIntegration'
GENERATED_C = FIXTURE / '.void/StaticApiIntegration.c'
EQUALITY = ROOT / 'StandardLibrary/Void/Collections/EqualityComparer.void'
INDEX = ROOT / 'StandardLibrary/Void/Index.void'
RANGE = ROOT / 'StandardLibrary/Void/Range.void'
count = 0


def expect(condition, detail):
    global count
    assert condition, detail
    count += 1
    print('True')


def run(*args):
    return subprocess.run([str(COMPILER), *map(str, args)], cwd=ROOT,
                          capture_output=True, text=True, timeout=180)


# End-to-end composition: generic default cache, GC, Index/Range values, copy/view ownership,
# closed generic storage, existing parsing/static values, and ordinary collection equality.
result = run('run', FIXTURE)
expect(result.returncode == 0, result.stderr)
observed = [line for line in result.stdout.splitlines() if line in ('True', 'False')]
expect(len(observed) == 41, (len(observed), result.stdout))
for index, line in enumerate(observed):
    expect(line == 'True', f'runtime assertion {index + 1}: {result.stdout}')

# Public declarations are ordinary source-visible StandardLibrary members.
equality = EQUALITY.read_text()
index_source = INDEX.read_text()
range_source = RANGE.read_text()
expect('public static EqualityComparer<T> Default { get { return DefaultEqualityComparer<T>.Instance; } }' in equality, equality)
expect('internal static readonly EqualityComparer<T> Instance = new DefaultEqualityComparer<T>();' in equality, equality)
expect('Runtime.Equals(x, y)' in equality and 'Runtime.Hash(obj)' in equality, equality)
expect('public static Index Start { get { return new Index(0, false); } }' in index_source, index_source)
expect('public static Index End { get { return new Index(0, true); } }' in index_source, index_source)
expect('public static Range All { get { return new Range(Index.Start, Index.End); } }' in range_source, range_source)
expect('public static Index FromStart(int value)' in index_source and 'public static Index FromEnd(int value)' in index_source, index_source)
expect('public static Range StartAt(Index start)' in range_source and 'public static Range EndAt(Index end)' in range_source, range_source)

# No compiler API-name branch implements these surfaces. The compiler change is only shared type display.
compiler_sources = ''.join((ROOT / path).read_text() for path in [
    'Compiler/src/compiler.c', 'Compiler/src/semantic.c', 'Compiler/src/monomorph.c'])
expect('EqualityComparer' not in compiler_sources, 'compiler must not special-case EqualityComparer.Default')
expect('Index.Start' not in compiler_sources and 'Index.End' not in compiler_sources, 'compiler must not special-case Index statics')
expect('Range.All' not in compiler_sources, 'compiler must not special-case Range.All')
expect('vc_semantic_type_display_name' in compiler_sources, 'shared semantic display path must exist')

# Generated C must remain strict/deterministic and retain distinct closed comparer specializations.
built = run('build', FIXTURE)
expect(built.returncode == 0, built.stderr)
expect(GENERATED_C.exists(), GENERATED_C)
generated_first = GENERATED_C.read_bytes()
expect(b'DefaultEqualityComparer__g1_int' in generated_first, 'missing int default comparer specialization')
expect(b'DefaultEqualityComparer__g1_string' in generated_first, 'missing string default comparer specialization')
expect(b'DefaultEqualityComparer__g1_Value339' in generated_first, 'missing value-type default comparer specialization')
expect(b'DefaultEqualityComparer__g1_Reference339' in generated_first, 'missing reference-type default comparer specialization')
expect('private static readonly EqualityComparer<T> _default' not in equality,
       'Default cache must not live on the generic base and reintroduce circular initialization')
rebuilt = run('build', FIXTURE)
expect(rebuilt.returncode == 0, rebuilt.stderr)
expect(GENERATED_C.read_bytes() == generated_first, 'generated C changed across identical builds')

# Assignment to the get-only/default surfaces uses ordinary property diagnostics.
with tempfile.TemporaryDirectory(prefix='void339-diag-') as temporary:
    source = Path(temporary) / 'Program.void'

    def diagnostic(statement):
        source.write_text('using Void; using Void.Collections; public static class Program { public static void Main() { ' +
                          statement + '; } }')
        checked = run('check', source, '--diagnostics=json')
        expect(checked.returncode != 0, (statement, checked.stdout, checked.stderr))
        records = [json.loads(line) for line in checked.stderr.splitlines() if line.startswith('{')]
        expect(len(records) == 1, records)
        record = records[0]
        expect(record['code'] == 'VOID3000', record)
        expect('__g' not in record['message'] and 'DefaultEqualityComparer' not in record['message'], record)
        return record

    default_assignment = diagnostic('EqualityComparer<int>.Default = EqualityComparer<int>.Default')
    expect("property 'Default' is read-only" in default_assignment['message'], default_assignment)
    start_assignment = diagnostic('Index.Start = new Index(1, false)')
    expect("property 'Start' is read-only" in start_assignment['message'], start_assignment)
    end_assignment = diagnostic('Index.End = new Index(1, true)')
    expect("property 'End' is read-only" in end_assignment['message'], end_assignment)
    range_assignment = diagnostic('Range.All = new Range(Index.Start, Index.End)')
    expect("property 'All' is read-only" in range_assignment['message'], range_assignment)

# LSP uses the same semantic declarations. Closed generic display must not expose __g names.
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
    request(2, 'completion', 'EqualityComparer<int>.Default', len('EqualityComparer<int>.')),
    request(3, 'completion', 'Index.Start', len('Index.')),
    request(4, 'completion', 'Range.All', len('Range.')),
    request(5, 'hover', 'EqualityComparer<int>.Default', len('EqualityComparer<int>.')),
    request(6, 'hover', 'Index.Start', len('Index.')),
    request(7, 'hover', 'Range.All', len('Range.')),
    request(8, 'definition', 'EqualityComparer<int>.Default', len('EqualityComparer<int>.')),
    request(9, 'definition', 'Index.Start', len('Index.')),
    request(10, 'definition', 'Range.All', len('Range.')),
    request(11, 'references', 'EqualityComparer<int>.Default', len('EqualityComparer<int>.'),
            context={'includeDeclaration': True}),
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
    header, separator, rest = remaining.partition(b'\r\n\r\n')
    expect(bool(separator), remaining[:200])
    length = None
    for line in header.split(b'\r\n'):
        if line.lower().startswith(b'content-length:'):
            length = int(line.split(b':', 1)[1])
            break
    expect(length is not None, header)
    body = rest[:length]
    remaining = rest[length:]
    response = json.loads(body)
    if 'id' in response:
        responses[response['id']] = response

for identifier in range(1, 12):
    expect(identifier in responses and 'error' not in responses[identifier], responses)

completion_default = responses[2]['result']['items']
default_items = [item for item in completion_default if item.get('label') == 'Default']
expect(len(default_items) == 1, completion_default)
expect(default_items[0].get('detail') == 'EqualityComparer<int>', default_items)
expect('__g' not in json.dumps(completion_default), completion_default)

index_labels = {item.get('label') for item in responses[3]['result']['items']}
expect('Start' in index_labels and 'End' in index_labels, index_labels)
range_labels = {item.get('label') for item in responses[4]['result']['items']}
expect('All' in range_labels, range_labels)

hover_default = json.dumps(responses[5]['result'])
expect('property Default: EqualityComparer<int>' in hover_default, hover_default)
expect('__g' not in hover_default, hover_default)
expect('property Start: Index' in json.dumps(responses[6]['result']), responses[6])
expect('property All: Range' in json.dumps(responses[7]['result']), responses[7])

expect(responses[8]['result']['uri'].endswith('/StandardLibrary/Void/Collections/EqualityComparer.void'), responses[8])
expect(responses[9]['result']['uri'].endswith('/StandardLibrary/Void/Index.void'), responses[9])
expect(responses[10]['result']['uri'].endswith('/StandardLibrary/Void/Range.void'), responses[10])
references = responses[11]['result']
expect(any(item['uri'].endswith('/StandardLibrary/Void/Collections/EqualityComparer.void') for item in references), references)
expect(sum(1 for item in references if item['uri'].endswith('/Tests/StaticApiIntegration/Program.void')) >= 3, references)
expect('__g' not in json.dumps(responses[2:8] if isinstance(responses, list) else {
    key: responses[key] for key in (2, 3, 4, 5, 6, 7)
}), responses)

print(f'#339 static API integration regression: {count} checks')
