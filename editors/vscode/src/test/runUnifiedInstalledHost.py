"""Qualify actual installed VOID artifacts/profile; close Code before running.

No isolated profile or copied toolchain is used. The development-host flag loads
exactly the existing installed extension directory, rather than the source tree.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--evidence', type=Path, required=True)
parser.add_argument('--ninja', type=Path, required=True)
args = parser.parse_args()
evidence = args.evidence.resolve()
evidence.mkdir(parents=True, exist_ok=True)
extension = Path.home() / '.vscode/extensions/void-language.void-language-0.1.2'
test = Path(__file__).resolve().parents[2] / 'out/test/unifiedInstalledHost.js'
assert extension.is_dir() and test.is_file()


def editor(root, phase):
    report = evidence / ('installed-editor-' + phase + '.json')
    assert not report.exists(), ('Use a fresh evidence destination', report)
    command = ['code', '--verbose', str(root), '--new-window',
               '--extensionDevelopmentPath=' + str(extension),
               '--extensionTestsPath=' + str(test), '--disable-workspace-trust',
               '--disable-extensions', '--skip-welcome', '--skip-release-notes',
               '--skip-add-to-recently-opened']
    env = dict(os.environ, VOID_DX_EVIDENCE=str(evidence), VOID_DX_PHASE=phase)
    with (evidence / ('editor-' + phase + '.log')).open('w') as log:
        result = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=240)
    assert result.returncode == 0 and report.is_file(), (phase, result.returncode, 'see editor log')
    assert json.loads(report.read_text())['status'] == 'PASS'
    print('actual installed editor:', phase, 'PASS', flush=True)


with tempfile.TemporaryDirectory(prefix='editor-project-', dir=evidence) as temporary:
    project = Path(temporary)
    project.chmod(0o775)
    (project / 'src').mkdir()
    (project / 'src/app.void').write_text('func main():\n    print("before package")\n')
    (project / '.void').mkdir(mode=0o775)
    (project / '.void').chmod(0o775)
    editor(project, 'live')
    editor(project, 'reopen')
editor(args.ninja.resolve(strict=True), 'ninja')
