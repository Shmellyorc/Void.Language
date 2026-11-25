"""Prepare an exact, local-only Wiki export. Never contacts or publishes to GitHub."""
import argparse
import difflib
import subprocess
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'docs/wiki'
WIKI = 'https://github.com/Shmellyorc/Void.Language/wiki/'
BASELINE = 'cc89906a2133eeead625b7167d1881562d1ef1bd'


def inventory():
    return {
        'schema': 1,
        'source_baseline': BASELINE,
        'wiki': WIKI.rstrip('/'),
        'files': [dict(file=p.name, sha256=hashlib.sha256(p.read_bytes()).hexdigest(),
                       role='shared' if p.stem.startswith('_') else 'page',
                       route=None if p.stem.startswith('_') else WIKI + p.stem)
                  for p in sorted(SOURCE.glob('*.md'), key=lambda page: page.name) if p.name != 'README.md'],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--refresh-inventory', action='store_true', help='Update repository-only page hashes after an intentional edit')
    mode.add_argument('--output', type=Path, help='Create a fresh private export directory with pages/ and publication.json')
    parser.add_argument('--live-snapshot', type=Path,
                        help='Compare a clean, read-only Wiki clone; emit preimages and review.diff beside the export')
    args = parser.parse_args()
    if args.live_snapshot and args.refresh_inventory:
        parser.error('--live-snapshot requires --output')
    current = inventory()
    manifest = Path(__file__).resolve().parent / 'publication.json'
    if args.refresh_inventory:
        manifest.write_text(json.dumps(current, indent=2) + '\n')
        print(f'Inventory refreshed: {len(current["files"])} publication files')
        return
    if json.loads(manifest.read_text()) != current:
        parser.error('Page bytes differ from publication.json; review edits and refresh the inventory first')
    review = None
    patch = ''
    if args.live_snapshot:
        snapshot = args.live_snapshot.resolve()
        def git(*operands):
            return subprocess.check_output(['git', '-C', str(snapshot), *operands], text=True).strip()
        if git('status', '--porcelain'):
            parser.error('Live snapshot must be clean; retain it unchanged for review')
        preimages, changed = {}, []
        for row in current['files']:
            old = snapshot / row['file']
            if old.is_symlink() or (old.exists() and not old.is_file()):
                parser.error('Refusing non-regular live page: ' + row['file'])
            before = old.read_bytes() if old.exists() else b''
            preimages[row['file']] = hashlib.sha256(before).hexdigest() if old.exists() else None
            after = (SOURCE / row['file']).read_bytes()
            if before != after:
                changed.append(row['file'])
                patch += ''.join(difflib.unified_diff(before.decode('utf-8').splitlines(True),
                    after.decode('utf-8').splitlines(True), fromfile='a/' + row['file'], tofile='b/' + row['file']))
        review = dict(head=git('rev-parse', 'HEAD'), branch=git('branch', '--show-current'),
                      preimages=preimages, changed=changed,
                      preserved_unknown_files=sorted(p.relative_to(snapshot).as_posix() for p in snapshot.rglob('*')
                          if p.is_file() and '.git' not in p.relative_to(snapshot).parts and p.relative_to(snapshot).as_posix() not in preimages),
                      publication_performed=False)
    destination = args.output.resolve()
    destination.mkdir(mode=0o700, parents=False, exist_ok=False)
    (destination / 'pages').mkdir()
    for row in current['files']:
        (destination / 'pages' / row['file']).write_bytes((SOURCE / row['file']).read_bytes())
    (destination / 'publication.json').write_text(json.dumps(current, indent=2) + '\n')
    if review is not None:
        (destination / 'live-review.json').write_text(json.dumps(review, indent=2) + '\n')
        (destination / 'review.diff').write_text(patch)
    print(f'Prepared {len(current["files"])} files in {destination / "pages"}; nothing published')


if __name__ == '__main__':
    main()
