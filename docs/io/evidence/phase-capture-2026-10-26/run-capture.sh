#!/bin/bash
set -euo pipefail
python3 - <<'CHECK'
from pathlib import Path
import json,hashlib
root=Path('/tmp/void-io-phase-capture-20261026-6vfao69v');m=json.loads((root/'manifest.json').read_text())
for key in ('runner','fixture'):
    assert hashlib.sha256(Path(m[key]).read_bytes()).hexdigest()==m[key+'_sha256'],key+' changed; refusing stale breakpoints'
assert not (root/'results.json').exists(),'Batch already ran; do not silently extend the attempt limit'
print('Verified unchanged runner and reconstructed fixture. Four remaining attempts; ASLR enabled.',flush=True)
CHECK
timeout --signal=INT --kill-after=30s 13m gdb -q -nx -batch -x /tmp/void-io-phase-capture-20261026-6vfao69v/capture.gdb --args /home/shmellyorc/Documents/Void.Language/build/ci-official-release/void_io_runner --async '/tmp/VOID io async wire 1icf1laa/main.void' 2>&1 | tee /tmp/void-io-phase-capture-20261026-6vfao69v/session.log
