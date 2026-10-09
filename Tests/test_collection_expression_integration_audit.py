#!/usr/bin/env python3
"""#370 independent integration probes; use the established strict-C helpers."""
from pathlib import Path
import json
import os
import shlex
import shutil
import subprocess
import tempfile
import test_collection_expression_stabilization as support

ROOT = support.ROOT
FIXTURE = ROOT / 'Tests/CollectionExpressionIntegrationAudit'
NAME = 'CollectionExpressionIntegrationAudit'
NEGATIVES = {
    'Nested': ('using Void; public static class Program { static void Pick(int[][] a) {} static void Pick(Span<int> a) {} public static void Main() { Pick([["bad"]]); } }', 'VOID3003', 'no matching method'),
    'New': ('using Void; public static class Program { static void Pick(int[] a) {} public static void Main() { Pick([new(7)]); } }', 'VOID3003', 'no matching method'),
    'Lambda': ('using Void; public static class Program { static void Pick(int[] a) {} public static void Main() { Pick([() => 7]); } }', 'VOID3003', 'no matching method'),
    'Spread': ('using Void; public static class Program { public static void Main() { int[] a = [..7]; } }', 'VOID3000', 'spread source must be a one-dimensional array'),
    'SpanEscape': ('using Void; public sealed class Holder { public Span<int> Data = [1, 2]; } public static class Program { public static void Main() {} }', 'VOID3000', 'ref struct field'),
    'SpanAsync': ('using Void; using Void.Threading.Tasks; public static class Program { public static async Task F() { Span<int> a = [1,2]; await Task.CompletedTask; } public static void Main() {} }', 'VOID3000', 'ref struct'),
    'Malformed': ('using Void; public static class Program { public static void Main() { int[][] a = [[1], [2,,3]]; } }', 'VOID2001', "expected expression, found ','"),
}


def lsp(path: Path, source: str) -> list[dict]:
    messages = [dict(jsonrpc='2.0', id=1, method='initialize', params={}),
        dict(jsonrpc='2.0', method='textDocument/didOpen', params=dict(textDocument=dict(
            uri=path.as_uri(), languageId='void', version=1, text=source))),
        dict(jsonrpc='2.0', id=2, method='shutdown', params=None),
        dict(jsonrpc='2.0', method='exit', params=None)]
    request = b''
    for message in messages:
        body = json.dumps(message).encode()
        request += b'Content-Length: ' + str(len(body)).encode() + b'\r\n\r\n' + body
    result = subprocess.run([str(support.COMPILER), 'lsp'], input=request,
        capture_output=True, cwd=ROOT, timeout=90)
    assert result.returncode == 0 and not result.stderr, result
    remaining = result.stdout
    reports = []
    while remaining:
        header, _, content = remaining.partition(b'\r\n\r\n')
        size = int(header.split(b':', 1)[1])
        reports.append(json.loads(content[:size]))
        remaining = content[size:]
    return reports


def main() -> None:
    with tempfile.TemporaryDirectory(prefix='void370-audit-') as folder:
        tmp = Path(folder)
        fixture = tmp / NAME
        shutil.copytree(FIXTURE, fixture, ignore=shutil.ignore_patterns('.void', 'bin', 'publish'))
        support.GENERATED = fixture / '.void' / (NAME + '.c')
        support.EXPECTED_OUTPUT = 'True\n' * 24
        check, run = support.check, support.run
        version = run(support.COMPILER, 'version')
        check(version.returncode == 0 and version.stdout.strip() == 'voidc 0.0.380', version.stdout)
        built = run(support.COMPILER, 'build', fixture)
        check(built.returncode == 0 and not built.stderr, built.stdout + built.stderr)
        executable = fixture / 'bin' / (NAME + ('.exe' if os.name == 'nt' else ''))
        support.verify_run(executable)
        generated = support.GENERATED.read_bytes()
        built = run(support.COMPILER, 'build', fixture)
        check(built.returncode == 0 and not built.stderr and support.GENERATED.read_bytes() == generated,
              built.stdout + built.stderr)
        parsed = run(support.COMPILER, 'parse', fixture / 'Program.void')
        check(parsed.returncode == 0 and 'CollectionExpression' in parsed.stdout and 'SpreadElement' in parsed.stdout,
              parsed.stdout + parsed.stderr)
        for name, (source, code, message) in NEGATIVES.items():
            path = tmp / name / 'Program.void'; path.parent.mkdir(); path.write_text(source)
            result = run(support.COMPILER, 'check', path, '--diagnostics=json')
            reports = [json.loads(line) for line in result.stderr.splitlines() if line.startswith('{')]
            report = reports[0] if reports else {}
            span = report.get('span') or {}
            a = (span.get('start') or {}).get('offset', -1)
            b = (span.get('end') or {}).get('offset', -1)
            check(result.returncode != 0 and len(reports) == 1 and report.get('code') == code and
                message in report.get('message', '') and 0 <= a < b <= len(source), result.stderr)
        valid = lsp(fixture / 'Program.void', (fixture / 'Program.void').read_text())
        check(any(r.get('method') == 'textDocument/publishDiagnostics' and
            r['params']['diagnostics'] == [] for r in valid), str(valid))
        source = NEGATIVES['Nested'][0]
        invalid = lsp(tmp / 'Nested/Program.void', source)
        check(any(r.get('method') == 'textDocument/publishDiagnostics' and
            any('no matching method' in d['message'] for d in r['params']['diagnostics']) for r in invalid), str(invalid))
        primary = shlex.split(os.environ.get('CC', 'cc'))
        for opt in ('-O0', '-O2'):
            target = tmp / ('primary-' + opt[1:])
            support.strict_compile(primary, opt, target)
            support.verify_run(target)
            print(f'{primary} {opt} strict ISO C11: passed', flush=True)
        if shutil.which('clang') and os.name != 'nt':
            for opt in ('-O0', '-O2'):
                target = tmp / ('clang-' + opt[1:])
                support.strict_compile(['clang'], opt, target, count=False)
                support.verify_run(target, count=False)
                print(f'clang {opt} strict ISO C11: passed', flush=True)
        else:
            print('Clang: unavailable', flush=True)
        if os.name != 'nt':
            for sanitizer in ('undefined', 'address'):
                target = tmp / sanitizer
                support.strict_compile(primary, '-O1', target, sanitizer=sanitizer, count=False)
                env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0:abort_on_error=1') if sanitizer == 'address' else None
                support.verify_run(target, env=env, count=False)
                print(f'{sanitizer} sanitizer: passed', flush=True)
        else:
            print('ASan/UBSan: unavailable', flush=True)
        assert support.checks == 18, support.checks
        print(f'# {support.checks} checks; 24 runtime assertions per execution', flush=True)


if __name__ == '__main__':
    main()
