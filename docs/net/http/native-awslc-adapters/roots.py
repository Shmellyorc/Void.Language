"""Read-only public Windows anchor/replacement control, without a TLS identity."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

from study import HERE, OUT, inventory, sha


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--unrelated-ca', type=Path, required=True,
                        help='Public PEM for a disposable CA absent from Windows ROOT')
    args = parser.parse_args()
    before = inventory()
    for profile in ('Debug', 'Release'):
        executable = OUT / profile / 'root-audit.exe'
        result = subprocess.run([str(executable), str(args.unrelated_ca.resolve())],
                                capture_output=True, text=True, timeout=30)
        after = inventory()
        record = {'profile': profile, 'exit': result.returncode,
                  'records': [json.loads(line) for line in result.stdout.splitlines()],
                  'error': result.stderr.strip(), 'inventory_before': before,
                  'inventory_after': after, 'root_audit_sha256': sha(executable),
                  'public_unrelated_ca_sha256': sha(args.unrelated_ca),
                  'source_sha256': {name: sha(HERE / name)
                                    for name in ('roots.cpp', 'common.hpp', 'roots.py')}}
        dest = OUT / (profile + '-roots.json')
        if dest.exists():
            shutil.copyfile(dest, OUT / ('earlier-' + dest.stem + '-' + sha(dest)[:12] + '.json'))
        dest.write_text(json.dumps(record, indent=2) + '\n')
        assert before == after, 'Read-only anchor control changed credential inventory'
        assert result.returncode == 0, 'Anchor replacement not established; receipt retained'
        print(profile, 'public default anchor accepted; unrelated explicit CA replaces it')


if __name__ == '__main__':
    main()
