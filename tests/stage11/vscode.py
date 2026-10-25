"""Run the real VS Code globals fixture with isolated project/profile directories.
Usage: python3 tests/stage11/vscode.py /path/to/code /path/to/voidls
"""
from pathlib import Path
import json, os, subprocess, sys, tempfile
repo = Path(__file__).resolve().parents[2]
root = Path(tempfile.mkdtemp(prefix='VOID Stage11 VSCode '))
project = root / 'project'; project.mkdir()
(project / '.vscode').mkdir()
(project / 'main.void').write_text('use "./game.void" as game\n## Shared score.\nvar score = 0\nfunc main():\n    game.reward()\n    print(score, game.defeated)\n', encoding='utf-8')
(project / 'game.void').write_text('var defeated = 0\nprivate var secret = 7\nfunc reward():\n    score += 10\n    defeated += 1\n', encoding='utf-8')
server = str(Path(sys.argv[2]).resolve())
(project / '.vscode/settings.json').write_text(json.dumps({'void.server.path':server,'void.project.entry':'main.void'}))
env=dict(os.environ, VOID_TEST_ROOT=str(project), VOIDLS_PATH=server)
args=[sys.argv[1], str(project), '--user-data-dir='+str(root/'profile'), '--extensions-dir='+str(root/'extensions'), '--disable-extensions', '--disable-workspace-trust', '--no-sandbox', '--disable-gpu-sandbox', '--disable-gpu', '--skip-welcome', '--skip-release-notes', '--disable-updates', '--extensionDevelopmentPath=' + str(repo / 'editors/vscode'), '--extensionTestsPath=' + str(repo / 'tests/stage11/vscode.cjs')]
print('Real VS Code fixture:',root, flush=True)
raise SystemExit(subprocess.run(args, env=env, timeout=120).returncode)
