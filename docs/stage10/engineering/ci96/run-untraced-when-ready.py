"""Reuse the established untraced profile only after the current runner finishes.
No concurrent duplicate build/test, flag weakening, or automatic compiler retry.
"""
from pathlib import Path
import json,subprocess,time,shutil
root=Path(__file__).resolve().parents[4];base=root/'docs/stage10/engineering/ci96'
while True:
    try:records=json.loads((base/'linux-commands.json').read_text())
    except (FileNotFoundError,json.JSONDecodeError):records=[]
    if records:
        last=records[-1]
        if last['returncode'] or last['label']=='san-tests':break
    time.sleep(10)
if not last['returncode']:
    print('Current runner completed strict acceptance without retry.',flush=True)
    raise SystemExit(0)
log=(base/last['log']).read_text(errors='replace')
if not last['label'].startswith('san-') or 'LeakSanitizer' not in log or 'ptrace' not in log:
    raise SystemExit('Current runner failed outside the known LSan launch context; inspect before retry.')
for name in ['LastTest.log','LastTestsFailed.log']:
    path=root/'build/step42-ci96-san/Testing/Temporary'/name
    if path.exists():shutil.copyfile(path,base/('san-traced-'+name))
mode='--tests-only' if last['label']=='san-tests' else '--san-only'
command=['python3',str(base/'run-linux-focused.py'),mode]
(base/'untraced-resume.json').write_text(json.dumps({'reason':'Known LSan/ptrace context; source and sanitizer flags unchanged','original_failure':last,'command':command,'concurrent_duplicate_work':False},indent=2)+'\n')
print('Resuming focused strict acceptance through the established untraced profile:',mode,flush=True)
raise SystemExit(subprocess.call(command,cwd=root))
