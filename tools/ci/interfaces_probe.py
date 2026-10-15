"""Controlled, isolated interface-suite repeats; crashes never count as passes.

Each child uses the full suite and unchanged assertions, with a 90-second driver
timeout counted as failure. Core files, when
permitted by the OS, are isolated in that child's working directory. No retry
silently converts a failing batch to a successful batch.
"""
import argparse
import concurrent.futures
import json
from pathlib import Path
import resource
import subprocess
import time


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--binary', required=True, type=Path)
    p.add_argument('--root', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    p.add_argument('--serial', type=int, default=6)
    p.add_argument('--parallel', type=int, default=8)
    p.add_argument('--workers', type=int, default=2)
    a = p.parse_args()
    if min(a.serial, a.parallel) < 0 or a.workers < 1:
        p.error('invalid repeat counts')
    out = a.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    binary, root = a.binary.resolve(), a.root.resolve()
    records = []
    # Configure inheritance before spawning worker threads. A Python preexec_fn
    # in a threaded parent can deadlock before exec and before run's timeout.
    _, hard = resource.getrlimit(resource.RLIMIT_CORE)
    cap = 256 * 1024 * 1024
    core_bytes = cap if hard == resource.RLIM_INFINITY else min(cap, hard)
    resource.setrlimit(resource.RLIMIT_CORE, (core_bytes, hard))

    def run(label):
        directory = out / label
        directory.mkdir()

        start = time.monotonic()
        try:
            result = subprocess.run([str(binary), str(root)], cwd=directory, capture_output=True,
                                    text=True, encoding='utf-8', errors='strict', timeout=90)
            status, stdout, stderr = result.returncode, result.stdout, result.stderr
        except subprocess.TimeoutExpired as error:
            status, stdout, stderr = None, str(error.stdout), str(error.stderr)
        (directory / 'stdout.log').write_text(stdout)
        (directory / 'stderr.log').write_text(stderr)
        return dict(label=label, returncode=status, seconds=round(time.monotonic()-start, 3),
                    last_progress=stderr.splitlines()[-3:], cores=[f.name for f in directory.glob('core*')])

    def save(record):
        records.append(record)
        print(json.dumps(record), flush=True)
        (out / 'results.json').write_text(json.dumps(records, indent=2) + '\n')

    for n in range(a.serial):
        save(run('serial-' + str(n)))
    with concurrent.futures.ThreadPoolExecutor(max_workers=a.workers) as pool:
        for r in pool.map(run, ['parallel-' + str(n) for n in range(a.parallel)]):
            save(r)
    summary = dict(binary=str(binary), root=str(root), workers=a.workers,
                   cases=len(records), core_bytes=core_bytes, passed=all(r['returncode'] == 0 for r in records))
    (out / 'complete.json').write_text(json.dumps(summary, indent=2) + '\n')
    raise SystemExit(0 if summary['passed'] else 1)


if __name__ == '__main__':
    main()
