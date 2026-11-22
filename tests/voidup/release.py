"""Exercise the producer's catalog validity boundary against native limits."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
from cryptography.hazmat.primitives.serialization import Encoding, PublicFormat

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--producer', type=Path, default=ROOT/'tools/voidup_release.py')
a = p.parse_args()
with tempfile.TemporaryDirectory(prefix='voidup-release-boundary-') as directory:
    work = Path(directory)
    config = work/'public.json'
    config.write_text(json.dumps(dict(schema=1, trust='engineering', approval='disposable regression',
        catalog='https://localhost/catalog.json', keys=[dict(id=f'test-{n}', public=Ed25519PrivateKey.generate().public_key().public_bytes(Encoding.Raw, PublicFormat.Raw).hex()) for n in range(3)])))
    descriptor = work/'release.json'
    descriptor.write_text(json.dumps(dict(id='release', host='linux-x86_64-ubuntu24.04', selection=dict(version='1.0.0', channel='stable'))))
    for days in (30, 1, 7):
        output = work/f'catalog-{days}.json'
        subprocess.run([sys.executable, '-B', str(a.producer), 'catalog', '--public-config', str(config),
                        '--serial', '1', '--valid-days', str(days), '--release', str(descriptor), '--output', str(output)], check=True)
        catalog = json.loads(output.read_bytes())
        assert catalog['expires']-catalog['issued'] == days*86400 <= 2592000, catalog
print('PASS: 1/7/30-day catalogs fit the native validity interval')
