from pathlib import Path
import json, subprocess
share=Path(r'\\vboxsvr\VM\slice7-final-integrated')
record=json.loads((share/'Debug-result.json').read_text())
assert record['stage']=='build' and record['profile']=='Debug'
scratch=Path(record['commands'][0]['argv'][1])
assert scratch.parent==Path.home()/'VS7I2' and scratch.name.startswith('Debug-')
subprocess.run(['icacls.exe',scratch,'/remove:g','*S-1-3-4'],check=True)
subprocess.run(['icacls.exe',scratch],check=True)
