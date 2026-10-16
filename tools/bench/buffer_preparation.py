"""Read-only Stage 2C/current Release byte-buffer and new reference-buffer audit."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

from heap_legacy import ROOT, FLAGS, snapshot


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', default='cb601b4')
    parser.add_argument('--compiler', default='/usr/bin/clang++-19')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--cpu', type=int)
    args = parser.parse_args()
    if args.cpu is not None:
        os.sched_setaffinity(0, {args.cpu})
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    sha, old = snapshot(args.baseline, output)
    fixture = old / 'tests/runtime/buffers.cpp'
    fixture.parent.mkdir(parents=True, exist_ok=True)
    fixture.write_bytes(subprocess.check_output(['git', '-C', ROOT, 'show', sha + ':tests/runtime/buffers.cpp']))
    commands, hashes = [], {}
    for name, source in [('stage2c', old), ('stage2d', ROOT)]:
        sources = [source / 'tests/runtime/buffers.cpp'] + [source / ('src/runtime/' + part + '.cpp') for part in ('heap', 'gc_schema', 'coordination', 'buffer')]
        command = [args.compiler, *FLAGS, '-DVD_BUFFER_WRAP_MEMSET=1', '-I' + str(source / 'include'), *sources,
                   '-Wl,--wrap=memset', '-pthread', '-o', output / name]
        subprocess.run([str(x) for x in command], check=True)
        commands.append([str(x) for x in command])
        hashes[name] = {str(path.relative_to(source)): hashlib.sha256(path.read_bytes()).hexdigest()
                        for path in [*sources, *sorted((source / 'include/void').glob('*.hpp'))]}
    # Sequential samples, without a compiler/test workload competing for the CPU.
    for name in ('stage2c', 'stage2d'):
        with (output / (name + '-bytes.log')).open('w') as stream:
            subprocess.run([output / name, '--measure'], stdout=stream, check=True)
    with (output / 'stage2d-references.log').open('w') as stream:
        subprocess.run([output / 'stage2d', '--measure-reference'], stdout=stream, check=True)
    (output / 'identity.json').write_text(json.dumps({'baseline': sha, 'commands': commands, 'input_sha256': hashes,
        'compiler': subprocess.check_output([args.compiler, '--version'], text=True),
        'affinity': sorted(os.sched_getaffinity(0)) if hasattr(os, 'sched_getaffinity') else None}, indent=2) + '\n')
    print(output)


if __name__ == '__main__':
    main()
