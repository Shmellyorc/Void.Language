from collections import Counter
from hashlib import sha256
import json
import os
from pathlib import Path
import subprocess

repo = Path.cwd()
evidence = Path('docs/projects/evidence/slice7-g4-windows')
report = Path('docs/projects/SLICE7_G4_WINDOWS_ARCHITECTURE.md')
previous = Path('docs/projects/evidence/slice7-g4-linux/inventory.txt')
inventory = evidence / 'inventory.txt'

def git(*args):
    return subprocess.check_output(['git', '--no-optional-locks', *args], cwd=repo)

def digest(path):
    return sha256(path.read_bytes()).hexdigest()

head = git('rev-parse', 'HEAD').decode().strip()
assert head == '2f667ea338127b8f91c3136804377ef3fa5218d6'
index = git('ls-files', '--stage', '-z')
index_hash = sha256(index).hexdigest()
assert index_hash == 'c9b3a3ca23a36c7c11fac5839d50c9f3d07311f5ec61f8577182f1abb0715fc0'
assert git('diff', '--cached', '--name-only', '-z') == b''
tree = []
for entry in git('ls-tree', '-rz', 'HEAD').split(b'\0')[:-1]:
    spec, name = entry.split(b'\t', 1)
    mode, _, object_id = spec.split()
    tree.append((name, mode + b' ' + object_id + b' 0\t' + name + b'\0'))
assert index == b''.join(row for _, row in sorted(tree))

assert digest(previous) == 'e3f8814f04a7a9d5c13960570ae2c5e9e3d083c98b027cab3f1bf592cbd5b866'
starting = {}
for line in previous.read_text().splitlines()[7:]:
    if line:
        flag, layer, name, initial_hash = line.split(' | ', 3)
        assert name not in starting
        starting[name] = (flag, layer, initial_hash)
assert len(starting) == 1737
entries = git('status', '--porcelain=v1', '-z', '--untracked-files=all').split(b'\0')[:-1]
assert all(entry[:2] in (b' M', b'??') and entry[2:3] == b' ' for entry in entries)
current = {os.fsdecode(entry[3:]): entry[:2].decode('ascii') for entry in entries}
current[str(inventory)] = '??'
new_evidence = {str(path) for path in evidence.rglob('*') if path.is_file()} | {str(report), str(inventory)}
assert set(current) == set(starting) | new_evidence
assert all(current[name] == '??' for name in new_evidence)

counters = Counter(current.values())
git('diff', '--check')
git('apply', '--reverse', '--check', str(evidence/'probe.patch'))
header = [
    f'HEAD {head}', f'Index tuple SHA-256 {index_hash}',
    'Index tuples match the prior audit checkpoint and HEAD. Staged diff is empty.',
    f'All {len(starting):,} previous working-tree paths retained. No production paths changed.',
    json.dumps({'index_entries': len(tree), 'modified': counters[' M'], 'untracked': counters['??'],
                'paths': len(current), 'starting_paths': len(starting),
                'production_changes': 0, 'g4w_report_evidence_paths': len(new_evidence)}, sort_keys=True),
    '', 'STATUS | LAYER | EXACT PATH | SHA-256'
]
for name, flag in sorted(current.items()):
    value = '(self)' if name == str(inventory) else digest(Path(name))
    if name in starting:
        prior_flag, layer, initial_hash = starting[name]
        assert flag == prior_flag, name
        if initial_hash != '(self)':
            assert value == initial_hash, name
    else:
        layer = 'S7 G4-W architecture report/evidence'
    header.append(f'{flag} | {layer} | {name} | {value}')
inventory.write_text('\n'.join(header) + '\n')
print(header[4])
