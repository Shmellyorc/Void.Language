"""Freeze the pre-repair package and repeat its exact benchmark, without builds.

Run once before production edits. The original sources, evidence and executable
bytes remain available even after the build tree is rebuilt. No ptrace is used.
"""
from pathlib import Path
import json
import os
import platform
import shutil
import subprocess
import tarfile
import tempfile
import time
import support as h

out = h.EVIDENCE / 'performance-repair' / 'baseline'
assert not out.exists(), 'Baseline is immutable; use the existing package'
out.mkdir(parents=True)
paths = [h.ROOT / p for p in ('src', 'include', 'stdlib', 'tests/log',
         'tests/io', 'tests/managed', 'tests/text_encoding', 'CMakeLists.txt',
         'README.md')]
paths += list((h.ROOT / 'docs/log').glob('*.md'))
manifest = {}
with tarfile.open(out / 'sources.tar.gz', 'w:gz') as archive:
    for path in paths:
        archive.add(path, arcname=str(path.relative_to(h.ROOT)))
        for file in ([path] if path.is_file() else path.rglob('*')):
            if file.is_file():
                manifest[str(file.relative_to(h.ROOT))] = h.crash.digest(file)
with tarfile.open(out / 'original-evidence.tar.gz', 'w:gz') as archive:
    for path in h.EVIDENCE.iterdir():
        if path.name != 'performance-repair':
            archive.add(path, arcname=path.name)
build = h.ROOT / 'build/ci-official-release'
executables = [build / (name + '-benchmark-default') for name in (
    'void_log_host', 'void_log_probe_normal', 'void_log_probe_optimized')]
for exe in executables:
    shutil.copy2(exe, out / exe.name)
    manifest[str(exe.relative_to(h.ROOT))] = h.crash.digest(exe)
for filename in ('CMakeCache.txt', 'void-native.txt'):
    shutil.copy2(build / filename, out / filename)
metadata = dict(source_and_executable_hashes=manifest, utc=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
                platform=platform.platform(), gc_allocation_budget=256,
                head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
                cpu_affinity=sorted(os.sched_getaffinity(0)),
                cache='direct Jit.compile; no persistent cache; AOT already emitted and linked',
                policy='No compilation or validation runs concurrent with measurements; desktop remains active')
(out / 'freeze.json').write_text(json.dumps(metadata, indent=2) + '\n')
(out / 'processes-before.txt').write_bytes(subprocess.check_output(['ps', '-eo', 'pid,pcpu,args']))
rows = []
for exe in executables:
    # Use the frozen executable bytes. The JIT still sees the verified unchanged
    # project sources; source hashes are checked again after execution.
    frozen = out / exe.name
    with tempfile.TemporaryDirectory(prefix='VOID-log-isolated-') as directory:
        result = h.run([frozen, h.ROOT, directory, '--benchmark'], small=True,
                       timeout=1800, sources=[out],
                       context={'mode': 'isolated-original-benchmark', 'freeze': str(out / 'freeze.json')})
    (out / (exe.name + '.stdout')).write_bytes(result.stdout)
    for line in result.stdout.decode().splitlines():
        if line.startswith('BENCH '):
            row = json.loads(line[6:])
            row.update(executable=str(frozen), executable_sha256=h.crash.digest(frozen),
                       gc_allocation_budget=256)
            rows.append(row)
    (out / 'isolated-samples.json').write_text(json.dumps(rows, indent=2) + '\n')
    print('Frozen isolated benchmark completed:', exe.name, flush=True)
for path, digest in manifest.items():
    assert h.crash.digest(h.ROOT / path) == digest, path
assert len(rows) == 216
(out / 'processes-after.txt').write_bytes(subprocess.check_output(['ps', '-eo', 'pid,pcpu,args']))
print('216 original-workload samples; all frozen source/executable hashes unchanged', flush=True)
