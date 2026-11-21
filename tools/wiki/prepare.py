"""Prepare an exact, local-only Wiki export. Never contacts or publishes to GitHub."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'docs/wiki'
WIKI = 'https://github.com/Shmellyorc/Void.Language/wiki/'
BASELINE = '553fe3f028ca3ce55d950c0bfe61b8d4503f13b3'


def inventory():
    return {
        'schema': 1,
        'source_baseline': BASELINE,
        'wiki': WIKI.rstrip('/'),
        'files': [dict(file=p.name, sha256=hashlib.sha256(p.read_bytes()).hexdigest(),
                       role='shared' if p.stem.startswith('_') else 'page',
                       route=None if p.stem.startswith('_') else WIKI + p.stem)
                  for p in sorted(SOURCE.glob('*.md')) if p.name != 'README.md'],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--refresh-inventory', action='store_true', help='Update repository-only page hashes after an intentional edit')
    mode.add_argument('--output', type=Path, help='Create a fresh private export directory with pages/ and publication.json')
    args = parser.parse_args()
    current = inventory()
    manifest = SOURCE / 'publication.json'
    if args.refresh_inventory:
        manifest.write_text(json.dumps(current, indent=2) + '\n')
        print(f'Inventory refreshed: {len(current["files"])} publication files')
        return
    if json.loads(manifest.read_text()) != current:
        parser.error('Page bytes differ from publication.json; review edits and refresh the inventory first')
    destination = args.output.resolve()
    destination.mkdir(mode=0o700, parents=False, exist_ok=False)
    (destination / 'pages').mkdir()
    for row in current['files']:
        (destination / 'pages' / row['file']).write_bytes((SOURCE / row['file']).read_bytes())
    (destination / 'publication.json').write_text(json.dumps(current, indent=2) + '\n')
    print(f'Prepared {len(current["files"])} files in {destination / "pages"}; nothing published')


if __name__ == '__main__':
    main()
