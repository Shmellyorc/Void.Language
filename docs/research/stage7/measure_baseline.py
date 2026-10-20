#!/usr/bin/env python3
"""Bounded Stage 7 research probe; no compiler/server implementation changes.

Run from any directory: python3 measure_baseline.py --build build/ci-release
Writes JSON to stdout. Fixtures are disposable; uses the existing protocol client.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import statistics
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools'))
from lsp_client import Client


def summary(values):
    ordered = sorted(values)
    return {'samples': len(values), 'median_ms': round(statistics.median(values), 3),
            'p95_ms': round(ordered[math.ceil(.95 * len(values)) - 1], 3),
            'raw_ms': [round(v, 3) for v in values]}


def timed(action):
    start = time.perf_counter()
    result = action()
    return (time.perf_counter() - start) * 1000, result


def rss(pid):
    try:
        lines = Path(f'/proc/{pid}/status').read_text().splitlines()
        return {key: int(next(line.split()[1] for line in lines if line.startswith(key + ':')))
                for key in ('VmRSS', 'VmHWM')}
    except (OSError, StopIteration):
        return None


def fixture(folder, modules, functions):
    folder.mkdir()
    imports, terms = [], []
    for m in range(modules):
        name = f'm{m}'
        imports.append(f'use "/{name}.void" as {name}\n')
        terms.append(f'{name}.fn0(x)')
        (folder / f'{name}.void').write_text(''.join(
            f'func fn{n}(x: i32) -> i32:\n    return x + {n % 10}\n' for n in range(functions)))
    text = ''.join(imports) + 'func entry(x: i32) -> i32:\n    return ' + ' + '.join(terms) + '\n'
    entry = folder / 'main.void'
    entry.write_text(text)
    return entry, text


def benchmark(build, entry, text, repetitions):
    server, compiler = build / 'voidls', build / 'voidc'
    result = {'modules': len(list(entry.parent.glob('*.void'))),
              'source_bytes': sum(p.stat().st_size for p in entry.parent.glob('*.void'))}
    startup, cli = [], []
    for _ in range(repetitions):
        ms, _ = timed(lambda: subprocess.run([str(compiler), 'check', str(entry)],
                      capture_output=True, check=True, timeout=60))
        cli.append(ms)
        start = time.perf_counter()
        client = Client(server, entry)
        diagnostics = client.initialize()
        startup.append((time.perf_counter() - start) * 1000)
        assert all(not d['diagnostics'] for d in diagnostics), diagnostics
        client.request('shutdown'); client.notify('exit'); client.finish()
    result['cli_check_process_including_ir_lowering'] = summary(cli)
    result['server_spawn_initialize_initial_diagnostics_barrier'] = summary(startup)
    client = Client(server, entry)
    client.initialize()
    uri = entry.as_uri()
    client.notify('textDocument/didOpen', {'textDocument':
        {'uri': uri, 'languageId': 'void', 'version': 1, 'text': text}})
    assert all(not d['diagnostics'] for d in client.sync())
    changes, bad, repaired, queries = [], [], [], []
    version = 1
    def change(new_text):
        nonlocal version
        version += 1
        client.notify('textDocument/didChange', {'textDocument': {'uri': uri, 'version': version},
                      'contentChanges': [{'text': new_text}]})
        return client.sync()
    call_at = text.index('.fn0') + 1
    position = {'line': text[:call_at].count('\n'),
                'character': call_at - text.rfind('\n', 0, call_at) - 1}
    for i in range(repetitions):
        ms, ds = timed(lambda: change(text + f'# edit {i}\n'))
        assert all(not d['diagnostics'] for d in ds); changes.append(ms)
        ms, ds = timed(lambda: change(text.replace('.fn0(x)', '.missing(x)', 1)))
        assert any(d['diagnostics'] for d in ds); bad.append(ms)
        stale, _ = client.request('textDocument/definition', {'textDocument': {'uri': uri}, 'position': position})
        assert stale['result'] is None
        ms, ds = timed(lambda: change(text))
        assert all(not d['diagnostics'] for d in ds); repaired.append(ms)
    for _ in range(repetitions * 10):
        ms, (response, _) = timed(lambda: client.request('textDocument/definition',
            {'textDocument': {'uri': uri}, 'position': position}))
        assert response['result']['uri'].endswith('/m0.void'); queries.append(ms)
    result['trivia_full_edit_diagnostics_barrier'] = summary(changes)
    result['invalid_edit_diagnostics_barrier'] = summary(bad)
    result['repair_diagnostics_barrier'] = summary(repaired)
    result['definition_roundtrip'] = summary(queries)
    result['server_memory_kib'] = rss(client.process.pid)
    # Queue full-document replacements without waiting. The barrier counts analyses
    # through diagnostic notifications; no timing assumption identifies the end.
    def burst():
        nonlocal version
        for n in range(20):
            version += 1
            client.notify('textDocument/didChange', {'textDocument': {'uri': uri, 'version': version},
                          'contentChanges': [{'text': text + f'# burst {n}\n'}]})
        return client.sync()
    ms, ds = timed(burst)
    result['queued_20_edits'] = {'elapsed_ms': round(ms, 3), 'diagnostic_notifications': len(ds)}
    result['shutdown_ms'], _ = timed(lambda: (client.request('shutdown'), client.notify('exit'), client.finish()))
    result['shutdown_ms'] = round(result['shutdown_ms'], 3)
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', default='build/ci-release')
    parser.add_argument('--repetitions', type=int, default=7)
    parser.add_argument('--examples-only', action='store_true')
    args = parser.parse_args()
    build = (ROOT / args.build).resolve()
    result = {'baseline': subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip(),
              'host': platform.platform(), 'python': platform.python_version(),
              'cpu': next((line.split(':', 1)[1].strip() for line in Path('/proc/cpuinfo').read_text().splitlines()
                           if line.startswith('model name')), 'unknown'),
              'cpu_count': os.cpu_count(), 'repetitions': args.repetitions,
              'binaries': {name: {'path': str(build / name), 'sha256': hashlib.sha256((build / name).read_bytes()).hexdigest()}
                           for name in ('voidls', 'voidc')}}
    if args.examples_only:
        cases = {}
        for name in ('analysis/main', 'generics/main', 'advanced/combined', 'async/spawn'):
            entry = ROOT / 'examples' / (name + '.void')
            cli, startup = [], []
            for _ in range(args.repetitions):
                ms, _ = timed(lambda: subprocess.run([str(build / 'voidc'), 'check', str(entry)],
                              capture_output=True, check=True, timeout=60))
                cli.append(ms)
                start = time.perf_counter()
                client = Client(build / 'voidls', entry)
                diagnostics = client.initialize()
                startup.append((time.perf_counter() - start) * 1000)
                assert all(not d['diagnostics'] for d in diagnostics), diagnostics
                client.request('shutdown'); client.notify('exit'); client.finish()
            cases[name] = {'cli_check_including_ir_lowering': summary(cli),
                           'server_spawn_initialize_diagnostics_barrier': summary(startup)}
        result['repository_examples'] = cases
        print(json.dumps(result, indent=2))
        return
    with tempfile.TemporaryDirectory(prefix='void-stage7-') as temp:
        cases = {}
        for name, modules, functions in [('small', 2, 5), ('medium', 16, 50), ('large', 32, 100)]:
            entry, text = fixture(Path(temp) / name, modules, functions)
            cases[name] = benchmark(build, entry, text, args.repetitions)
        result['workloads'] = cases
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
