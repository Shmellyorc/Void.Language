from hashlib import sha256
from pathlib import Path
import json

root = Path(__file__).resolve().parent
share = Path.home()/'VM/project-slice7-g4-checkpoint'
outputs = (
    'vcvars.log', 'windows-build.log', 'windows-clean.log', 'windows-collision.log',
    'windows-replace.log', 'windows-status.txt', 'redirection-vcvars.log',
    'redirection-build.log', 'redirection-registry.log', 'redirection-baseline.log',
    'redirection-bare.log', 'redirection-manifested.log', 'redirection-status.txt',
    'activation-vcvars.log', 'activation-build.log', 'activation-override.log',
    'activation-default.log', 'activation-status.txt', 'edge-vcvars.log',
    'edge-build.log', 'edge-override.log', 'edge-default.log', 'edge-status.txt',
    'process-default-vcvars.log', 'process-default-build.log', 'process-default-run.log',
    'process-default-status.txt',
)
inputs = (
    'dependency.c', 'provider.c', 'windows-mapping.c', 'windows-probe.cmd',
    'redirected.c', 'windows-redirection.c', 'windows-redirection.cmd',
    'windows-activation.c', 'override.manifest', 'windows-activation.cmd',
    'windows-activation-edge.c', 'override-dependency.manifest',
    'windows-activation-edge.cmd', 'windows-process-default.cmd',
)
hashes = {}
for name in outputs + inputs:
    payload = (share/name).read_bytes()
    target = root/Path(name).with_suffix('.txt') if name in outputs else root/name
    if target.exists() or name in inputs:
        assert target.read_bytes() == payload, target
    else:
        target.write_bytes(payload)
    hashes[name] = sha256(payload).hexdigest()
assert b'result=134' in (root/'windows-collision.txt').read_bytes()
assert b'initializer-marker=1' in (root/'edge-override.txt').read_bytes()
assert b'kernel-oracle-dependency=c00000d4' in (root/'process-default-run.txt').read_bytes()
assert b'process-default=1' in (root/'process-default-status.txt').read_bytes()
for target in (share/'preserved-evidence.json', root/'preserved-evidence.json'):
    target.write_text(json.dumps(hashes,indent=2)+'\n')
print(f'Preserved {len(hashes)} exact native evidence inputs and outputs.')
