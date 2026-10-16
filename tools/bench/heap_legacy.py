#!/usr/bin/env python3
"""Local Release Heap audit; read-only Git snapshots, no worktree or Git mutations."""
import argparse
import io
import hashlib
import json
import os
from pathlib import Path
import platform
import statistics
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[2]
FLAGS = ['-O3', '-DNDEBUG', '-std=c++20', '-frtti', '-Wall', '-Wextra', '-Wpedantic']


def run(command, **kwargs):
    return subprocess.check_output([str(x) for x in command], text=True, **kwargs)


def snapshot(ref, output):
    sha = run(['git', '-C', ROOT, 'rev-parse', '--verify', ref + '^{commit}']).strip()
    destination = output / sha
    destination.mkdir(parents=True, exist_ok=True)
    data = subprocess.check_output(['git', '-C', ROOT, 'archive', sha, 'include', 'src/runtime'])
    with tarfile.open(fileobj=io.BytesIO(data)) as archive:
        archive.extractall(destination, filter='data')
    return sha, destination


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--parent', default='3c69f66')
    parser.add_argument('--stage2b', default='cb77d62')
    parser.add_argument('--compiler', default='/usr/bin/clang++-19')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--objects', type=int, default=50000)
    parser.add_argument('--samples', type=int, default=7)
    parser.add_argument('--label', default='working')
    parser.add_argument('--cpu', type=int, help='pin builds and probes to this allowed logical CPU (Linux)')
    args = parser.parse_args()
    if args.cpu is not None:
        os.sched_setaffinity(0, {args.cpu})
    output = args.output.resolve();output.mkdir(parents=True, exist_ok=True)
    parent, parent_root = snapshot(args.parent, output)
    stage2b, stage2b_root = snapshot(args.stage2b, output)
    variants = [('parent', parent_root), ('stage2b', stage2b_root), (args.label, ROOT)]
    commands = []
    for label, source in variants:
        sources = [source / 'src/runtime/heap.cpp']
        if (source / 'src/runtime/gc_schema.cpp').exists():
            sources.append(source / 'src/runtime/gc_schema.cpp')
        for instrumented in (False, True):
            binary = output / (label + ('-counts' if instrumented else '-time'))
            command = [args.compiler, *FLAGS, '-I' + str(source / 'include'), ROOT / 'tools/bench/heap_legacy.cpp', *sources, '-o', binary]
            if instrumented:
                command.append('-DVD_BENCH_ALLOCATIONS=1')
            subprocess.run([str(x) for x in command], check=True)
            commands.append([str(x) for x in command])
    counts = {label: json.loads(run([output / (label + '-counts'), args.objects])) for label, _ in variants}
    samples = {label: [] for label, _ in variants}
    # Warm each variant, then rotate process order to reduce systematic thermal/order bias.
    for label, _ in variants:
        run([output / (label + '-time'), args.objects])
    for sample in range(args.samples):
        order = variants[sample % len(variants):] + variants[:sample % len(variants)]
        for label, _ in order:
            samples[label].append(json.loads(run([output / (label + '-time'), args.objects])))
    summary = {}
    for label, _ in variants:
        rows = []
        for index, metric in enumerate(counts[label]):
            row = dict(metric)
            for key in ('allocate_ns_per_object', 'live_gc_ns_per_object', 'dead_gc_ns_per_object'):
                values = [sample[index][key] for sample in samples[label]]
                row[key] = {'median': statistics.median(values), 'min': min(values), 'max': max(values)}
            rows.append(row)
        summary[label] = rows
    result = {'parent': parent, 'stage2b': stage2b, 'working_head': run(['git', '-C', ROOT, 'rev-parse', 'HEAD']).strip(),
              'compiler': run([args.compiler, '--version']), 'platform': platform.platform(),
              'cpu': platform.processor(), 'cpu_affinity': sorted(os.sched_getaffinity(0)) if hasattr(os, 'sched_getaffinity') else None,
              'input_sha256': {label: {str(path.relative_to(source)): hashlib.sha256(path.read_bytes()).hexdigest()
                                     for directory in ('include', 'src/runtime') for path in sorted((source / directory).rglob('*')) if path.is_file()}
                               for label, source in variants},
              'harness_sha256': hashlib.sha256((ROOT / 'tools/bench/heap_legacy.cpp').read_bytes()).hexdigest(),
              'commands': commands, 'objects': args.objects, 'samples': args.samples,
              'summary': summary, 'raw_samples': samples}
    (output / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
