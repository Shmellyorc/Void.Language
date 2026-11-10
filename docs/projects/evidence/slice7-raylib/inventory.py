"""Verify the raylib layer against the retained opening snapshot and regenerate its patch."""
from collections import Counter
from difflib import unified_diff
from hashlib import sha256
from pathlib import Path
import json
import os
import subprocess

root = Path.cwd()
evidence = Path(__file__).resolve().parent.relative_to(root)
work = Path('build/slice7-raylib')
report = Path('docs/projects/SLICE7_RAYLIB_INTEGRATION.md')
inventory = evidence / 'inventory.txt'
patch = evidence / 'source.patch'
patch_base = work / 'patch-base'
baseline = json.loads((work / 'baseline.json').read_text())
prior = Path('docs/projects/evidence/slice7-g4-conventional/inventory.txt')
layers = {}
for row in prior.read_text().splitlines():
    columns = row.split(' | ', 4)
    if len(columns) == 5 and columns[0] in (' M', '??'):
        _, layer, name, _, _ = columns
        layers[name] = layer


def git(*args):
    return subprocess.check_output(['git', '--no-optional-locks', *args])


def digest(path):
    return sha256(Path(path).read_bytes()).hexdigest()


head = git('rev-parse', 'HEAD').decode().strip()
index = git('ls-files', '--stage', '-z')
index_hash = sha256(index).hexdigest()
assert head == (work / 'head').read_text().strip()
assert index_hash == (work / 'index.sha256').read_text().split()[0]
assert not git('diff', '--cached', '--name-only', '-z')
git('diff', '--check')

changed = {p.relative_to(patch_base).as_posix() for p in patch_base.rglob('*') if p.is_file()}
sample = {p.as_posix() for p in Path('examples/raylib').rglob('*') if p.is_file()}
assert len(changed) == 10
assert len(sample) == 6 and not sample & baseline.keys()
diff = []
for name in sorted(changed | sample):
    diff.append(f'diff --git a/{name} b/{name}\n')
    if name in changed:
        before = patch_base / name
        opening = baseline[name][1] if name in baseline else sha256(git('show', f'{head}:{name}')).hexdigest()
        assert digest(before) == opening, name
        original = before.read_bytes().decode().splitlines(keepends=True)
        label = f'a/{name}'
    else:
        diff.append('new file mode 100644\n')
        original = []
        label = '/dev/null'
    after = Path(name).read_bytes().decode().splitlines(keepends=True)
    diff.extend(unified_diff(original, after, label, f'b/{name}'))
patch.write_text(''.join(diff))
git('apply', '--reverse', '--check', str(patch))

entries = git('status', '--porcelain=v1', '-z', '--untracked-files=all').split(b'\0')[:-1]
current = {os.fsdecode(row[3:]): row[:2].decode() for row in entries}
current[str(inventory)] = '??'
deliverables = {str(p) for p in evidence.rglob('*') if p.is_file()} | {str(report), str(inventory)}
assert current.keys() == baseline.keys() | changed | sample | deliverables, (
    current.keys() - (baseline.keys() | changed | sample | deliverables),
    (baseline.keys() | changed | sample | deliverables) - current.keys(),
)
counters = Counter(current.values())
summary = dict(index_entries=index.count(b'\0'), modified=counters[' M'], untracked=counters['??'],
               original_paths=len(baseline), changed=len(changed), sample=len(sample),
               report_evidence=len(deliverables))
header = [
    f'HEAD {head}', f'Index tuple SHA-256 {index_hash}',
    'HEAD and index match the task opening snapshot. Staged diff is empty.',
    'Previous working-tree content matches the opening snapshot outside the listed raylib source edits.',
    json.dumps(summary, sort_keys=True),
    '', 'STATUS | LAYER | PATH | OPENING SHA-256 | FINAL SHA-256',
]
for name, flag in sorted(current.items()):
    before = baseline[name][1] if name in baseline else digest(patch_base / name) if name in changed else '-'
    after = '(self)' if name == str(inventory) else digest(name)
    if name in baseline:
        assert flag == baseline[name][0], name
        assert (before != after) == (name in changed), name
        layer = layers.get(name, 'pre-existing working tree')
        if name in changed:
            layer += ' + S7 raylib affinity'
    elif name in changed:
        assert flag == ' M' and before != after, name
        layer = 'HEAD + S7 raylib affinity'
    else:
        assert flag == '??', name
        layer = 'S7 raylib example' if name in sample else 'S7 raylib report/evidence'
    header.append(f'{flag} | {layer} | {name} | {before} | {after}')
inventory.write_text('\n'.join(header) + '\n')
print(summary)
