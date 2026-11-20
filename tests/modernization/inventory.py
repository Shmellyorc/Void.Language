"""Keep the complete G88 audit, public docs and provider enum parity reviewable."""
from pathlib import Path
import json
import hashlib
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/raylib'))
from enums import verify_enums

report = (ROOT / 'docs/projects/G88_STDLIB_PACKAGES_MODERNIZATION.md').read_text()
files = sorted(p for name in ('stdlib', 'verified-packages') for p in (ROOT / name).rglob('*') if p.is_file())
for file in files:
    assert '`' + str(file.relative_to(ROOT)) + '`' in report, file
for file in (ROOT / 'stdlib').rglob('*.void'):
    lines = file.read_text().splitlines()
    for index, line in enumerate(lines):
        if re.match(r'^(?:async )?(?:func |property |(?:value )?struct |(?:flags )?enum |const )', line):
            assert index and lines[index - 1].startswith('##'), (file, index + 1, line)
source = (ROOT / 'verified-packages/raylib/src/raylib.void').read_text()
entries = json.loads((ROOT / 'tools/raylib/inventory.json').read_text())['enumerations']
verify_enums(source, entries)
for before, after in [('window_hidden = 128', 'window_hidden = 129'),
                      ('enum keyboard_key(i32)', 'enum keyboard_key(u32)'),
                      ('enum camera_mode(i32)', 'enum removed_camera_mode(i32)')]:
    assert before in source
    try:
        verify_enums(source.replace(before, after, 1), entries)
    except ValueError:
        pass
    else:
        raise AssertionError(('accepted mismatched provider enum', before))
bible = ROOT / 'docs/LANGUAGE_BIBLE.md'
for base, text in ((bible.parent, bible.read_text()), (ROOT / 'docs/projects', report)):
    for link in re.findall(r'\]\(([^\s)]+)\)', text):
        if '://' in link or link.startswith('#'):
            continue
        assert (base / link.split('#')[0]).is_file(), link
history = (ROOT / 'docs/LANGUAGE_BIBLE_HISTORY.md').read_text()
assert '9b7bbff41ba9a49ebc3d35a01fe77a7b5103ee77' in history
assert hashlib.sha256(history.split('\n---\n\n', 1)[1].encode()).hexdigest() == '3e3eeb7ea8af58b93d2a85a6e630c8960b5d0001bb92794d39edbd2861a50454'
print(f'G88 inventory: {len(files)} files, public declaration docs, canonical links and 3 provider parity refusals passed')
