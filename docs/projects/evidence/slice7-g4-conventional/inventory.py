"""Verify the unstaged Gate 4 layer and regenerate its source patch and inventory."""
from collections import Counter
from difflib import unified_diff
from hashlib import sha256
from pathlib import Path
import json
import os
import subprocess

root = Path.cwd()
evidence = Path(__file__).resolve().parent.relative_to(root)
work = Path('build/slice7-g4-conventional')
report = Path('docs/projects/SLICE7_G4_CONVENTIONAL_FFI.md')
inventory = evidence / 'inventory.txt'
patch = evidence / 'source.patch'
baseline = json.loads((work / 'baseline.json').read_text())
original_inventory = Path('docs/projects/evidence/slice7-g4-w2/inventory.txt')
layers = {}
for line in original_inventory.read_text().splitlines()[7:]:
    if line:
        _, layer, name, _ = line.split(' | ', 3)
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
assert git('diff', '--cached', '--name-only', '-z') == b''
git('diff', '--check')
changed = {
    'CMakeLists.txt', 'include/void/c_dynamic.hpp', 'include/void/c_primitive.hpp',
    'include/void/frontend.hpp', 'include/void/project.hpp', 'src/dynamic_closure.cpp',
    'src/frontend/compiler.cpp', 'src/frontend/project.cpp', 'src/frontend/project_selection.inc',
    'src/ir/c_primitive.cpp', 'src/main.cpp', 'src/native_build.cpp',
    'src/runtime/abi.cpp', 'src/runtime/c_dynamic.cpp',
}
fixtures = {str(path) for path in Path('tests/ffi').glob('conventional*') if path.is_file()}
assert len(fixtures) == 7 and not fixtures & baseline.keys()
diff = []
for name in sorted(changed | fixtures):
    before = work / 'patch-base' / name
    after = Path(name)
    diff.append(f'diff --git a/{name} b/{name}\n')
    if name in changed:
        assert digest(before) == baseline[name][1], name
        original = before.read_bytes().decode().splitlines(keepends=True)
        label = f'a/{name}'
    else:
        diff.append('new file mode 100644\n')
        original = []
        label = '/dev/null'
    diff.extend(unified_diff(original, after.read_bytes().decode().splitlines(keepends=True), label, f'b/{name}'))
patch.write_text(''.join(diff))
git('apply', '--reverse', '--check', str(patch))
entries = git('status', '--porcelain=v1', '-z', '--untracked-files=all').split(b'\0')[:-1]
current = {os.fsdecode(row[3:]): row[:2].decode() for row in entries}
current[str(inventory)] = '??'
new_evidence = {str(path) for path in evidence.rglob('*') if path.is_file()} | {str(report), str(inventory)}
assert current.keys() == baseline.keys() | fixtures | new_evidence
for name in current.keys() - baseline.keys():
    assert current[name] == '??'
counters = Counter(current.values())
header = [
    f'HEAD {head}', f'Index tuple SHA-256 {index_hash}',
    'HEAD and index match the task opening snapshot. Staged diff is empty.',
    f'{len(baseline)} opening paths. Prior content is unchanged outside the listed conventional FFI sources.',
    json.dumps(dict(index_entries=index.count(b'\0'), modified=counters[' M'], untracked=counters['??'],
                    original_paths=len(baseline), changed=len(changed), fixtures=len(fixtures),
                    report_evidence=len(new_evidence)), sort_keys=True),
    '', 'STATUS | LAYER | PATH | OPENING SHA-256 | FINAL SHA-256',
]
for name, flag in sorted(current.items()):
    before = baseline[name][1] if name in baseline else '-'
    after = '(self)' if name == str(inventory) else digest(name)
    if name in baseline:
        assert flag == baseline[name][0]
        assert (before != after) == (name in changed), name
        layer = layers.get(name, 'pre-existing working tree')
        if name in changed:
            layer += ' + S7 G4 conventional'
    else:
        layer = 'S7 G4 conventional fixtures' if name in fixtures else 'S7 G4 conventional report/evidence'
    header.append(f'{flag} | {layer} | {name} | {before} | {after}')
inventory.write_text('\n'.join(header) + '\n')
print(header[4])
