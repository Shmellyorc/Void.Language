"""Interleaved local crash-diagnostics overhead measurements, never a pass gate.
Usage: performance.py VOIDC VOIDLS OUTPUT_JSON [ENTRY_CONTROL_VOIDC ENTRY_CONTROL_VOIDLS]
Entry controls may link G89 entry objects against the current libraries; that is
an entry-path comparison, not an independently rebuilt accepted-baseline binary.
"""
from pathlib import Path
import json
import os
import random
import statistics
import subprocess
import sys
import tempfile
import time

compiler, server, output = map(Path, sys.argv[1:4])
compiler, server = compiler.resolve(), server.resolve()
controls = [Path(p).resolve() for p in sys.argv[4:6]]
TRIALS = 41

def frame(method, params=None, id=None):
    value = {'jsonrpc': '2.0', 'method': method}
    if params is not None: value['params'] = params
    if id is not None: value['id'] = id
    body = json.dumps(value).encode()
    return b'Content-Length: ' + str(len(body)).encode() + b'\r\n\r\n' + body

with tempfile.TemporaryDirectory(prefix='VOID G90 performance ') as temporary:
    root = Path(temporary)
    source = root / 'main.void'
    source.write_text('func main() -> i32:\n    return 17\n')
    mode_env = {}
    for mode in ['unset', 'off', 'on'] + (['entry-control'] if controls else []):
        env = dict(os.environ, VOID_CRASH_DIRECTORY=str(root / 'reports'))
        env.pop('VOID_CRASH_DIAGNOSTICS', None)
        if mode in ('off', 'on'): env['VOID_CRASH_DIAGNOSTICS'] = '1' if mode == 'on' else '0'
        mode_env[mode] = env
    tasks = {}
    for mode in mode_env:
        cc, ls = controls if mode == 'entry-control' else (compiler, server)
        session = frame('initialize', {'capabilities': {}}, 1) + frame('initialized', {})
        session += b''.join(frame('textDocument/hover', {'textDocument': {'uri': source.as_uri()}, 'position': {'line': 0, 'character': 5}}, i+2) for i in range(100))
        session += frame('shutdown', None, 102) + frame('exit')
        for task, command, data in [('version', [str(cc), '--version'], b''),
                                    ('check', [str(cc), 'check', str(source)], b''),
                                    ('jit-o0', [str(cc), 'run', str(source), '--no-opt', '--no-cache'], b''),
                                    ('lsp-100-hover-session', [str(ls), '--entry', str(source)], session)]:
            tasks[(mode, task)] = (command, data)
    samples = {mode: {task: [] for selected, task in tasks if selected == mode} for mode in mode_env}
    rng = random.Random(90)
    def measure(key):
        mode, task = key
        command, data = tasks[key]
        begin = time.perf_counter_ns()
        result = subprocess.run(command, env=mode_env[mode], input=data, capture_output=True, timeout=20)
        elapsed = (time.perf_counter_ns() - begin) / 1e6
        assert result.returncode == (17 if task == 'jit-o0' else 0), (key, result)
        assert b'VOID crash report:' not in result.stderr
        return elapsed
    for key in tasks: measure(key)  # One warmup per combination.
    for _ in range(TRIALS):
        order = list(tasks); rng.shuffle(order)
        for key in order: samples[key[0]][key[1]].append(measure(key))
    assert not (root / 'reports').exists()
    summary = {mode: {task: {'median_ms': statistics.median(values),
                             'min_ms': min(values), 'max_ms': max(values), 'samples_ms': values}
                      for task, values in results.items()} for mode, results in samples.items()}
    output.write_text(json.dumps({'trials': TRIALS, 'interleaving_seed': 90,
        'compiler': str(compiler), 'server': str(server), 'entry_controls': list(map(str, controls)),
        'entry_control_scope': 'G89 entry objects + current libraries; not a full baseline rebuild',
        'results': summary}, indent=2) + '\n')
    for mode, results in summary.items():
        print(mode, {task: round(value['median_ms'], 3) for task, value in results.items()})
