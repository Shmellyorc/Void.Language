"""Render the bounded observations; raw measurements remain authoritative."""
from pathlib import Path
from statistics import median
import json

root = Path(__file__).resolve().parents[2]
evidence = root / 'docs/log/evidence'
rows = json.loads((evidence / 'benchmarks.json').read_text())
assert len(rows) == 216 and all(r.get('gc_allocation_budget') == 256 for r in rows)
modes = ['JIT normal', 'JIT O2', 'AOT normal', 'AOT O2']
def mode(row):
    return ('JIT' if 'void_log_host-' in row['executable'] else 'AOT') + (' O2' if row['optimized'] else ' normal')
workloads = {
    1: 'Disabled direct / 10,000', 2: 'Disabled eager display / 10,000',
    3: 'Disabled enabled-guard / 10,000', 4: 'Disabled named lazy / 10,000',
    5: 'Disabled captured lazy / 10,000', 10: 'Console 64 B / 200',
    11: 'File 64 B / 200', 12: 'File 1 KiB / 100',
    13: 'File 65,500 B / 3', 14: 'File 64 B + UTC / 200',
    15: '20 rotations / 0 backups', 16: '20 rotations / 3 backups',
    17: '20 rotations / 32 backups', 18: 'Direct io identical bytes / 200',
    19: 'Direct console identical bytes / 200', 20: 'File + 20 explicit flushes / 200',
    30: 'Sequential async / 100', 31: 'Sequential async / 1,000',
}
def sample(command, selected):
    result = [r for r in rows if r['command'] == command and mode(r) == selected]
    assert len(result) == 3
    return result
def timing(command, selected, cpu=False):
    values = [r['cpu_seconds'] * 1000 if cpu else r['wall_ns'] / 1e6 for r in sample(command, selected)]
    return f'{median(values):.2f} [{min(values):.2f}, {max(values):.2f}]'
def value(command, key):
    groups = [{r[key] for r in sample(command, selected)} for selected in modes]
    if all(len(group) == 1 for group in groups):
        values = [next(iter(group)) for group in groups]
        return str(values[0]) if len(set(values)) == 1 else ' / '.join(map(str, values))
    return '; '.join(str(sorted(group)) for group in groups)

text = '''# Logging bounded measurements

28 October 2026, local Linux x86-64 Release, pinned LLVM/Clang 19.1.7.
AMD Ryzen 7 1700X, 16 logical CPUs, about 31 GiB RAM. Other local acceptance
processes ran concurrently; these are bounded observations, not isolated-machine
throughput qualification. [Environment](evidence/benchmark-environment.json),
[raw 216 samples](evidence/benchmarks.json), [workloads](../../tests/log/benchmark.void),
[host](../../tests/log/host.cpp), [measurement driver](../../tests/log/benchmarks_default.py).

Each of 18 workloads has a warmup and three measured repetitions in each of four
modes. JIT source analysis/lowering/compilation occurs before execution timing.
AOT probes are emitted and linked independently before launch and link Runtime
without LLVM. The host's actual soft/hard stack limits are both 1,048,576 bytes.
Counts/checksums prevent discarded work; caller message construction, acquisition
and final checked close remain inside each execution interval.

The final timing host uses **Runtime(256)**, matching production JIT creation and
native bootstrap. Its [exact source overlay](evidence/benchmark-default-host.cpp)
changes only the test host's Runtime budget (1→256) and resolves its probe include.
All production libraries, source programs, optimization flags and probe behavior
are shared. The original [216 collection-at-every-allocation samples](evidence/benchmarks-gc-stress.json)
remain separate. An intermediate Runtime(0) experiment disabled automatic GC;
its completed JIT samples and interrupted AOT samples are preserved separately
and excluded from the tables below. The [stop journal](evidence/benchmark-unbudgeted-stop.json)
records the measurement correction; it was not a logger failure.

## Execution wall and CPU time

Milliseconds per whole workload, **median [minimum, maximum]**, three samples.
Payload sizes exclude framing. Console measurements use the existing isolated
output adapter; real OS redirection and broken pipes are functional acceptance
tests, not terminal-rendering performance measurements. File measurements use
actual temporary native files and checked synchronization.

| Workload | JIT normal | JIT O2 | AOT normal | AOT O2 |
| --- | ---: | ---: | ---: | ---: |
'''
for command, name in workloads.items():
    text += '| ' + name + ' | ' + ' | '.join(timing(command, selected) for selected in modes) + ' |\n'
text += '\nProcess CPU milliseconds, with the same ordering and sample range:\n\n| Workload | JIT normal | JIT O2 | AOT normal | AOT O2 |\n| --- | ---: | ---: | ---: | ---: |\n'
for command, name in workloads.items():
    text += '| ' + name + ' | ' + ' | '.join(timing(command, selected, True) for selected in modes) + ' |\n'

text += '\n## Operations and allocations\n\nA single value means all modes/samples agree. Otherwise values are in the four\nmode columns\' order above. Managed constructions count the existing construction\nprobe, not every string/buffer/native allocation. Retained output bytes include\nconsole capture and remaining managed files; deleted rotation history is excluded.\n\n| Workload | Managed constructions | Retained output B | Console calls | Native writes | Flushes | Metadata | Moves | Deletes |\n| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |\n'
for command, name in workloads.items():
    text += '| ' + name + ' | ' + ' | '.join(value(command, key) for key in ['managed_constructions', 'output_retained_bytes', 'console_calls', 'file_writes', 'flushes', 'metadata', 'moves', 'deletes']) + ' |\n'

text += '\n## Compilation and memory observations\n\n| Mode | Analysis/lower/JIT compile ms | Admission/preparation ms | Process high-water RSS range KiB |\n| --- | ---: | ---: | ---: |\n'
for selected in modes:
    subset = [r for r in rows if mode(r) == selected]
    compile_values = {r['compile_ns'] for r in subset}; prep = {r['preparation_ns'] for r in subset}
    assert len(compile_values) == len(prep) == 1
    rss = [r['process_high_water_rss_kib'] for r in subset]
    text += f'| {selected} | {next(iter(compile_values))/1e6:.2f} | {next(iter(prep))/1e6:.2f} | {min(rss)}–{max(rss)} |\n'
text += '''
AOT's zero compile column means compilation occurred in the earlier emitter/link
commands; it does not mean compilation was free. Their exact wall times, source
and executable hashes are in process receipts. JIT preparation includes its
compilation interval. RSS is the cumulative process high-water mark, including
compilation, warmups and prior workloads, not per-record live memory.

Async repeated-root memory snapshots, bytes and host handles (three repetitions):

| Mode / calls | Heap before | Heap at root return | Heap after explicit GC | Handles before → after |
| --- | ---: | ---: | ---: | --- |
'''
for selected in modes:
    for command, count in [(30, 100), (31, 1000)]:
        subset = sample(command, selected)
        field = lambda key: ', '.join(str(r[key]) for r in subset)
        text += f'| {selected} / {count} | {field("heap_before")} | {field("heap_transient_after_root")} | {field("heap_after_gc")} | ' + ', '.join(f'{r["handles_before"]}→{r["handles_after"]}' for r in subset) + ' |\n'
text += '''
These are explicit boundary snapshots, not a continuous peak-live-heap trace.
The existing async root retains task results/ordering records until root exit;
the logger adds no message queue. Post-GC payloads and host handles return to their
per-mode baseline across repeated roots. No constant-RSS or total-allocation-free
claim follows from that observation.

The measured costs are material. In JIT O2, 200 logged 64-byte file records took
265.32 ms versus 3.04 ms for direct io with identical bytes. The 64-byte, 1-KiB
and 65,500-byte cases cost approximately 20.7, 18.9 and 18.9 microseconds per input
byte, respectively: the bounded framing path scales approximately linearly here,
but has substantial overhead. These whole-path measurements do not isolate an
individual helper for a justified implementation optimization.

The JIT O2 async workload took 68.57 ms for 100 records and 3318.72 ms for 1000.
That is superlinear within this bounded range; no constant per-record async cost
is claimed. The measurement includes the established scheduler and root-lifetime
task retention and does not isolate their individual contributions. The repeated
post-root memory/handle baselines above show no unexplained retained growth.

The disabled cases perform no output. Eager custom display still runs 10,000 times;
both lazy bodies run zero times, while captured callback construction remains
visible. Ordinary file records do not perform per-record path metadata calls.
Rotation's counts grow with the exact configured backup set. The framing source
makes bounded linear UTF-8 passes and appends to one byte builder; larger-message
rows expose its actual cost, including caller construction. Direct console/io
rows use identical record bytes and show the framing/admission cost separately
from transport. No fixed throughput target or speculative Runtime optimization
was introduced. The source-generation and GC-policy choices remain reviewable
alongside the raw data rather than hidden behind a single throughput number.
'''
(root / 'docs/log/BENCHMARKS.md').write_text(text)
print('Rendered 216 normal-GC measurements and memory/operation observations')
