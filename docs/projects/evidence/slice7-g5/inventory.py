"""Regenerate the exact Gate 5 source patch and verify the layered working tree."""
from collections import Counter
from difflib import unified_diff
from hashlib import sha256
from pathlib import Path
import json
import os
import subprocess

workspace = Path.cwd()
evidence = Path(__file__).resolve().parent.relative_to(workspace)
work = Path('build/slice7-g5')
base = work / 'patch-base'
report = Path('docs/projects/SLICE7_G5_CALLBACKS.md')
research = Path('docs/projects/C_CALLBACK_RESEARCH.md')
patch = evidence / 'source.patch'
inventory = evidence / 'inventory.txt'
opening = json.loads((work / 'baseline.json').read_text())


def git(*args):
    return subprocess.check_output(['git', '--no-optional-locks', *args])


def digest(path):
    return sha256(Path(path).read_bytes()).hexdigest()


previous = {}
prior = Path('docs/projects/evidence/slice7-raylib/inventory.txt')
for row in prior.read_text().splitlines():
    fields = row.split(' | ', 4)
    if len(fields) == 5 and fields[0] in (' M', '??'):
        previous[fields[2]] = fields[1]

head = git('rev-parse', 'HEAD').decode().strip()
index = git('ls-files', '--stage', '-z')
index_hash = sha256(index).hexdigest()
assert head == (work / 'head').read_text().strip()
assert index_hash == (work / 'index.sha256').read_text().strip()
assert not git('diff', '--cached', '--name-only', '-z')
git('diff', '--check')

before_hashes = {}
changed = set()
for file in base.rglob('*'):
    if not file.is_file():
        continue
    name = file.relative_to(base).as_posix()
    before = opening[name][1] if name in opening else sha256(git('show', f'{head}:{name}')).hexdigest()
    assert digest(file) == before, name
    before_hashes[name] = before
    if digest(name) != before:
        changed.add(name)
fixtures = {p.as_posix() for p in Path('tests/ffi').glob('callback*') if p.is_file()}
assert len(fixtures) == 7 and not fixtures & opening.keys()
diff = []
for name in sorted(changed | fixtures):
    diff.append(f'diff --git a/{name} b/{name}\n')
    if name in changed:
        label = f'a/{name}'
        original = (base / name).read_bytes().decode().splitlines(keepends=True)
    else:
        diff.append('new file mode 100644\n')
        label, original = '/dev/null', []
    revised = Path(name).read_bytes().decode().splitlines(keepends=True)
    diff.extend(unified_diff(original, revised, label, f'b/{name}'))
patch.write_text(''.join(diff))
git('apply', '--reverse', '--check', str(patch))

entries = git('status', '--porcelain=v1', '-z', '--untracked-files=all').split(b'\0')[:-1]
current = {os.fsdecode(row[3:]): row[:2].decode() for row in entries}
current[str(inventory)] = '??'
deliverables = {p.as_posix() for p in evidence.rglob('*') if p.is_file()} | {str(inventory), str(report)}
research_name = str(research)
expected = opening.keys() | changed | fixtures | deliverables | {research_name}
assert current.keys() == expected, (current.keys() - expected, expected - current.keys())
counts = Counter(current.values())
summary = dict(opening_paths=len(opening), amended_sources=len(changed), fixtures=len(fixtures),
               evidence=len(deliverables), modified=counts[' M'], untracked=counts['??'])
rows = [
    f'HEAD {head}', f'Index tuple SHA-256 {index_hash}',
    'HEAD and index match the Gate 5 opening snapshot. Staged diff is empty.',
    'Prior working-tree bytes match the opening snapshot outside the listed Gate 5 source edits.',
    json.dumps(summary, sort_keys=True), '', 'STATUS | LAYER | PATH | OPENING SHA-256 | FINAL SHA-256',
]
for name, flag in sorted(current.items()):
    before = opening[name][1] if name in opening else before_hashes.get(name, '-')
    after = '(self)' if name == str(inventory) else digest(name)
    if name in opening:
        assert flag == opening[name][0] and (before != after) == (name in changed), name
        layer = previous.get(name, 'pre-existing working tree')
        if name in changed:
            layer += ' + S7 G5'
    elif name in changed:
        assert flag == ' M', name
        layer = 'HEAD + S7 G5'
    else:
        assert flag == '??', name
        if name == research_name:
            layer = 'Independent Research Clanker'
        elif name in fixtures:
            layer = 'S7 G5 fixture'
        else:
            layer = 'S7 G5 report/evidence'
    rows.append(f'{flag} | {layer} | {name} | {before} | {after}')
inventory.write_text('\n'.join(rows) + '\n')
print(summary)
