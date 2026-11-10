"""Remove the verified S5 launch screenshots and generated script bytecode."""
from pathlib import Path
import hashlib
import json
import subprocess

root = Path.cwd().resolve(strict=True)
local = root / 'build/slice5'
evidence = root / 'docs/projects/evidence/slice5'
fixture_cache = Path('/home/shmellyorc/VM/project-slice5/fixture/__pycache__')
host_state = {
    'removed': [],
    'retained': [str(local), str(root / 'build/slice5-start'), str(evidence),
                 str(root / 'docs/projects/evidence/slice5-source.patch'),
                 *[str(root / 'build' / ('llvm23-' + name)) for name in ('release', 'debug', 'strict')],
                 '/home/shmellyorc/VM/project-slice5'],
    'test_fixture_cleanup': 'CTest subprocesses and VS Code exited; their owned temporary workspaces self-cleaned.',
}
for profile in ('release', 'debug', 'strict'):
    for suffix in ('ctest.log', 'ctest.xml', 'binaries.sha256'):
        assert (evidence / 'linux' / f'{profile}-{suffix}').is_file()
assert (evidence / 'editor/persistence-host.json').is_file()
assert json.loads((evidence / 'windows-fixture/rerun.json').read_text())['status'] == 'PASS'
assert json.loads((evidence / 'windows-cleanup-results.json').read_text())['status'] == 'PASS'
subprocess.run(['git', 'apply', '--reverse', '--check', str(root / 'docs/projects/evidence/slice5-source.patch')], check=True)
paths = [local / name for name in ('vm-before.png', 'vm-native-command.png', 'vm-final-run-before.png',
                                    'vm-final-command.png', 'vm-fixture-command.png', 'vm-cleanup-command.png')]
paths.append(fixture_cache / 'rerun.cpython-314.pyc')
for path in paths:
    if not path.exists():
        continue
    assert path.is_file() and not path.is_symlink() and path.resolve(strict=True) == path, path
    assert path.stat().st_nlink == 1
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    path.unlink()
    host_state['removed'].append({'path': str(path), 'sha256': digest})
if fixture_cache.exists() and not any(fixture_cache.iterdir()):
    assert not fixture_cache.is_symlink() and fixture_cache.resolve(strict=True) == fixture_cache
    fixture_cache.rmdir()
    host_state['removed'].append({'path': str(fixture_cache)})
(evidence / 'host-cleanup-results.json').write_text(json.dumps(host_state, indent=2) + '\n')
